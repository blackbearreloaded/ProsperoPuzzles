// ProsperoPuzzles - Application entry point.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Opens the display, controller and audio, then runs the shell (library and
// game screens) every frame. Lifecycle markers, frame pacing and audio health
// are logged for hardware runs.

#include "app/shell.hpp"
#include "audio/cues.hpp"
#include "audio/mixer.hpp"
#include "core/frame_stats.hpp"
#include "core/input.hpp"
#include "core/save_file.hpp"
#include "gfx/draw_list.hpp"
#include "gfx/font.hpp"
#include "gfx/gl_batch.hpp"
#include "platform/ps5/audio_out.hpp"
#include "platform/ps5/display_egl.hpp"
#include "platform/ps5/pad.hpp"
#include "platform/ps5/system.hpp"
#include "ui/theme.hpp"

#include <GL/glcorearb.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

extern "C" void ppz_heap_stats(std::size_t *live_bytes, std::size_t *peak_bytes,
                               std::size_t *blocks, std::size_t *failures);

namespace
{

constexpr const char *kAssets = "/app0/assets";
constexpr const char *kDataRoot = "/download0/prosperopuzzles";

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

    app::Shell shell(batch, fonts, viewport.scale, kDataRoot);

    std::int64_t previous = sys::monotonic_us();
    std::uint64_t frames = 0;
    FrameStats stats;
    PadSample samples[64];
    gfx::DrawList list;
    for (;;)
    {
        const std::int64_t now = sys::monotonic_us();
        const float dt = frames == 0 ? 1.0f / 60.0f : static_cast<float>(now - previous) / 1e6f;
        const std::size_t count = pad.read(samples);
        const InputFrame input = tracker.update(std::span<const PadSample>(samples, count),
                                                static_cast<std::uint64_t>(now));
        if (input.focus_lost)
            sys::log("[PPZ] input focus lost connected=%d", input.connected ? 1 : 0);
        if (input.pressed != 0 || input.nav != Direction::none)
            sys::log("[PPZ] input pressed=0x%x held=0x%x nav=%d repeat=%d samples=%zu raw=0x%x",
                     input.pressed, input.held, static_cast<int>(input.nav),
                     input.nav_repeat ? 1 : 0, count, count > 0 ? samples[count - 1].buttons : 0u);

        shell.update(input, dt > 0.05f ? 0.05f : dt);
        const std::string game = shell.active_game();
        for (audio::Cue cue : shell.take_cues())
            sounds.play(mixer, cue, game);

        list.clear();
        shell.draw(list);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
        if (stats.count() == 600)
        {
            char summary[160];
            stats.format(summary, sizeof(summary));
            sys::log("[PPZ] %s draws=%zu", summary, batch.last_draw_calls());
            sys::log("[PPZ] audio grains=%llu errors=%llu voices=%d",
                     static_cast<unsigned long long>(audio_out.grains()),
                     static_cast<unsigned long long>(audio_out.errors()), mixer.active_voices());
            stats.reset();
            if (frames % 3600 < 600)
                log_heap(frames);
        }
    }
}
