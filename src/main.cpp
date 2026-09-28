// ProsperoPuzzles - Application entry point.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Bring-up diagnostics (M1-M3): draws the UI gallery with the real 2D
// renderer and a live button strip, plays a cue for every button press,
// keeps a persisted boot counter, generates each Tatham puzzle once, and logs
// lifecycle markers, frame pacing and audio health as hardware-run oracles.

#include "audio/cues.hpp"
#include "audio/mixer.hpp"
#include "core/frame_stats.hpp"
#include "core/input.hpp"
#include "core/save_file.hpp"
#include "games/sgt/sgt_catalog.hpp"
#include "games/sgt/sgt_session.hpp"
#include "gfx/draw_list.hpp"
#include "gfx/font.hpp"
#include "gfx/gl_batch.hpp"
#include "platform/ps5/audio_out.hpp"
#include "platform/ps5/display_egl.hpp"
#include "platform/ps5/pad.hpp"
#include "platform/ps5/system.hpp"
#include "ui/gallery.hpp"
#include "ui/theme.hpp"

#include <GL/glcorearb.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>

extern "C" void ppz_heap_stats(std::size_t *live_bytes, std::size_t *peak_bytes,
                               std::size_t *blocks, std::size_t *failures);

namespace
{

constexpr const char *kAssets = "/app0/assets";
constexpr const char *kDataRoot = "/download0/prosperopuzzles";

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

// Persisted boot counter: the hardware run's storage oracle.
void count_boot(const std::string &root)
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
}

bool load_font(const char *name, ppz::gfx::Font *font)
{
    std::string data;
    const std::string path = std::string(kAssets) + "/fonts/" + name;
    if (!ppz::save::read_file(path, &data) || !font->load(data))
    {
        ppz::sys::log("[PPZ] font %s failed: %s", name, font->error().c_str());
        return false;
    }
    return true;
}

} // namespace

int main()
{
    using namespace ppz;
    sys::log("[PPZ] entry");
    sys::log("[PPZ] storage dir=%d", save::ensure_directory(kDataRoot) ? 1 : 0);
    count_boot(kDataRoot);

    ps5::Display display;
    if (!display.open())
    {
        sys::log("[PPZ] fatal: display open failed");
        sys::park();
    }
    gfx::Font regular;
    gfx::Font semibold;
    gfx::GlBatch batch;
    if (!load_font("inter-regular.ppzfont", &regular) ||
        !load_font("inter-semibold.ppzfont", &semibold) || !batch.init())
    {
        sys::log("[PPZ] fatal: renderer init failed");
        sys::park();
    }
    const ui::Fonts fonts{&regular, &semibold, batch.create_font_texture(regular),
                          batch.create_font_texture(semibold)};
    const gfx::Viewport viewport = gfx::fit_viewport(display.width(), display.height());

    ps5::Pad pad;
    pad.open();
    InputTracker tracker;
    audio::Mixer mixer;
    ps5::AudioOut audio_out;
    audio_out.start(mixer);
    audio::SoundBank sounds;
    const auto bank = sounds.load(std::string(kAssets) + "/audio/sfx");
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
    gfx::DrawList list;
    ui::GalleryState gallery;
    char status[160] = "";
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
        if (input.nav == Direction::left)
            gallery.focused_card = (gallery.focused_card + 4) % 5;
        else if (input.nav == Direction::right)
            gallery.focused_card = (gallery.focused_card + 1) % 5;
        if (input.focus_lost)
            sys::log("[PPZ] input focus lost connected=%d", input.connected ? 1 : 0);

        gallery.seconds = static_cast<double>(now - start) / 1e6;
        gallery.held_actions = input.held;
        gallery.status = status;
        list.clear();
        ui::draw_gallery(list, fonts, gallery);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        batch.draw(list, viewport, display.width(), display.height());
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
            sys::log("[PPZ] first-swap ok instances=%zu draws=%zu", list.instances().size(),
                     batch.last_draw_calls());
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
        // log its timing. These frames stall and are excluded from pacing.
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
            std::snprintf(
                status, sizeof(status),
                "frame %.2f ms  \xC2\xB7  max %.2f ms  \xC2\xB7  %zu instances / %zu draws  "
                "\xC2\xB7  audio grains %llu",
                stats.mean_ms(), stats.max_ms(), list.instances().size(), batch.last_draw_calls(),
                static_cast<unsigned long long>(audio_out.grains()));
            stats.reset();
            if (frames % 3600 < 600)
                log_heap(frames);
        }
    }
}
