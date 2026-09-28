// ProsperoPuzzles - How to play card: rules and controls for one game.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "audio/cues.hpp"
#include "core/input.hpp"
#include "core/tween.hpp"
#include "games/registry.hpp"
#include "gfx/draw_list.hpp"
#include "ui/theme.hpp"

#include <string>
#include <vector>

namespace ppz::ui
{

// A modal card over the current screen. Cross or Circle closes it.
class HowToCard
{
  public:
    void open(const games::GameInfo &game);
    bool is_open() const
    {
        return open_;
    }
    void update(const InputFrame &input, float dt, std::vector<audio::Cue> &cues);
    // Keeps the fade running while closed (call every frame).
    void animate(float dt);
    void draw(gfx::DrawList &list, const Fonts &fonts) const;

  private:
    std::string title_;
    std::string tagline_;
    std::string rules_;
    std::string controls_;
    gfx::Color accent_{};
    bool open_ = false;
    tween::Spring fade_;
};

} // namespace ppz::ui
