// ProsperoPuzzles - System presentation art (icon0, pic0/pic1) drawn with the app renderer.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "app/thumbnails.hpp"
#include "gfx/draw_list.hpp"
#include "ui/theme.hpp"

#include <functional>

namespace ppz::host
{

// Draws art-icon (centred 1080x1080 square of the 1920x1080 frame) and
// art-background, handing each finished frame to write(name).
bool render_art(gfx::DrawList &list, const ui::Fonts &fonts, app::Thumbnails &thumbnails,
                const std::function<bool(const char *)> &write);

} // namespace ppz::host
