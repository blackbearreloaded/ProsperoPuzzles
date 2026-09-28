// ProsperoPuzzles - Link Up play screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "games/kit/puzzle_scene.hpp"
#include "games/linkup/core.hpp"

namespace ppz::linkup
{

class LinkUpScene final : public kit::PuzzleScene
{
  public:
    explicit LinkUpScene(const ui::Fonts &fonts);

    // Test hooks.
    const Puzzle &puzzle() const
    {
        return puzzle_;
    }
    const Paths &paths() const
    {
        return paths_;
    }
    // The pair being drawn, or kNone.
    int drawing() const
    {
        return drawing_;
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
    bool kit_moves_cursor() const override
    {
        return drawing_ == kNone;
    }
    bool wants_back() const override
    {
        return drawing_ != kNone;
    }
    std::vector<ui::Hint> hints() const override;
    bool extra_stat(std::string *label, std::string *value) const override;

  private:
    // Records one undo step per gesture, just before its first change.
    void touch();
    void draw_step(Direction direction, std::vector<audio::Cue> &cues);
    void let_go(std::vector<audio::Cue> &cues);

    Puzzle puzzle_;
    Paths paths_{};
    int drawing_ = kNone;
    bool touched_ = false;
    std::array<float, kMaxCells> pop_{};  // per-cell pop when the head enters
    std::array<float, kMaxPairs> glow_{}; // per-pair glow when it connects
};

} // namespace ppz::linkup
