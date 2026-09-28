// ProsperoPuzzles - Kakuro rules, generator and play screen tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/kakuro/core.hpp"
#include "games/kakuro/kakuro_scene.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>

using namespace ppz;
using namespace ppz::kakuro;

namespace
{

InputFrame press(Action action)
{
    InputFrame f;
    f.pressed = action_bit(action);
    return f;
}

InputFrame nav(Direction direction)
{
    InputFrame f;
    f.nav = direction;
    return f;
}

// Walks the scene's cursor onto a white cell with D-pad presses only: a
// breadth-first search over copies of the scene finds the shortest path.
void walk_to(KakuroScene &scene, int cell, std::vector<audio::Cue> &cues)
{
    const int n = scene.puzzle().side;
    const auto at_cell = [n](const KakuroScene &s) { return s.cursor_row() * n + s.cursor_col(); };
    struct Node
    {
        KakuroScene scene;
        std::vector<Direction> path;
    };
    std::vector<Node> queue{{scene, {}}};
    std::vector<bool> seen(static_cast<std::size_t>(n * n), false);
    seen[static_cast<std::size_t>(at_cell(scene))] = true;
    std::vector<audio::Cue> scratch;
    for (std::size_t head = 0; head < queue.size(); ++head)
    {
        if (at_cell(queue[head].scene) == cell)
        {
            const std::vector<Direction> path = queue[head].path;
            for (Direction d : path)
                scene.update(nav(d), 0.016f, cues);
            EXPECT_EQ(at_cell(scene), cell);
            return;
        }
        for (Direction d : {Direction::up, Direction::down, Direction::left, Direction::right})
        {
            Node next{queue[head].scene, queue[head].path};
            next.scene.update(nav(d), 0.016f, scratch);
            const int reached = at_cell(next.scene);
            if (seen[static_cast<std::size_t>(reached)])
                continue;
            seen[static_cast<std::size_t>(reached)] = true;
            next.path.push_back(d);
            queue.push_back(std::move(next));
        }
    }
    FAIL() << "could not reach cell " << cell;
}

// Opens the picker, moves its focus to digit and places it.
void enter_digit(KakuroScene &scene, int digit, std::vector<audio::Cue> &cues)
{
    scene.update(press(Action::confirm), 0.016f, cues);
    ASSERT_TRUE(scene.picker_open());
    const int target = digit - 1;
    while (scene.picker_focus() != target)
    {
        const int f = scene.picker_focus();
        const Direction d = f / 3 < target / 3   ? Direction::down
                            : f / 3 > target / 3 ? Direction::up
                            : f % 3 < target % 3 ? Direction::right
                                                 : Direction::left;
        scene.update(nav(d), 0.016f, cues);
    }
    scene.update(press(Action::confirm), 0.016f, cues);
    EXPECT_FALSE(scene.picker_open());
}

} // namespace

TEST(Kakuro, GeneratedPuzzlesAreWellFormedAndUnique)
{
    for (int side : {6, 8, 10})
    {
        for (std::uint64_t seed = 1; seed <= 6; ++seed)
        {
            const Puzzle p = generate(seed * 7919 + static_cast<std::uint64_t>(side), side);
            ASSERT_EQ(p.side, side);
            EXPECT_TRUE(well_formed(p)) << "side " << side << " seed " << seed;
            Digits found{};
            EXPECT_EQ(count_solutions(p, 5, &found), 1) << "side " << side << " seed " << seed;
            EXPECT_EQ(found, p.solution);
            EXPECT_TRUE(solved(p, p.solution));
            // Reasonably dense: at least half of the grid is white.
            EXPECT_GE(white_cells(p) * 2, side * side);
            for (int i = 0; i < side; ++i)
            {
                EXPECT_FALSE(p.white(i));        // clue row
                EXPECT_FALSE(p.white(i * side)); // clue column
            }
        }
    }
}

TEST(Kakuro, GenerationIsDeterministicAndFast)
{
    const Puzzle a = generate(77, 10);
    const Puzzle b = generate(77, 10);
    EXPECT_EQ(a.solution, b.solution);
    EXPECT_EQ(a.right, b.right);
    const auto start = std::chrono::steady_clock::now();
    for (std::uint64_t seed = 100; seed < 110; ++seed)
        generate(seed, 10);
    const auto ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    // Generous: sanitizer builds are several times slower than release.
    EXPECT_LT(ms / 10.0, 1000.0);
}

TEST(Kakuro, ConflictsFlagRepeatsAndWrongSums)
{
    const Puzzle p = generate(42, 6);
    const std::vector<kakuro::Run> all = runs(p);
    ASSERT_FALSE(all.empty());
    const kakuro::Run &run = all.front();
    ASSERT_GE(run.length, 2);
    Digits digits{};
    const int a = run.cells[0];
    const int b = run.cells[1];
    digits[static_cast<std::size_t>(a)] = 5;
    digits[static_cast<std::size_t>(b)] = 5; // repeated in the run
    std::vector<bool> bad = conflicts(p, digits);
    EXPECT_TRUE(bad[static_cast<std::size_t>(a)]);
    EXPECT_TRUE(bad[static_cast<std::size_t>(b)]);
    EXPECT_FALSE(solved(p, digits));

    // A full run with the right digits is fine; nudge one to break its sum.
    digits.fill(0);
    for (int k = 0; k < run.length; ++k)
        digits[run.cells[static_cast<std::size_t>(k)]] =
            p.solution[run.cells[static_cast<std::size_t>(k)]];
    bad = conflicts(p, digits);
    EXPECT_FALSE(bad[static_cast<std::size_t>(a)]);
    std::uint8_t &first = digits[static_cast<std::size_t>(a)];
    unsigned used = 0;
    for (int k = 0; k < run.length; ++k)
        used |= 1u << digits[run.cells[static_cast<std::size_t>(k)]];
    for (int d = 1; d <= 9; ++d)
        if ((used & (1u << d)) == 0)
        {
            first = static_cast<std::uint8_t>(d);
            break;
        }
    bad = conflicts(p, digits);
    for (int k = 0; k < run.length; ++k)
        EXPECT_TRUE(bad[run.cells[static_cast<std::size_t>(k)]]);

    // A partly filled run with a small sum is not an error yet.
    digits.fill(0);
    digits[static_cast<std::size_t>(a)] = 1;
    EXPECT_FALSE(conflicts(p, digits)[static_cast<std::size_t>(a)]);
    EXPECT_TRUE(solved(p, p.solution));
}

TEST(KakuroScene, PlaysUndoesSavesAndSolves)
{
    ui::Fonts fonts;
    KakuroScene scene(fonts);
    scene.start({}, {});
    scene.new_puzzle(99, 0);
    std::vector<audio::Cue> cues;
    const Puzzle p = scene.puzzle();
    const int n = p.side;

    // The cursor never rests on a block, even when moving around.
    scene.update(InputFrame{}, 0.016f, cues);
    EXPECT_TRUE(p.white(scene.cursor_row() * n + scene.cursor_col()));
    for (Direction d : {Direction::up, Direction::left, Direction::up, Direction::left,
                        Direction::down, Direction::right})
    {
        scene.update(nav(d), 0.016f, cues);
        EXPECT_TRUE(p.white(scene.cursor_row() * n + scene.cursor_col()));
    }

    // Picker: Cross opens it on 5, the D-pad moves inside it, Circle closes it
    // without leaving to the pause menu.
    const int cell = scene.cursor_row() * n + scene.cursor_col();
    const int row = scene.cursor_row();
    const int col = scene.cursor_col();
    scene.update(press(Action::confirm), 0.016f, cues);
    EXPECT_TRUE(scene.picker_open());
    EXPECT_EQ(scene.picker_focus(), 4);
    scene.update(nav(Direction::up), 0.016f, cues);
    EXPECT_EQ(scene.picker_focus(), 1);
    EXPECT_EQ(scene.cursor_row(), row); // the board cursor stays put
    EXPECT_EQ(scene.cursor_col(), col);
    scene.update(press(Action::back), 0.016f, cues);
    EXPECT_FALSE(scene.picker_open());
    EXPECT_FALSE(scene.menu_open());
    EXPECT_EQ(scene.digits()[static_cast<std::size_t>(cell)], 0);

    // Place a 7, undo, redo; Square clears.
    enter_digit(scene, 7, cues);
    EXPECT_EQ(scene.digits()[static_cast<std::size_t>(cell)], 7);
    scene.update(press(Action::page_prev), 0.016f, cues);
    EXPECT_EQ(scene.digits()[static_cast<std::size_t>(cell)], 0);
    scene.update(press(Action::page_next), 0.016f, cues);
    EXPECT_EQ(scene.digits()[static_cast<std::size_t>(cell)], 7);
    EXPECT_TRUE(scene.in_progress());
    scene.update(press(Action::west), 0.016f, cues);
    EXPECT_EQ(scene.digits()[static_cast<std::size_t>(cell)], 0);
    scene.update(press(Action::page_prev), 0.016f, cues);
    EXPECT_EQ(scene.digits()[static_cast<std::size_t>(cell)], 7);

    // The save restores the same board and digits.
    const std::string save = scene.save();
    KakuroScene restored(fonts);
    restored.start(save, scene.stats());
    EXPECT_EQ(restored.digits(), scene.digits());
    EXPECT_EQ(restored.puzzle().solution, p.solution);
    EXPECT_EQ(restored.puzzle().right, p.right);
    EXPECT_EQ(restored.puzzle().down, p.down);

    // A corrupt save falls back to a fresh puzzle.
    std::string broken = save;
    broken[broken.size() / 2] = static_cast<char>(0x7f);
    KakuroScene fallback(fonts);
    fallback.start(broken, {});
    EXPECT_TRUE(well_formed(fallback.puzzle()));

    // Fill every white cell with the solution.
    for (int i = 0; i < n * n; ++i)
    {
        if (!p.white(i) ||
            scene.digits()[static_cast<std::size_t>(i)] == p.solution[static_cast<std::size_t>(i)])
            continue;
        walk_to(scene, i, cues);
        enter_digit(scene, p.solution[static_cast<std::size_t>(i)], cues);
    }
    EXPECT_TRUE(scene.is_solved());
    EXPECT_NE(std::find(cues.begin(), cues.end(), audio::Cue::complete), cues.end());
    EXPECT_FALSE(scene.in_progress());
    for (int frame = 0; frame < 120; ++frame)
        scene.update(InputFrame{}, 1.0f / 60.0f, cues);
    EXPECT_TRUE(scene.menu_open()); // the solved menu follows the flourish
}

TEST(KakuroScene, CircleOpensPauseAndSizesStartNewPuzzles)
{
    ui::Fonts fonts;
    KakuroScene scene(fonts);
    scene.start({}, {});
    std::vector<audio::Cue> cues;
    scene.update(press(Action::back), 0.016f, cues);
    EXPECT_TRUE(scene.menu_open());
    scene.update(press(Action::confirm), 0.016f, cues); // Resume
    EXPECT_FALSE(scene.menu_open());
    scene.new_puzzle(5, 2);
    EXPECT_EQ(scene.puzzle().side, 10);
    EXPECT_EQ(scene.size(), 2);
    scene.new_puzzle(5, 1);
    EXPECT_EQ(scene.puzzle().side, 8);
}
