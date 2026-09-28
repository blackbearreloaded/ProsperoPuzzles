// ProsperoPuzzles - Visual test scene exercising every 2D primitive.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "gfx/draw_list.hpp"
#include "ui/theme.hpp"

#include <cstdint>

namespace ppz::ui
{

struct GalleryState
{
    double seconds = 0.0;
    std::uint32_t held_actions = 0; // lights the button strip
    int focused_card = 0;
    const char *status = "";
};

// Draws the gallery in the 1920x1080 virtual space: gradients, cards with
// shadows, text at every theme size, face-button symbols, lines, rings and a
// live input strip. Used by host snapshots and the console diagnostics.
void draw_gallery(gfx::DrawList &list, const Fonts &fonts, const GalleryState &state);

} // namespace ppz::ui
