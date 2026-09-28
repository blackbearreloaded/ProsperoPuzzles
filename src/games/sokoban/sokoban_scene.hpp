// ProsperoPuzzles - Sokoban play screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "games/kit/puzzle_scene.hpp"
#include "games/sokoban/core.hpp"

namespace ppz::sokoban
{

class SokobanScene final : public kit::PuzzleScene
{
  public:
    explicit SokobanScene(const ui::Fonts &fonts);

    // Test hooks.
    const Puzzle &puzzle() const
    {
        return puzzle_;
    }
    const State &state() const
    {
        return state_;
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
        return puzzle_.cols;
    }
    int grid_rows() const override
    {
        return puzzle_.rows;
    }
    kit::Grid grid() const override;
    void draw_board(gfx::DrawList &list) const override;
    bool show_cursor() const override
    {
        return false; // the keeper is the focus
    }
    bool kit_moves_cursor() const override
    {
        return false;
    }
    std::vector<ui::Hint> hints() const override;
    bool extra_stat(std::string *label, std::string *value) const override;

  private:
    void snap_pieces();
    void draw_walls(gfx::DrawList &list, const kit::Grid &g) const;
    void draw_crate(gfx::DrawList &list, const kit::Grid &g, int index) const;
    void draw_keeper(gfx::DrawList &list, const kit::Grid &g) const;

    Puzzle puzzle_;
    State state_;
    int facing_ = 1; // direction the keeper looks (down)
    bool placed_ = false;
    tween::Spring keeper_x_;
    tween::Spring keeper_y_;
    std::array<tween::Spring, kMaxCrates> crate_x_{};
    std::array<tween::Spring, kMaxCrates> crate_y_{};
    std::array<float, kMaxCrates> pop_{}; // a crate landing on a target
    float step_ = 0.0f;                   // the keeper's bob on each step
};

} // namespace ppz::sokoban
