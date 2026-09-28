// ProsperoPuzzles - Modern skin for the Tatham puzzles: palette and shape styling.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "gfx/draw_list.hpp"

#include <string_view>
#include <vector>

namespace ppz::sgt
{

// How the canvas renderer draws one game.
struct Style
{
    // Filled, non-background rectangles at least this big (canvas pixels,
    // relative to the board's short side) get rounded corners. 0 disables.
    float round_min_fraction = 0.0f;
    float round_radius = 0.18f; // of the rectangle's short side
    // Filled circles lose their 1px outline (flat, modern discs).
    bool flat_discs = false;
    // Strokes at least this thick keep their width; thinner lines are drawn
    // at this width instead (0 = unchanged). Canvas pixels per 1000 of board.
    float min_line = 0.0f;
};

Style style_for(std::string_view game_id);

// Maps Tatham's palette (index 0 is always the background) onto the app's
// colours: warm paper background, navy ink, and the 2048/Tenfold hues for
// saturated colours. Per-game overrides fix colours the generic mapping
// cannot place. rgb is Tatham's flat r,g,b float array.
std::vector<gfx::Color> restyle_palette(std::string_view game_id, const std::vector<float> &rgb);

} // namespace ppz::sgt
