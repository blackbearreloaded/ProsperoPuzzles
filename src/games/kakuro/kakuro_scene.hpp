// ProsperoPuzzles - Kakuro play screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/tween.hpp"
#include "games/kakuro/core.hpp"
#include "games/kit/puzzle_scene.hpp"

namespace ppz::kakuro
{

class KakuroScene final : public kit::PuzzleScene
{
  public:
    explicit KakuroScene(const ui::Fonts &fonts);

    // Test hooks.
    const Puzzle &puzzle() const
    {
        return puzzle_;
    }
    const Digits &digits() const
    {
        return digits_;
    }
    bool picker_open() const
    {
        return picker_open_;
    }
    int picker_focus() const
    {
        return picker_focus_;
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
        return puzzle_.side;
    }
    int grid_rows() const override
    {
        return puzzle_.side;
    }
    kit::Grid grid() const override;
    void draw_board(gfx::DrawList &list) const override;
    void draw_overlay(gfx::DrawList &list) const override;
    gfx::Rect cursor_rect(float col, float row) const override;
    bool kit_moves_cursor() const override
    {
        return false;
    }
    bool wants_back() const override
    {
        return picker_open_;
    }
    std::vector<ui::Hint> hints() const override;
    bool extra_stat(std::string *label, std::string *value) const override;

  private:
    // The white cell nearest to (col, row); the cell itself when it is white.
    int nearest_white(int col, int row) const;
    void move(Direction direction, std::vector<audio::Cue> &cues);
    void place(int cell, int digit, std::vector<audio::Cue> &cues);
    // Where the picker card and its nine buttons sit for the cursor's cell.
    gfx::Rect picker_card() const;

    Puzzle puzzle_;
    Digits digits_{};
    std::array<float, kMaxCells> pop_{}; // per-cell pop when a digit lands
    bool picker_open_ = false;
    int picker_focus_ = 4; // 0..8 for digits 1..9
    tween::Timer picker_intro_;
    tween::Spring focus_x_; // the focused picker button slides between digits
    tween::Spring focus_y_;
};

} // namespace ppz::kakuro
