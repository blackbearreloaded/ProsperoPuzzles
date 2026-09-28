// ProsperoPuzzles - Color Sort play screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/tween.hpp"
#include "games/colorsort/core.hpp"
#include "games/kit/puzzle_scene.hpp"

namespace ppz::colorsort
{

class ColorSortScene final : public kit::PuzzleScene
{
  public:
    explicit ColorSortScene(const ui::Fonts &fonts);

    // Test hooks.
    const Board &board() const
    {
        return board_;
    }
    // The tube whose top balls are lifted, or -1.
    int lifted() const
    {
        return lifted_;
    }
    int columns() const
    {
        return grid_cols();
    }

  protected:
    void generate(std::uint64_t seed, int size) override;
    void restart() override;
    std::string serialize() const override;
    bool deserialize(std::string_view data) override;
    bool solved() const override;
    void play(const InputFrame &input, std::vector<audio::Cue> &cues) override;
    void animate(float dt) override;
    int grid_cols() const override;
    int grid_rows() const override;
    kit::Grid grid() const override;
    gfx::Rect cursor_rect(float col, float row) const override;
    void draw_board(gfx::DrawList &list) const override;
    std::vector<ui::Hint> hints() const override;
    bool extra_stat(std::string *label, std::string *value) const override;
    bool wants_back() const override
    {
        return lifted_ >= 0;
    }

  private:
    // Where the tubes sit on screen.
    struct Layout
    {
        int cols = 1;
        float tube_w = 1.0f; // tube width
        float tube_h = 1.0f; // tube height
        float head = 0.0f;   // room above each tube for lifted balls
        float ball = 1.0f;   // ball diameter
        float step = 1.0f;   // vertical distance between stacked balls
        float gap_x = 0.0f;  // between tubes in a row
        float gap_y = 0.0f;  // between rows
        float x = 0.0f;      // first tube's left edge
        float y = 0.0f;      // top of the first row (its head room)
        gfx::Rect card{};

        gfx::Rect tube(float col, float row) const;
        gfx::Rect tube(int index) const;
        // Centre of the ball resting in slot (0 = bottom) of a tube.
        float slot_x(int index) const;
        float slot_y(int index, int slot) const;
        // Centre of the top lifted ball, just above the tube's mouth.
        float hover_y(int index) const;
    };
    // A ball in the air between two tubes.
    struct Flight
    {
        int colour = 0;
        int tube = 0; // destination
        int slot = 0;
        float from_x = 0.0f;
        float from_y = 0.0f;
        float t = 0.0f; // seconds; negative while waiting its turn
    };

    Layout layout() const;
    int tube_at(int col, int row) const;
    void clear_motion();
    void lift(int tube, std::vector<audio::Cue> &cues);
    void put_back(std::vector<audio::Cue> &cues);
    void pour_into(int tube, std::vector<audio::Cue> &cues);
    // Upward offset of a tube's lifted balls right now.
    float lift_offset(int tube) const;

    Board start_{}; // as generated, for restart
    Board board_{};
    int lifted_ = -1;
    std::array<int, kMaxTubes> lifted_count_{};   // top balls drawn raised
    std::array<float, kMaxTubes> lift_px_{};      // full lift height of those balls
    std::array<tween::Spring, kMaxTubes> lift_{}; // 0 resting .. 1 raised
    std::array<float, kMaxTubes> land_{};         // bounce when balls land (1 -> 0)
    std::array<float, kMaxTubes> badge_{};        // seconds since a tube completed
    std::vector<Flight> flights_;
};

} // namespace ppz::colorsort
