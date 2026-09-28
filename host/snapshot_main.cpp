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
#include "core/library.hpp"
#include "games/registry.hpp"
#include "games/sgt/sgt_catalog.hpp"
#include "games/sgt/sgt_scene.hpp"
#include "ui/gallery.hpp"
#include "ui/library_scene.hpp"
#include "ui/theme.hpp"

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
    ppz::ui::LibraryScene scene(library);
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
        game.start();
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
    return ok ? 0 : 1;
}
