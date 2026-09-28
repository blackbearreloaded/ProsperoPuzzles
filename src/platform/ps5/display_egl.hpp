// ProsperoPuzzles - EGL display and OpenGL 4.6 Core context.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <EGL/egl.h>

namespace ppz::ps5
{

// Owns the single EGL display, window surface and GL 4.6 Core context for the
// process lifetime. ps5-opengl presents fullscreen at the SDK's build-time
// profile; reopening a presenter is slow, so open once and never recreate.
class Display
{
  public:
    Display() = default;
    Display(const Display &) = delete;
    Display &operator=(const Display &) = delete;
    ~Display();

    bool open();
    bool swap();
    void close();

    int width() const
    {
        return width_;
    }
    int height() const
    {
        return height_;
    }
    EGLint last_error() const
    {
        return last_error_;
    }

  private:
    void fail(const char *operation);

    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLContext context_ = EGL_NO_CONTEXT;
    int width_ = 0;
    int height_ = 0;
    EGLint last_error_ = EGL_SUCCESS;
};

const char *egl_error_name(EGLint error);

} // namespace ppz::ps5
