// ProsperoPuzzles - Trail play screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "games/kit/puzzle_scene.hpp"
#include "games/trail/core.hpp"

namespace ppz::trail
{

class TrailScene final : public kit::PuzzleScene
{
  public:
    explicit TrailScene(const ui::Fonts &fonts);

    // Test hooks.
    const Puzzle &puzzle() const
    {
        return puzzle_;
    }
    const std::vector<std::uint8_t> &path() const
    {
        return path_;
    }
    bool cutting() const
    {
        return cutting_;
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
        return cutting_; // while drawing, the ring is the path's head
    }
    bool wants_back() const override
    {
        return cutting_; // Circle leaves cut mode instead of pausing
    }
    std::vector<ui::Hint> hints() const override;
    bool extra_stat(std::string *label, std::string *value) const override;

  private:
    void draw_mode(std::vector<audio::Cue> &cues, const InputFrame &input);
    void cut_mode(std::vector<audio::Cue> &cues, const InputFrame &input);
    int path_index(int cell) const;

    Puzzle puzzle_;
    std::vector<std::uint8_t> path_;
    bool cutting_ = false;
    std::array<float, kMaxCells> pop_{}; // 1 when a cell joins the path, easing to 0
};

} // namespace ppz::trail
