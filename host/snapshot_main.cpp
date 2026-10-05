// ProsperoPuzzles - Headless host renderer: draws UI scenes to PNG files.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// usage: ppz_snapshots <assets dir> <output dir> [width height]
// Renders with the same DrawList/GlBatch/font code as the console, through
// Mesa's surfaceless EGL (llvmpipe), into an offscreen framebuffer.

#include "core/save_file.hpp"
#include "gfx/draw_list.hpp"
#include "gfx/font.hpp"
#include "gfx/gl_batch.hpp"
#include "gfx/gl_program.hpp"
#include "app/thumbnails.hpp"
#include "art.hpp"
#include "core/version.hpp"
#include "games/kit/puzzle_scene.hpp"
#include "games/native.hpp"
#include "core/library.hpp"
#include "ui/confetti.hpp"
#include "ui/howto_card.hpp"
#include "games/registry.hpp"
#include "games/sgt/sgt_catalog.hpp"
#include "games/sgt/sgt_scene.hpp"
#include "games/g2048/g2048_scene.hpp"
#include "games/tenfold/tenfold_scene.hpp"
#include "ui/gallery.hpp"
#include "ui/library_scene.hpp"
#include "ui/about_scene.hpp"
#include "ui/about_scene.hpp"
#include "ui/settings_scene.hpp"
#include "ui/theme.hpp"
#include "ui/update_dialog.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glcorearb.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb/stb_image_write.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{

bool open_context()
{
    auto get_platform_display = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
        eglGetProcAddress("eglGetPlatformDisplayEXT"));
    EGLDisplay display =
        get_platform_display != nullptr
            ? get_platform_display(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr)
            : eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major = 0;
    EGLint minor = 0;
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, &major, &minor) ||
        !eglBindAPI(EGL_OPENGL_API))
        return false;
    const EGLint context_attributes[] = {EGL_CONTEXT_MAJOR_VERSION,
                                         4,
                                         EGL_CONTEXT_MINOR_VERSION,
                                         5,
                                         EGL_CONTEXT_OPENGL_PROFILE_MASK,
                                         EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                                         EGL_NONE};
    EGLContext context =
        eglCreateContext(display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, context_attributes);
    return context != EGL_NO_CONTEXT &&
           eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context);
}

// An updater that answers what a snapshot needs.
class ScriptedUpdater final : public ppz::Updater
{
  public:
    ppz::UpdateProgress progress;
    bool begins = true;

    bool take(ppz::UpdateOffer *) override
    {
        return false;
    }
    bool begin() override
    {
        return begins;
    }
    ppz::UpdateProgress poll() override
    {
        return progress;
    }
    void cancel() override
    {
    }
    bool apply() override
    {
        return true;
    }
    void finish() override
    {
    }
};

bool load_font(const std::string &path, ppz::gfx::Font *font)
{
    std::string data;
    if (!ppz::save::read_file(path, &data) || !font->load(data))
    {
        std::fprintf(stderr, "cannot load font %s\n", path.c_str());
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::fprintf(stderr, "usage: %s <assets dir> <output dir> [width height]\n", argv[0]);
        return 2;
    }
    const std::string assets = argv[1];
    const std::string output = argv[2];
    const int width = argc > 4 ? std::atoi(argv[3]) : 1920;
    const int height = argc > 4 ? std::atoi(argv[4]) : 1080;

    if (!open_context())
    {
        std::fprintf(stderr, "no surfaceless EGL OpenGL 4.5 context\n");
        return 1;
    }
    std::fprintf(stderr, "GL %s / %s\n", reinterpret_cast<const char *>(glGetString(GL_VERSION)),
                 reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    ppz::gfx::set_glsl_prefix("#version 450 core\n");

    ppz::gfx::Font regular;
    ppz::gfx::Font semibold;
    if (!load_font(assets + "/fonts/inter-regular.ppzfont", &regular) ||
        !load_font(assets + "/fonts/inter-semibold.ppzfont", &semibold))
        return 1;
    ppz::gfx::GlBatch batch;
    if (!batch.init())
        return 1;
    ppz::ui::Fonts fonts{&regular, &semibold, batch.create_font_texture(regular),
                         batch.create_font_texture(semibold)};

    GLuint framebuffer = 0;
    GLuint color = 0;
    glGenFramebuffers(1, &framebuffer);
    glGenRenderbuffers(1, &color);
    glBindRenderbuffer(GL_RENDERBUFFER, color);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        return 1;

    ppz::gfx::DrawList list;
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width * height * 4));
    stbi_flip_vertically_on_write(1);
    const auto write = [&](const char *name)
    {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        batch.draw(list, ppz::gfx::fit_viewport(width, height), width, height);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        const std::string path = output + "/" + name + ".png";
        const bool ok =
            stbi_write_png(path.c_str(), width, height, 4, pixels.data(), width * 4) != 0;
        std::fprintf(stderr, "wrote %s: %zu instances, %zu draw calls, GL error 0x%x\n",
                     path.c_str(), list.instances().size(), batch.last_draw_calls(), glGetError());
        return ok;
    };

    // Presentation art only (PPZ_ART=1): tools/render-art.sh turns these into sce_sys files.
    if (std::getenv("PPZ_ART") != nullptr)
    {
        ppz::app::Thumbnails thumbnails(batch, fonts, static_cast<float>(width) / 1920.0f);
        for (const auto &game : ppz::games::all())
            thumbnails.request(game.id, {});
        thumbnails.pump(64);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        return ppz::host::render_art(list, fonts, thumbnails, write) ? 0 : 1;
    }

    ppz::ui::GalleryState state;
    state.seconds = 1.25;
    state.held_actions = 0x0111; // up, confirm, jump_prev
    state.focused_card = 2;
    state.status = "host snapshot \xC2\xB7 llvmpipe";
    list.clear();
    ppz::ui::draw_gallery(list, fonts, state);
    bool ok = write("gallery");

    // Library: three favorites, one game in progress, focus moved onto a card.
    ppz::Library library(ppz::games::library_entries());
    library.toggle_favorite("net");
    library.toggle_favorite("lightup");
    library.toggle_favorite("g2048");
    library.set_in_progress("mines", true);
    library.set_completed("bridges", true);
    library.set_completed("crowns", true);
    ppz::ui::LibraryScene scene(library);
    scene.version = ppz::read_content_version(assets + "/../sce_sys/param.json");
    ppz::app::Thumbnails thumbnails(batch, fonts, 1.0f);
    for (const auto &game : ppz::games::all())
        thumbnails.request(game.id, {});
    thumbnails.pump(64);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    scene.set_thumbnails(&thumbnails);
    std::vector<ppz::audio::Cue> cues;
    ppz::InputFrame idle;
    ppz::InputFrame right;
    right.nav = ppz::Direction::right;
    ppz::InputFrame down;
    down.nav = ppz::Direction::down;
    scene.update(down, 0.016f, cues);
    scene.update(right, 0.016f, cues);
    for (int frame = 0; frame < 90; ++frame)
        scene.update(idle, 1.0f / 60.0f, cues);
    list.clear();
    scene.draw(list, fonts);
    ok = write("library") && ok;
    {
        // The details menu with a personal record line.
        ppz::ui::Menu details;
        details.open("Crowns",
                     {{"Play", 1}, {"Add to favorites", 3}, {"How to play", 4}, {"Close", 0}},
                     "Place one crown in every row, column and colour region, with no two "
                     "touching.",
                     "Best 7 \xC3\x97 7 1:35  \xC2\xB7  8 \xC3\x97 8 3:02  \xC2\xB7  Solved 6");
        for (int frame = 0; frame < 40; ++frame)
            details.update(idle, 1.0f / 60.0f, cues);
        list.clear();
        scene.draw(list, fonts);
        details.draw(list, fonts);
        ok = write("details-record") && ok;
    }
    {
        // Settings, focused on Resolution set to 4K.
        ppz::Settings settings;
        settings.resolution = 2;
        ppz::ui::SettingsScene settings_scene(settings);
        for (int row = 0; row < 6; ++row)
            settings_scene.update(down, 0.016f, cues);
        for (int frame = 0; frame < 60; ++frame)
            settings_scene.update(idle, 1.0f / 60.0f, cues);
        list.clear();
        settings_scene.draw(list, fonts, "ProsperoPuzzles  \xC2\xB7  Version 01.001.000");
        ok = write("settings") && ok;
        list.clear();
        ppz::ui::AboutScene().draw(list, fonts, "01.001.000");
        ok = write("about") && ok;
    }

    {
        // The update dialog over the library: the offer, the download, the
        // unpacking, the close, a failure, and a copy that can't update itself.
        ppz::InputFrame confirm;
        confirm.pressed = ppz::action_bit(ppz::Action::confirm);
        const ppz::UpdateOffer offer{true, "01.000.020", "01.000.020", 39167016};
        const auto shot = [&](ppz::ui::UpdateDialog &dialog, int frames, const char *name)
        {
            for (int frame = 0; frame < frames; ++frame)
                dialog.update(idle, 1.0f / 60.0f, cues);
            list.clear();
            scene.draw(list, fonts);
            dialog.draw(list, fonts);
            return write(name);
        };
        ScriptedUpdater updater;
        ppz::ui::UpdateDialog dialog;
        dialog.open(updater, offer, false);
        ok = shot(dialog, 18, "update-offer-rising") && ok;
        ok = shot(dialog, 72, "update-offer") && ok;
        dialog.update(confirm, 1.0f / 60.0f, cues);
        updater.progress.phase = ppz::UpdatePhase::starting;
        ok = shot(dialog, 40, "update-preparing") && ok;
        updater.progress = {
            ppz::UpdatePhase::downloading, 24641536, 39167016, "about 6 s left", {}};
        ok = shot(dialog, 90, "update-downloading") && ok;
        updater.progress = {ppz::UpdatePhase::unpacking, 31457280, 52428800, {}, {}};
        ok = shot(dialog, 90, "update-unpacking") && ok;
        updater.progress.phase = ppz::UpdatePhase::ready;
        ok = shot(dialog, 50, "update-closing") && ok;

        ScriptedUpdater broken;
        ppz::ui::UpdateDialog failed;
        failed.open(broken, offer, false);
        failed.update(confirm, 1.0f / 60.0f, cues);
        broken.progress.phase = ppz::UpdatePhase::failed;
        broken.progress.error = "The update helper couldn't be started. Is the payload loader "
                                "running?";
        ok = shot(failed, 60, "update-failed") && ok;

        ppz::ui::UpdateDialog notice;
        notice.open(updater, {false, "01.000.020", "01.000.020", 0}, false);
        ok = shot(notice, 90, "update-notice") && ok;
    }

    for (int step = 0; step < 4; ++step)
        scene.update(down, 0.016f, cues);
    for (int frame = 0; frame < 90; ++frame)
        scene.update(idle, 1.0f / 60.0f, cues);
    list.clear();
    scene.draw(list, fonts);
    ok = write("library-scrolled") && ok;

    ppz::InputFrame filter;
    filter.pressed = ppz::action_bit(ppz::Action::page_next);
    scene.update(filter, 0.016f, cues);
    for (int frame = 0; frame < 90; ++frame)
        scene.update(idle, 1.0f / 60.0f, cues);
    list.clear();
    scene.draw(list, fonts);
    ok = write("library-favorites") && ok;

    // A few Tatham puzzles on the play screen, after some cursor input.
    const char *puzzles[] = {"net",     "solo",     "mines", "loopy",
                             "pattern", "untangle", "map",   "bridges"};
    for (const char *id : puzzles)
    {
        const ppz::sgt::GameEntry *entry = ppz::sgt::find_game(id);
        ppz::sgt::SgtScene game(*entry, batch, fonts, 1.0f);
        game.start({}, {});
        ppz::InputFrame step;
        step.nav = ppz::Direction::right;
        game.update(step, 0.016f, cues);
        step.nav = ppz::Direction::down;
        game.update(step, 0.016f, cues);
        ppz::InputFrame select;
        select.pressed = ppz::action_bit(ppz::Action::confirm);
        game.update(select, 0.016f, cues);
        for (int frame = 0; frame < 60; ++frame)
            game.update(idle, 1.0f / 60.0f, cues);
        if (std::string(id) == "solo")
        {
            ppz::InputFrame keys;
            keys.pressed = ppz::action_bit(ppz::Action::north);
            game.update(keys, 0.016f, cues);
            for (int frame = 0; frame < 30; ++frame)
                game.update(idle, 1.0f / 60.0f, cues);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        list.clear();
        game.draw(list);
        ok = write((std::string("game-") + id).c_str()) && ok;
    }

    // Native puzzles: a fixed medium board after a few moves, and the pause menu.
    for (const ppz::games::NativeGame &native : ppz::games::native_games())
    {
        const std::string only = std::getenv("PPZ_NATIVE") ? std::getenv("PPZ_NATIVE") : "";
        if (!only.empty() && only != native.id)
            continue;
        auto scene = native.create(fonts);
        scene->start({}, {});
        ppz::kit::PuzzleScene *puzzle = scene->as_puzzle();
        if (puzzle != nullptr)
            puzzle->new_puzzle(20260928, 1);
        const ppz::Direction steps[] = {ppz::Direction::right, ppz::Direction::down,
                                        ppz::Direction::left, ppz::Direction::down};
        for (ppz::Direction d : steps)
        {
            ppz::InputFrame f;
            f.nav = d;
            scene->update(f, 0.016f, cues);
            ppz::InputFrame press;
            press.pressed = ppz::action_bit(ppz::Action::confirm);
            scene->update(press, 0.016f, cues);
            for (int frame = 0; frame < 20; ++frame)
                scene->update(idle, 1.0f / 60.0f, cues);
        }
        for (int frame = 0; frame < 60; ++frame)
            scene->update(idle, 1.0f / 60.0f, cues);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        list.clear();
        scene->draw(list);
        ok = write((std::string("native-") + native.id).c_str()) && ok;
    }

    // Every Tatham puzzle as it first appears (PPZ_SNAPSHOT_ALL=1; for skin review).
    if (std::getenv("PPZ_SNAPSHOT_ALL") != nullptr)
    {
        for (const ppz::sgt::GameEntry &entry : ppz::sgt::catalog())
        {
            ppz::sgt::SgtScene game(entry, batch, fonts, 1.0f);
            game.start({}, {});
            ppz::InputFrame step;
            step.nav = ppz::Direction::right;
            game.update(step, 0.016f, cues);
            for (int frame = 0; frame < 30; ++frame)
                game.update(idle, 1.0f / 60.0f, cues);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            list.clear();
            game.draw(list);
            ok = write((std::string("all-") + entry.id).c_str()) && ok;
        }
    }

    {
        ppz::g2048::G2048Scene g2048(fonts);
        g2048.start({}, {});
        for (auto d : {ppz::Direction::left, ppz::Direction::up, ppz::Direction::left,
                       ppz::Direction::up, ppz::Direction::right, ppz::Direction::up})
        {
            ppz::InputFrame f;
            f.nav = d;
            g2048.update(f, 0.016f, cues);
            for (int frame = 0; frame < 30; ++frame)
                g2048.update(idle, 1.0f / 60.0f, cues);
        }
        list.clear();
        g2048.draw(list);
        ok = write("game-g2048") && ok;
        ppz::ui::Confetti confetti;
        confetti.burst(ppz::gfx::Color::rgb(0xf2b134), 7);
        for (int frame = 0; frame < 32; ++frame)
            confetti.update(1.0f / 60.0f);
        confetti.draw(list);
        ok = write("game-confetti") && ok;
        for (const char *id : {"bridges", "undead"})
        {
            ppz::ui::HowToCard card;
            card.open(*ppz::games::find(id));
            for (int frame = 0; frame < 40; ++frame)
                card.animate(1.0f / 60.0f);
            list.clear();
            g2048.draw(list);
            card.draw(list, fonts);
            ok = write((std::string("howto-") + id).c_str()) && ok;
        }
    }
    {
        ppz::tenfold::TenfoldScene tenfold(fonts);
        tenfold.start({}, {});
        for (int frame = 0; frame < 60; ++frame)
            tenfold.update(idle, 1.0f / 60.0f, cues);
        ppz::InputFrame hint;
        hint.pressed = ppz::action_bit(ppz::Action::west);
        tenfold.update(hint, 0.016f, cues);
        ppz::InputFrame select;
        select.pressed = ppz::action_bit(ppz::Action::confirm);
        tenfold.update(select, 0.016f, cues);
        for (int frame = 0; frame < 20; ++frame)
            tenfold.update(idle, 1.0f / 60.0f, cues);
        list.clear();
        tenfold.draw(list);
        ok = write("game-tenfold") && ok;
        ppz::InputFrame menu;
        menu.pressed = ppz::action_bit(ppz::Action::menu);
        tenfold.update(menu, 0.016f, cues);
        for (int frame = 0; frame < 30; ++frame)
            tenfold.update(idle, 1.0f / 60.0f, cues);
        list.clear();
        tenfold.draw(list);
        ok = write("game-tenfold-paused") && ok;
    }
    return ok ? 0 : 1;
}
