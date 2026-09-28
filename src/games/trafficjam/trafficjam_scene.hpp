// ProsperoPuzzles - Traffic Jam play screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/tween.hpp"
#include "games/kit/puzzle_scene.hpp"
#include "games/trafficjam/core.hpp"

namespace ppz::trafficjam
{

class TrafficJamScene final : public kit::PuzzleScene
{
  public:
    explicit TrafficJamScene(const ui::Fonts &fonts);

    // Test hooks.
    const Puzzle &puzzle() const
    {
        return puzzle_;
    }
    const Positions &positions() const
    {
        return pos_;
    }
    int grabbed() const
    {
        return grabbed_;
    }

  protected:
    void generate(std::uint64_t seed, int size) override;
    void restart() override;
    std::string serialize() const override;
    bool deserialize(std::string_view data) override;
    bool solved() const override;
    void play(const InputFrame &input, std::vector<audio::Cue> &cues) override;
    void animate(float dt) override;
    int grid_cols() const override
    {
        return kSide;
    }
    int grid_rows() const override
    {
        return kSide;
    }
    kit::Grid grid() const override;
    void draw_board(gfx::DrawList &list) const override;
    gfx::Rect cursor_rect(float col, float row) const override;
    bool kit_moves_cursor() const override
    {
        return grabbed_ < 0;
    }
    bool wants_back() const override
    {
        return grabbed_ >= 0;
    }
    std::vector<ui::Hint> hints() const override;
    bool extra_stat(std::string *label, std::string *value) const override;

  private:
    void snap_springs();
    void pick_colors();
    gfx::Color vehicle_color(int v) const;
    void slide(int dir, std::vector<audio::Cue> &cues);
    void draw_vehicle(gfx::DrawList &list, const kit::Grid &g, int v, float along, float lift,
                      float pop) const;

    Puzzle puzzle_;
    Positions pos_{};
    int grabbed_ = -1;  // the vehicle held by the player, or -1
    bool slid_ = false; // the current grab has moved (its undo step is recorded)
    std::array<tween::Spring, kMaxVehicles> along_{}; // drawn position along each axis
    std::array<tween::Spring, kMaxVehicles> lift_{};  // 0 resting, 1 held
    std::array<std::uint8_t, kMaxVehicles> hue_{};    // palette slot of each vehicle
};

} // namespace ppz::trafficjam
