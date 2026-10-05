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
#include "audio/music.hpp"
#include "core/version.hpp"
#include "core/frame_stats.hpp"
#include "core/input.hpp"
#include "core/migrate.hpp"
#include "core/save_file.hpp"
#include "core/settings.hpp"
#include "gfx/draw_list.hpp"
#include "gfx/font.hpp"
#include "gfx/gl_batch.hpp"
#include "platform/ps5/audio_out.hpp"
#include "platform/ps5/display_egl.hpp"
#include "platform/ps5/pad.hpp"
#include "platform/ps5/storage.hpp"
#include "platform/ps5/system.hpp"
#include "platform/ps5/updater_ps5.hpp"
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

// Opens at *resolution (an index into Settings::kResolutions), falling back
// to 1080p; *resolution reports the mode that opened.
bool open_display(ppz::ps5::Display &display, int *resolution)
{
    using ppz::Settings;
    const Settings::Resolution &mode = Settings::kResolutions[*resolution];
    if (display.open(mode.width, mode.height))
    {
        ppz::sys::log("[PPZ] display mode %s %dx%d", mode.label, display.width(), display.height());
        return true;
    }
    if (*resolution == 0)
        return false;
    ppz::sys::log("[PPZ] display mode %s failed, using 1080p", mode.label);
    *resolution = 0;
    return display.open(Settings::kResolutions[0].width, Settings::kResolutions[0].height);
}

// Changes the display mode: every GL object dies with the context, so the
// shell and the batch release theirs first and rebuild them afterwards.
bool restart_display(ppz::ps5::Display &display, ppz::gfx::GlBatch &batch,
                     const ppz::ui::Fonts &fonts, ppz::app::Shell &shell, int *resolution)
{
    using namespace ppz;
    const std::int64_t start = sys::monotonic_us();
    shell.release_gpu();
    batch.release();
    const GLuint font_textures[] = {fonts.regular_texture, fonts.semibold_texture};
    glDeleteTextures(2, font_textures);
    display.close();
    if (!open_display(display, resolution) || !batch.init())
        return false;
    // A fresh context numbers textures from 1 in creation order, so the font
    // atlases come back under the names every scene already holds.
    const std::uint32_t regular = batch.create_font_texture(*fonts.regular);
    const std::uint32_t semibold = batch.create_font_texture(*fonts.semibold);
    if (regular != fonts.regular_texture || semibold != fonts.semibold_texture)
    {
        sys::log("[PPZ] font textures renumbered %u/%u -> %u/%u", fonts.regular_texture,
                 fonts.semibold_texture, regular, semibold);
        return false;
    }
    shell.restore_gpu(gfx::fit_viewport(display.width(), display.height()).scale);
    sys::log("[PPZ] display restart %dx%d in %lld ms", display.width(), display.height(),
             static_cast<long long>((sys::monotonic_us() - start) / 1000));
    return true;
}

bool load_font(const std::string &assets, const char *name, ppz::gfx::Font *font)
{
    std::string data;
    const std::string path = assets + "/fonts/" + name;
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
    // Filesystem access first, while the process has one thread: it decides
    // where the app's own files and its data are (platform/ps5/storage.hpp).
    const ps5::Storage &where = ps5::prepare_storage();
    const std::string assets = where.app_dir + "/assets";
    const std::string &data_root = where.data_root;
    const bool directory = save::ensure_directory(data_root);
    sys::open_log(data_root.c_str());
    sys::log("[PPZ] storage access=%d route=%s app=%s data=%s dir=%d", where.access, where.route,
             where.app_dir.c_str(), data_root.c_str(), directory ? 1 : 0);
    if (!where.previous_root.empty())
    {
        // The first start with filesystem access brings the sandbox's saves along.
        const save::Migration moved = save::migrate(where.previous_root, data_root);
        if (moved.ran)
            sys::log("[PPZ] storage migrated from=%s copied=%d failed=%d",
                     where.previous_root.c_str(), moved.copied, moved.failed);
    }

    // The display opens at the saved resolution (1080p if that fails).
    const Settings saved = app::Shell::load_settings(data_root);
    int resolution = ps5::Display::supports_display_modes() ? saved.resolution : 0;
    ps5::Display display;
    if (!open_display(display, &resolution))
    {
        sys::log("[PPZ] fatal: display open failed");
        sys::park();
    }
    gfx::Font regular;
    gfx::Font semibold;
    gfx::GlBatch batch;
    if (!load_font(assets, "inter-regular.ppzfont", &regular) ||
        !load_font(assets, "inter-semibold.ppzfont", &semibold) || !batch.init())
    {
        sys::log("[PPZ] fatal: renderer init failed");
        sys::park();
    }
    const ui::Fonts fonts{&regular, &semibold, batch.create_font_texture(regular),
                          batch.create_font_texture(semibold)};
    gfx::Viewport viewport = gfx::fit_viewport(display.width(), display.height());

    ps5::Pad pad;
    pad.open();
    InputTracker tracker;
    audio::Mixer mixer;
    // The music stream attaches to the mixer before the audio thread starts.
    // The playlist order is shuffled from the launch time, so it differs
    // every time the app opens.
    audio::MusicPlayer music;
    const int tracks =
        music.init(mixer, assets + "/audio/music", static_cast<std::uint64_t>(sys::monotonic_us()));
    sys::log("[PPZ] music songs=%d", tracks);
    ps5::AudioOut audio_out;
    audio_out.start(mixer);
    audio::SoundBank sounds;
    const auto bank = sounds.load(assets + "/audio/sfx");
    sys::log("[PPZ] sounds files=%d rejected=%d", bank.files, bank.rejected);
    for (const std::string &error : bank.errors)
        sys::log("[PPZ] sound rejected %s", error.c_str());

    app::Shell shell(batch, fonts, viewport.scale, data_root);
    shell.set_applied_resolution(resolution);
    const std::string &version = where.version;
    shell.set_version(version);
    sys::log("[PPZ] version %s", version.empty() ? "unknown" : version.c_str());
    // Once per launch: is a newer release listed? The shell offers it.
    ps5::ConsoleUpdater updater;
    updater.start();
    shell.set_updater(&updater);

    std::int64_t previous = sys::monotonic_us();
    std::uint64_t frames = 0;
    FrameStats stats;
    // The FPS overlay averages over half a second so the number is readable.
    double fps_seconds = 0.0;
    int fps_frames = 0;
    double fps_shown = 0.0;
    PadSample samples[64];
    gfx::DrawList list;
    std::int64_t last_frame_start = sys::monotonic_us();
    for (;;)
    {
        const std::int64_t now = sys::monotonic_us();
        // Animation time is start-to-start (one full frame), not the gap
        // between the previous swap returning and this frame beginning.
        const float dt =
            frames == 0 ? 1.0f / 60.0f : static_cast<float>(now - last_frame_start) / 1e6f;
        last_frame_start = now;
        const std::size_t count = pad.read(samples);
        InputFrame input = tracker.update(std::span<const PadSample>(samples, count),
                                          static_cast<std::uint64_t>(now));
#ifdef PPZ_DEV_UPDATE_AUTO_ACCEPT
        // Development only, for scripted console runs: the offer is accepted
        // that many seconds after it appeared, as if Cross had been pressed.
        {
            static std::int64_t offered = 0;
            if (!shell.update_dialog_open())
                offered = 0;
            else if (offered == 0)
                offered = now;
            else if (offered > 0 && now - offered > PPZ_DEV_UPDATE_AUTO_ACCEPT * 1000000LL)
            {
                sys::log("[PPZ] update offer accepted by the development timer");
                input.pressed |= action_bit(Action::confirm);
                offered = -1;
            }
        }
#endif
        if (input.focus_lost)
            sys::log("[PPZ] input focus lost connected=%d", input.connected ? 1 : 0);
        if (input.pressed != 0 || input.nav != Direction::none)
            sys::log("[PPZ] input pressed=0x%x held=0x%x nav=%d repeat=%d samples=%zu raw=0x%x",
                     input.pressed, input.held, static_cast<int>(input.nav),
                     input.nav_repeat ? 1 : 0, count, count > 0 ? samples[count - 1].buttons : 0u);

        shell.update(input, dt > 0.05f ? 0.05f : dt);
        if (shell.take_settings_changed())
        {
            const Settings &settings = shell.settings();
            mixer.set_bus_gain(audio::Bus::music, Settings::gain(settings.music_volume));
            mixer.set_bus_gain(audio::Bus::sfx, Settings::gain(settings.sfx_volume));
            mixer.set_bus_gain(audio::Bus::ui, Settings::gain(settings.ui_volume));
            InputSettings input_settings = tracker.settings();
            input_settings.swap_confirm = settings.swap_confirm;
            tracker.set_settings(input_settings);
        }
        const std::string game = shell.active_game();
        for (audio::Cue cue : shell.take_cues())
        {
            sounds.play(mixer, cue, game);
            if (cue == audio::Cue::complete)
                music.duck();
        }
        music.pump(dt > 0.05f ? 0.05f : dt);
        if (shell.take_quit())
        {
            // The update is staged: the helper replaces the files once the app is gone.
            sys::log("[PPZ] closing for the update");
            sys::exit_app();
        }
        if (shell.take_display_mode_changed())
        {
            resolution = shell.settings().resolution;
            if (!restart_display(display, batch, fonts, shell, &resolution))
            {
                sys::log("[PPZ] fatal: display restart failed");
                sys::park();
            }
            viewport = gfx::fit_viewport(display.width(), display.height());
            shell.set_applied_resolution(resolution);
            // The restart takes a moment; do not animate or count FPS across it.
            last_frame_start = sys::monotonic_us();
            previous = last_frame_start;
            fps_seconds = 0.0;
            fps_frames = 0;
        }

        list.clear();
        shell.draw(list);
        fps_seconds += static_cast<double>(dt);
        ++fps_frames;
        if (fps_seconds >= 0.5)
        {
            fps_shown = fps_frames / fps_seconds;
            fps_seconds = 0.0;
            fps_frames = 0;
        }
        if (shell.settings().show_fps && fps_shown > 0.0)
        {
            char fps[32];
            std::snprintf(fps, sizeof(fps), "%.0f FPS", fps_shown);
            list.text(regular, fonts.regular_texture, fps, 1900, 30, 20,
                      gfx::Color::rgb(0xffffff, 0.7f), gfx::Align::right);
        }
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
            sys::log("[PPZ] audio grains=%llu errors=%llu voices=%d music=%s underruns=%llu",
                     static_cast<unsigned long long>(audio_out.grains()),
                     static_cast<unsigned long long>(audio_out.errors()), mixer.active_voices(),
                     music.current().c_str(),
                     static_cast<unsigned long long>(mixer.stream_underruns()));
            stats.reset();
            if (frames % 3600 < 600)
                log_heap(frames);
        }
    }
}
