// ProsperoPuzzles - Crowns rules, generator and play screen tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/crowns/core.hpp"
#include "games/crowns/crowns_scene.hpp"

#include <gtest/gtest.h>

using namespace ppz;
using namespace ppz::crowns;

namespace
{

// Marks the generator's solution on a fresh mark array.
std::array<std::uint8_t, kMaxCells> solution_marks(const Puzzle &p)
{
    std::array<std::uint8_t, kMaxCells> marks{};
    for (int r = 0; r < p.side; ++r)
        marks[static_cast<std::size_t>(r * p.side + p.solution[static_cast<std::size_t>(r)])] =
            kCrown;
    return marks;
}

InputFrame press(Action action)
{
    InputFrame f;
    f.pressed = action_bit(action);
    return f;
}

} // namespace

TEST(Crowns, GeneratedPuzzlesHaveExactlyOneSolution)
{
    for (int side : {6, 7, 8})
    {
        for (std::uint64_t seed = 1; seed <= 6; ++seed)
        {
            const Puzzle p = generate(seed * 7919 + side, side);
            ASSERT_EQ(p.side, side);
            std::array<std::uint8_t, kMaxSide> found{};
            EXPECT_EQ(count_solutions(p, 5, &found), 1) << "side " << side << " seed " << seed;
            EXPECT_EQ(found, p.solution);
            // Every region is used and holds exactly one solution crown.
            std::vector<int> per_region(static_cast<std::size_t>(side), 0);
            for (int r = 0; r < side; ++r)
                ++per_region[p.region[static_cast<std::size_t>(
                    r * side + p.solution[static_cast<std::size_t>(r)])]];
            for (int count : per_region)
                EXPECT_EQ(count, 1);
            EXPECT_TRUE(solved(p, solution_marks(p)));
        }
    }
}

TEST(Crowns, ConflictsFlagRowsColumnsRegionsAndTouching)
{
    const Puzzle p = generate(42, 6);
    std::array<std::uint8_t, kMaxCells> marks{};
    marks[0] = kCrown;
    marks[7] = kCrown; // diagonal neighbour of cell 0
    const std::vector<bool> bad = conflicts(p, marks);
    EXPECT_TRUE(bad[0]);
    EXPECT_TRUE(bad[7]);
    EXPECT_FALSE(solved(p, marks));
    marks[7] = kEmpty;
    marks[3] = kCrown; // same row
    EXPECT_TRUE(conflicts(p, marks)[3]);
}

TEST(CrownsScene, PlaysUndoesSavesAndSolves)
{
    ui::Fonts fonts;
    CrownsScene scene(fonts);
    scene.start({}, {});
    scene.new_puzzle(99, 0);
    std::vector<audio::Cue> cues;
    const Puzzle p = scene.puzzle();

    // Crown on the cursor cell, then undo it.
    const int cell = scene.cursor_row() * p.side + scene.cursor_col();
    scene.update(press(Action::confirm), 0.016f, cues);
    EXPECT_EQ(scene.marks()[static_cast<std::size_t>(cell)], kCrown);
    scene.update(press(Action::page_prev), 0.016f, cues);
    EXPECT_EQ(scene.marks()[static_cast<std::size_t>(cell)], kEmpty);
    scene.update(press(Action::page_next), 0.016f, cues); // redo
    EXPECT_EQ(scene.marks()[static_cast<std::size_t>(cell)], kCrown);
    EXPECT_TRUE(scene.in_progress());

    // The save restores the same board and marks.
    const std::string save = scene.save();
    CrownsScene restored(fonts);
    restored.start(save, scene.stats());
    EXPECT_EQ(restored.marks(), scene.marks());
    for (int i = 0; i < p.side * p.side; ++i)
        EXPECT_EQ(restored.puzzle().region[static_cast<std::size_t>(i)],
                  p.region[static_cast<std::size_t>(i)]);

    // Walk the cursor to each solution cell and crown it.
    scene.update(press(Action::confirm), 0.016f, cues); // clear the test crown
    for (int r = 0; r < p.side; ++r)
    {
        const int c = p.solution[static_cast<std::size_t>(r)];
        while (scene.cursor_row() != r || scene.cursor_col() != c)
        {
            InputFrame step;
            step.nav = scene.cursor_row() < r   ? Direction::down
                       : scene.cursor_row() > r ? Direction::up
                       : scene.cursor_col() < c ? Direction::right
                                                : Direction::left;
            scene.update(step, 0.016f, cues);
        }
        scene.update(press(Action::confirm), 0.016f, cues);
    }
    EXPECT_TRUE(scene.is_solved());
    EXPECT_NE(std::find(cues.begin(), cues.end(), audio::Cue::complete), cues.end());
    EXPECT_FALSE(scene.in_progress());
    for (int frame = 0; frame < 120; ++frame)
        scene.update(InputFrame{}, 1.0f / 60.0f, cues);
    EXPECT_TRUE(scene.menu_open()); // the solved menu follows the flourish
}

TEST(CrownsScene, CircleOpensPauseAndSizesStartNewPuzzles)
{
    ui::Fonts fonts;
    CrownsScene scene(fonts);
    scene.start({}, {});
    std::vector<audio::Cue> cues;
    scene.update(press(Action::back), 0.016f, cues);
    EXPECT_TRUE(scene.menu_open());
    // Resume closes it.
    scene.update(press(Action::confirm), 0.016f, cues);
    EXPECT_FALSE(scene.menu_open());
    scene.new_puzzle(5, 2);
    EXPECT_EQ(scene.puzzle().side, 8);
    EXPECT_EQ(scene.size(), 2);
}
