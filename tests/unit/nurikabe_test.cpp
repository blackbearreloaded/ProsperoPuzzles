// ProsperoPuzzles - Nurikabe rules, generator and play screen tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/nurikabe/core.hpp"
#include "games/nurikabe/nurikabe_scene.hpp"

#include <gtest/gtest.h>

#include <algorithm>

using namespace ppz;
using namespace ppz::nurikabe;

namespace
{

// The generator's solution as player marks.
Cells solution_marks(const Puzzle &p)
{
    Cells marks{};
    for (int i = 0; i < p.side * p.side; ++i)
        marks[static_cast<std::size_t>(i)] =
            p.solution[static_cast<std::size_t>(i)] ? kSea : kUnknown;
    return marks;
}

InputFrame press(Action action)
{
    InputFrame f;
    f.pressed = action_bit(action);
    return f;
}

// Walks the scene's cursor to (col, row).
void walk(NurikabeScene &scene, int col, int row, std::vector<audio::Cue> &cues)
{
    while (scene.cursor_row() != row || scene.cursor_col() != col)
    {
        InputFrame step;
        step.nav = scene.cursor_row() < row   ? Direction::down
                   : scene.cursor_row() > row ? Direction::up
                   : scene.cursor_col() < col ? Direction::right
                                              : Direction::left;
        scene.update(step, 0.016f, cues);
    }
}

} // namespace

TEST(Nurikabe, GeneratedPuzzlesHaveExactlyOneSolution)
{
    for (int side : {6, 8, 10})
    {
        for (std::uint64_t seed = 1; seed <= 5; ++seed)
        {
            const Puzzle p = generate(seed * 7919 + static_cast<std::uint64_t>(side), side);
            ASSERT_EQ(p.side, side);
            EXPECT_TRUE(valid_solution(p, p.solution)) << "side " << side << " seed " << seed;
            Cells found{};
            EXPECT_EQ(count_solutions(p, 2, &found), 1) << "side " << side << " seed " << seed;
            EXPECT_EQ(found, p.solution);
            EXPECT_TRUE(solved(p, solution_marks(p)));
            EXPECT_EQ(sea_marked(p, solution_marks(p)), sea_target(p));
            int land = 0;
            for (int i = 0; i < side * side; ++i)
                land += p.clue[static_cast<std::size_t>(i)];
            EXPECT_EQ(land + sea_target(p), side * side); // numbers add up to the land
        }
    }
}

TEST(Nurikabe, GenerationIsDeterministic)
{
    const Puzzle a = generate(2026, 10);
    const Puzzle b = generate(2026, 10);
    EXPECT_EQ(a.clue, b.clue);
    EXPECT_EQ(a.solution, b.solution);
}

TEST(Nurikabe, SolvedTreatsUnmarkedCellsAsIsland)
{
    const Puzzle p = generate(42, 6);
    Cells marks = solution_marks(p);
    // Dots on island cells change nothing.
    for (int i = 0; i < 36; ++i)
        if (marks[static_cast<std::size_t>(i)] == kUnknown &&
            p.clue[static_cast<std::size_t>(i)] == 0)
            marks[static_cast<std::size_t>(i)] = kDot;
    EXPECT_TRUE(solved(p, marks));
    EXPECT_TRUE(errors(p, marks).pools.empty());
    // One sea cell left unshaded joins an island and breaks it.
    for (int i = 0; i < 36; ++i)
        if (marks[static_cast<std::size_t>(i)] == kSea)
        {
            marks[static_cast<std::size_t>(i)] = kUnknown;
            break;
        }
    EXPECT_FALSE(solved(p, marks));
}

TEST(Nurikabe, ErrorsFlagJoinedNumbersWalledNumbersAndPools)
{
    Puzzle p;
    p.side = 5;
    p.clue[0] = 2;  // (0,0)
    p.clue[2] = 1;  // (2,0)
    p.clue[24] = 3; // (4,4)
    Cells marks{};
    marks[1] = kDot; // joins the 2 and the 1
    Errors e = errors(p, marks);
    EXPECT_TRUE(e.bad[0]);
    EXPECT_TRUE(e.bad[2]);
    EXPECT_FALSE(e.bad[24]);
    EXPECT_TRUE(e.pools.empty());

    // The 3 walled in by sea with room for two cells.
    marks[1] = kUnknown;
    marks[19] = kSea; // (4,3)
    marks[22] = kSea; // (2,4)
    marks[18] = kSea; // (3,3)
    e = errors(p, marks);
    EXPECT_TRUE(e.bad[24]);

    // A 2x2 block of sea.
    marks[13] = kSea; // (3,2)
    marks[14] = kSea; // (4,2)
    e = errors(p, marks);
    ASSERT_EQ(e.pools.size(), 1u);
    EXPECT_EQ(e.pools[0], 13);
}

TEST(NurikabeScene, PlaysUndoesSavesAndSolves)
{
    ui::Fonts fonts;
    NurikabeScene scene(fonts);
    scene.start({}, {});
    scene.new_puzzle(99, 0);
    std::vector<audio::Cue> cues;
    const Puzzle p = scene.puzzle();
    const int n = p.side;

    // A numbered cell cannot be shaded.
    int numbered = 0;
    while (p.clue[static_cast<std::size_t>(numbered)] == 0)
        ++numbered;
    walk(scene, numbered % n, numbered / n, cues);
    cues.clear();
    scene.update(press(Action::confirm), 0.016f, cues);
    EXPECT_EQ(scene.marks()[static_cast<std::size_t>(numbered)], kUnknown);
    EXPECT_NE(std::find(cues.begin(), cues.end(), audio::Cue::invalid), cues.end());

    // Sea on a plain cell, undo, redo, then a dot replaces it.
    int plain = 0;
    while (p.clue[static_cast<std::size_t>(plain)] != 0)
        ++plain;
    walk(scene, plain % n, plain / n, cues);
    const auto mark = [&] { return scene.marks()[static_cast<std::size_t>(plain)]; };
    scene.update(press(Action::confirm), 0.016f, cues);
    EXPECT_EQ(mark(), kSea);
    scene.update(press(Action::page_prev), 0.016f, cues);
    EXPECT_EQ(mark(), kUnknown);
    scene.update(press(Action::page_next), 0.016f, cues);
    EXPECT_EQ(mark(), kSea);
    scene.update(press(Action::west), 0.016f, cues);
    EXPECT_EQ(mark(), kDot);
    scene.update(press(Action::confirm), 0.016f, cues); // dot -> sea
    EXPECT_EQ(mark(), kSea);
    EXPECT_TRUE(scene.in_progress());

    // The save restores the same board and marks.
    const std::string save = scene.save();
    NurikabeScene restored(fonts);
    restored.start(save, scene.stats());
    EXPECT_EQ(restored.marks(), scene.marks());
    EXPECT_EQ(restored.puzzle().clue, p.clue);
    EXPECT_EQ(restored.puzzle().solution, p.solution);

    // Clear the test mark, then shade every solution sea cell.
    scene.update(press(Action::confirm), 0.016f, cues);
    EXPECT_EQ(mark(), kUnknown);
    for (int i = 0; i < n * n; ++i)
    {
        if (!p.solution[static_cast<std::size_t>(i)])
            continue;
        walk(scene, i % n, i / n, cues);
        scene.update(press(Action::confirm), 0.016f, cues);
    }
    EXPECT_TRUE(scene.is_solved());
    EXPECT_NE(std::find(cues.begin(), cues.end(), audio::Cue::complete), cues.end());
    EXPECT_FALSE(scene.in_progress());
    for (int frame = 0; frame < 120; ++frame)
        scene.update(InputFrame{}, 1.0f / 60.0f, cues);
    EXPECT_TRUE(scene.menu_open()); // the solved menu follows the flourish
}

TEST(NurikabeScene, SizesStartNewPuzzlesAndBadSavesAreRefused)
{
    ui::Fonts fonts;
    NurikabeScene scene(fonts);
    scene.start({}, {});
    scene.new_puzzle(5, 2);
    EXPECT_EQ(scene.puzzle().side, 10);
    EXPECT_EQ(scene.size(), 2);
    scene.new_puzzle(5, 1);
    EXPECT_EQ(scene.puzzle().side, 8);

    // A damaged save falls back to a fresh puzzle rather than loading garbage.
    std::string save = scene.save();
    for (char &c : save)
        c = static_cast<char>(0x7f);
    NurikabeScene broken(fonts);
    broken.start(save, {});
    EXPECT_GE(broken.puzzle().side, kMinSide);
    EXPECT_TRUE(valid_solution(broken.puzzle(), broken.puzzle().solution));
}
