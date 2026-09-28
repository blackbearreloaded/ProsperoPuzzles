// ProsperoPuzzles - Application entry point.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Bring-up diagnostics (M1/M2): presents an animated test pattern with a live
// button strip, plays a cue for every button press, keeps a persisted boot
// counter, generates each Tatham puzzle once, and logs lifecycle markers,
// frame pacing and audio health as the hardware runs' oracles.

#include "audio/cues.hpp"
#include "audio/mixer.hpp"
#include "core/frame_stats.hpp"
#include "core/input.hpp"
#include "core/save_file.hpp"
#include "games/sgt/sgt_catalog.hpp"
#include "games/sgt/sgt_session.hpp"
#include "gfx/gl_program.hpp"
#include "platform/ps5/audio_out.hpp"
#include "platform/ps5/display_egl.hpp"
#include "platform/ps5/pad.hpp"
#include "platform/ps5/system.hpp"

#include <GL/glcorearb.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

extern "C" void ppz_heap_stats(std::size_t *live_bytes, std::size_t *peak_bytes,
                               std::size_t *blocks, std::size_t *failures);

namespace
{

constexpr const char *kVertexShader = R"(
layout(location = 0) uniform vec2 u_resolution;
layout(location = 1) uniform vec4 u_rect;
out vec2 v_local;
out vec2 v_half;
const vec2 kCorners[6] = vec2[6](vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
                                 vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0));
void main()
{
    vec2 corner = kCorners[gl_VertexID];
    vec2 pixel = u_rect.xy + corner * u_rect.zw;
    v_half = 0.5 * u_rect.zw;
    v_local = (corner - 0.5) * u_rect.zw;
    vec2 ndc = pixel / u_resolution * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
}
)";

constexpr const char *kFragmentShader = R"(
layout(location = 2) uniform vec4 u_color;
layout(location = 3) uniform float u_radius;
in vec2 v_local;
in vec2 v_half;
out vec4 frag_color;
void main()
{
    vec2 q = abs(v_local) - v_half + vec2(u_radius);
    float distance = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - u_radius;
    float coverage = clamp(0.5 - distance, 0.0, 1.0);
    frag_color = vec4(u_color.rgb, u_color.a * coverage);
}
)";

// Test pattern for the first hardware runs: a card, an orbiting tile and a
// one-second sweep bar, so a screenshot proves frames are advancing.
class TestPattern
{
  public:
    bool init()
    {
        program_ = ppz::gfx::build_program("test-pattern", kVertexShader, kFragmentShader);
        if (program_ == 0)
            return false;
        glGenVertexArrays(1, &vao_);
        return true;
    }

    // held: logical action bits; each lights one cell of the button strip.
    void draw(double seconds, int width, int height, std::uint32_t held)
    {
        const float scale = static_cast<float>(height) / 1080.0f;
        glViewport(0, 0, width, height);
        glClearColor(0.03f, 0.05f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(program_);
        glBindVertexArray(vao_);
        glUniform2f(0, static_cast<float>(width), static_cast<float>(height));

        rect(560, 240, 800, 600, 36, 0.93f, 0.91f, 0.86f, 1.0f, scale);
        const double angle = seconds * 1.4;
        const float x = 960.0f + 230.0f * static_cast<float>(std::cos(angle)) - 70.0f;
        const float y = 520.0f + 150.0f * static_cast<float>(std::sin(angle)) - 70.0f;
        rect(x, y, 140, 140, 28, 0.96f, 0.45f, 0.26f, 1.0f, scale);
        const float sweep = static_cast<float>(seconds - std::floor(seconds));
        rect(640, 760, 640, 24, 12, 0.78f, 0.76f, 0.72f, 1.0f, scale);
        rect(640, 760, 24.0f + 616.0f * sweep, 24, 12, 0.18f, 0.55f, 0.95f, 1.0f, scale);
        // Button strip: one cell per logical action, lit while held.
        const int count = static_cast<int>(ppz::Action::count);
        for (int index = 0; index < count; ++index)
        {
            const bool lit = (held & (1u << static_cast<unsigned>(index))) != 0;
            const float cell_x = 600.0f + static_cast<float>(index) * 45.0f;
            rect(cell_x, 900, 36, 36, 8, lit ? 0.35f : 0.18f, lit ? 0.85f : 0.22f,
                 lit ? 0.45f : 0.30f, 1.0f, scale);
        }
    }

  private:
    void rect(float x, float y, float w, float h, float radius, float r, float g, float b, float a,
              float scale)
    {
        glUniform4f(1, x * scale, y * scale, w * scale, h * scale);
        glUniform4f(2, r, g, b, a);
        glUniform1f(3, radius * scale);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    GLuint program_ = 0;
    GLuint vao_ = 0;
};

void log_heap(std::uint64_t frames)
{
    std::size_t live = 0;
    std::size_t peak = 0;
    std::size_t blocks = 0;
    std::size_t failures = 0;
    ppz_heap_stats(&live, &peak, &blocks, &failures);
    ppz::sys::log("[PPZ] heap frames=%llu live=%zu peak=%zu blocks=%zu failures=%zu",
                  static_cast<unsigned long long>(frames), live, peak, blocks, failures);
}

constexpr const char *kActionNames[] = {
    "up",        "down",      "left",      "right",     "confirm", "back",  "north", "west",
    "page_prev", "page_next", "jump_prev", "jump_next", "menu",    "touch", "l3",    "r3"};
static_assert(sizeof(kActionNames) / sizeof(kActionNames[0]) ==
              static_cast<std::size_t>(ppz::Action::count));

ppz::audio::Cue cue_for(ppz::Action action)
{
    using ppz::Action;
    using ppz::audio::Cue;
    switch (action)
    {
    case Action::confirm:
        return Cue::ui_select;
    case Action::back:
        return Cue::ui_back;
    case Action::north:
        return Cue::ui_favorite_on;
    case Action::west:
        return Cue::place;
    case Action::page_prev:
    case Action::page_next:
    case Action::jump_prev:
    case Action::jump_next:
        return Cue::ui_tab;
    case Action::menu:
        return Cue::ui_pause_open;
    case Action::touch:
        return Cue::complete;
    default:
        return Cue::ui_focus;
    }
}

// Persisted boot counter: the hardware run's storage oracle.
unsigned boot_count(const std::string &root)
{
    const std::string path = root + "/boot.bin";
    std::string data;
    unsigned count = 0;
    if (ppz::save::read_file(path, &data))
    {
        const auto decoded = ppz::save::decode(ppz::save::Kind::stats, data);
        if (decoded.ok && decoded.payload.size() == 4)
        {
            for (int i = 3; i >= 0; --i)
                count = (count << 8) |
                        static_cast<unsigned char>(decoded.payload[static_cast<std::size_t>(i)]);
        }
        else
        {
            ppz::sys::log("[PPZ] boot counter unreadable: %s", decoded.error.c_str());
        }
    }
    ++count;
    std::string payload(4, '\0');
    for (int i = 0; i < 4; ++i)
        payload[static_cast<std::size_t>(i)] = static_cast<char>((count >> (8 * i)) & 0xff);
    const std::string error =
        ppz::save::write_atomic(path, ppz::save::encode(ppz::save::Kind::stats, 1, payload));
    ppz::sys::log("[PPZ] boot count=%u saved=%s", count, error.empty() ? "ok" : error.c_str());
    return count;
}

} // namespace

int main()
{
    using namespace ppz;
    sys::log("[PPZ] entry");

    const std::string data_root = "/download0/prosperopuzzles";
    sys::log("[PPZ] storage dir=%d", save::ensure_directory(data_root) ? 1 : 0);
    boot_count(data_root);

    ps5::Display display;
    if (!display.open())
    {
        sys::log("[PPZ] fatal: display open failed");
        sys::park();
    }
    TestPattern pattern;
    if (!pattern.init())
    {
        sys::log("[PPZ] fatal: test pattern init failed");
        sys::park();
    }

    ps5::Pad pad;
    pad.open();
    InputTracker tracker;
    audio::Mixer mixer;
    ps5::AudioOut audio_out;
    audio_out.start(mixer);
    audio::SoundBank sounds;
    const auto bank = sounds.load("/app0/assets/audio/sfx");
    sys::log("[PPZ] sounds files=%d rejected=%d", bank.files, bank.rejected);
    for (const std::string &error : bank.errors)
        sys::log("[PPZ] sound rejected %s", error.c_str());

    const std::int64_t start = sys::monotonic_us();
    std::int64_t previous = start;
    std::uint64_t frames = 0;
    FrameStats stats;
    const auto games = sgt::catalog();
    std::size_t next_game = 0;
    PadSample samples[64];
    for (;;)
    {
        const std::int64_t now = sys::monotonic_us();
        const std::size_t count = pad.read(samples);
        const InputFrame input = tracker.update(std::span<const PadSample>(samples, count),
                                                static_cast<std::uint64_t>(now));
        for (std::size_t index = 0; index < static_cast<std::size_t>(Action::count); ++index)
        {
            const auto action = static_cast<Action>(index);
            if (input.is_pressed(action))
            {
                sys::log("[PPZ] input pressed=%s", kActionNames[index]);
                sounds.play(mixer, cue_for(action));
            }
        }
        if (input.focus_lost)
            sys::log("[PPZ] input focus lost connected=%d", input.connected ? 1 : 0);
        pattern.draw(static_cast<double>(now - start) / 1e6, display.width(), display.height(),
                     input.held);
        if (!display.swap())
        {
            sys::log("[PPZ] fatal: swap failed frame=%llu error=%s",
                     static_cast<unsigned long long>(frames),
                     ps5::egl_error_name(display.last_error()));
            sys::park();
        }
        ++frames;
        const std::int64_t presented = sys::monotonic_us();
        if (frames == 1)
        {
            sys::log("[PPZ] first-swap ok");
            const bool hidden = sys::hide_splash_screen();
            sys::log("[PPZ] ready splash_hidden=%d", hidden ? 1 : 0);
            log_heap(frames);
        }
        else
        {
            stats.add(static_cast<double>(presented - previous) / 1000.0);
        }
        previous = presented;
        // Tatham self-test: from frame 120, generate one puzzle per frame and
        // log its timing, proving the collection runs on the console. The
        // generation stalls these frames; they are excluded from pacing.
        if (frames >= 120 && next_game < games.size())
        {
            const std::int64_t begin = sys::monotonic_us();
            sgt::Session session(games[next_game]);
            session.new_game();
            int width = 0;
            int height = 0;
            session.resize(1600, 1000, &width, &height);
            sys::log("[PPZ] sgt %s gen_ms=%.1f canvas=%dx%d status=%d", games[next_game].id,
                     static_cast<double>(sys::monotonic_us() - begin) / 1000.0, width, height,
                     session.status());
            if (++next_game == games.size())
                sys::log("[PPZ] sgt self-test done games=%zu", games.size());
            previous = sys::monotonic_us();
        }
        if (stats.count() == 600)
        {
            char summary[160];
            stats.format(summary, sizeof(summary));
            sys::log("[PPZ] %s", summary);
            sys::log("[PPZ] audio grains=%llu errors=%llu voices=%d",
                     static_cast<unsigned long long>(audio_out.grains()),
                     static_cast<unsigned long long>(audio_out.errors()), mixer.active_voices());
            stats.reset();
            if (frames % 3600 < 600)
                log_heap(frames);
        }
    }
}
