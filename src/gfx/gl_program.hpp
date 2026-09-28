// ProsperoPuzzles - GLSL program compilation.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <GL/glcorearb.h>

namespace ppz::gfx
{

// Shader sources carry no #version line; the platform prefix is prepended here
// ("#version 460 core" on PS5) so the same sources can target other hosts.
extern const char *const kGlslPrefix;

// Compiles and links a vertex/fragment program. Returns 0 and logs the info
// log on failure.
GLuint build_program(const char *label, const char *vertex_source, const char *fragment_source);

} // namespace ppz::gfx
