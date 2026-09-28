// ProsperoPuzzles - About screen: credits, sources and version.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "audio/cues.hpp"
#include "core/input.hpp"
#include "gfx/draw_list.hpp"
#include "ui/theme.hpp"

#include <string>
#include <vector>

namespace ppz::ui
{

class AboutScene
{
  public:
    // Returns true when the player leaves the screen.
    bool update(const InputFrame &input, std::vector<audio::Cue> &cues);
    void draw(gfx::DrawList &list, const Fonts &fonts, const std::string &version) const;
};

} // namespace ppz::ui
