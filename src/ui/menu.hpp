// ProsperoPuzzles - Modal menu overlay (pause, win and game-over dialogs).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "audio/cues.hpp"
#include "core/input.hpp"
#include "core/tween.hpp"
#include "gfx/draw_list.hpp"
#include "ui/theme.hpp"

#include <string>
#include <vector>

namespace ppz::ui
{

class Menu
{
  public:
    struct Item
    {
        std::string label;
        int id = 0;
        bool enabled = true;
    };
    static constexpr int kCancelled = -2;
    static constexpr int kNone = -1;

    // record is an optional highlighted line under the subtitle (personal bests).
    void open(std::string title, std::vector<Item> items, std::string subtitle = {},
              std::string record = {});
    void close();
    bool is_open() const
    {
        return open_;
    }

    // Returns the chosen item id, kCancelled (Circle/Options, when allowed),
    // or kNone. Also animates the fade even while closed.
    int update(const InputFrame &input, float dt, std::vector<audio::Cue> &cues,
               bool cancellable = true);
    void draw(gfx::DrawList &list, const Fonts &fonts) const;

  private:
    std::string title_;
    std::string subtitle_;
    std::string record_;
    std::vector<Item> items_;
    int focus_ = 0;
    bool open_ = false;
    tween::Spring fade_;
    tween::Spring highlight_;
};

} // namespace ppz::ui
