// ProsperoPuzzles - Headless host run of the shell: frames of leaving a game.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// usage: ppz_transition <assets dir> <output dir> <data dir> [moves right]
// Opens a game from the library, plays a move, returns to the library through
// the pause menu and writes every frame from the exit onward as a PNG.

#include "app/shell.hpp"
#include "core/save_file.hpp"
#include "gfx/draw_list.hpp"
#include "gfx/font.hpp"
#include "gfx/gl_batch.hpp"
#include "gfx/gl_program.hpp"

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
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, nullptr, nullptr) ||
        !eglBindAPI(EGL_OPENGL_API))
        return false;
    const EGLint attributes[] = {EGL_CONTEXT_MAJOR_VERSION,
                                 4,
                                 EGL_CONTEXT_MINOR_VERSION,
                                 5,
                                 EGL_CONTEXT_OPENGL_PROFILE_MASK,
                                 EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                                 EGL_NONE};
    EGLContext context = eglCreateContext(display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, attributes);
    return context != EGL_NO_CONTEXT &&
           eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context);
}

bool load_font(const std::string &path, ppz::gfx::Font *font)
{
    std::string data;
    return ppz::save::read_file(path, &data) && font->load(data);
}

ppz::InputFrame press(ppz::Action action)
{
    ppz::InputFrame frame;
    frame.connected = true;
    frame.pressed = ppz::action_bit(action);
    return frame;
}

ppz::InputFrame nav(ppz::Direction direction)
{
    ppz::InputFrame frame;
    frame.connected = true;
    frame.nav = direction;
    return frame;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 4)
        return 2;
    const std::string assets = argv[1];
    const std::string output = argv[2];
    const std::string data = argv[3];
    const int moves_right = argc > 4 ? std::atoi(argv[4]) : 0;
    const int width = 1920;
    const int height = 1080;
    if (!open_context())
        return 1;
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

    ppz::app::Shell shell(batch, fonts, 1.0f, data);
    ppz::gfx::DrawList list;
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width * height * 4));
    stbi_flip_vertically_on_write(1);
    int written = 0;
    const float dt = 1.0f / 60.0f;
    ppz::InputFrame idle;
    idle.connected = true;
    // One frame as the console runs it: update, then draw into the surface.
    const auto step = [&](const ppz::InputFrame &input, bool capture)
    {
        shell.update(input, dt);
        shell.take_cues();
        list.clear();
        shell.draw(list);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        batch.draw(list, ppz::gfx::fit_viewport(width, height), width, height);
        if (!capture)
            return;
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        char name[64];
        std::snprintf(name, sizeof(name), "/frame-%03d.png", written++);
        stbi_write_png((output + name).c_str(), width, height, 4, pixels.data(), width * 4);
    };
    const auto run = [&](int frames, bool capture = false)
    {
        for (int i = 0; i < frames; ++i)
            step(idle, capture);
    };

    run(30);
    for (int i = 0; i < moves_right; ++i)
    {
        step(nav(ppz::Direction::right), false);
        run(6);
    }
    step(press(ppz::Action::confirm), false); // launch the focused game
    run(60);
    step(press(ppz::Action::confirm), false); // one move, so there is a game to keep
    run(20);
    step(press(ppz::Action::menu), false); // pause menu
    run(20);
    step(nav(ppz::Direction::up), false); // the last entry returns to the library
    run(6);
    step(press(ppz::Action::confirm), true);
    run(40, true);
    std::fprintf(stderr, "wrote %d frames, GL error 0x%x\n", written, glGetError());
    return 0;
}
