// ProsperoPuzzles - Nurikabe play screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "games/kit/puzzle_scene.hpp"
#include "games/nurikabe/core.hpp"

namespace ppz::nurikabe
{

class NurikabeScene final : public kit::PuzzleScene
{
  public:
    explicit NurikabeScene(const ui::Fonts &fonts);

    // Test hooks.
    const Puzzle &puzzle() const
    {
        return puzzle_;
    }
    const Cells &marks() const
    {
        return marks_;
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
    std::vector<ui::Hint> hints() const override;
    bool extra_stat(std::string *label, std::string *value) const override;

  private:
    void set_mark(int cell, std::uint8_t mark, std::vector<audio::Cue> &cues);
    void settle(); // snaps the animations to the marks (new puzzle, restart, load)

    Puzzle puzzle_;
    Cells marks_{};
    std::array<float, kMaxCells> pop_{}; // per-cell pop when the player acts
    std::array<float, kMaxCells> wet_{}; // 0..1 how far a cell has flooded
};

} // namespace ppz::nurikabe
