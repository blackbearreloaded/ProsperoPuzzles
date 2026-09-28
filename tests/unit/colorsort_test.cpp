// ProsperoPuzzles - Color Sort rules, generator and play screen tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/colorsort/colorsort_scene.hpp"
#include "games/colorsort/core.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>

using namespace ppz;
using namespace ppz::colorsort;

namespace
{

InputFrame press(Action action)
{
    InputFrame f;
    f.pressed = action_bit(action);
    return f;
}

// A board from rows of ball colours (bottom first), one row per tube.
Board make_board(int colours, std::initializer_list<std::initializer_list<int>> tubes)
{
    Board b;
    b.colours = colours;
    b.tubes = static_cast<int>(tubes.size());
    int t = 0;
    for (const auto &tube : tubes)
    {
        for (int colour : tube)
            b.ball[static_cast<std::size_t>(t)][b.count[static_cast<std::size_t>(t)]++] =
                static_cast<std::uint8_t>(colour);
        ++t;
    }
    return b;
}

// Moves the scene's cursor onto a tube.
void go_to(ColorSortScene &scene, int tube, std::vector<audio::Cue> &cues)
{
    const int cols = scene.columns();
    const int col = tube % cols;
    const int row = tube / cols;
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

TEST(ColorSort, GeneratedBoardsAreShuffledAndSolvable)
{
    for (int colours : {4, 6, 8})
    {
        double worst_ms = 0.0;
        for (std::uint64_t seed = 1; seed <= 8; ++seed)
        {
            const auto begin = std::chrono::steady_clock::now();
            const Board b = generate(seed * 7919 + static_cast<std::uint64_t>(colours), colours);
            worst_ms = std::max(worst_ms, std::chrono::duration<double, std::milli>(
                                              std::chrono::steady_clock::now() - begin)
                                              .count());
            ASSERT_EQ(b.colours, colours);
            ASSERT_EQ(b.tubes, colours + kEmptyTubes);
            EXPECT_TRUE(valid(b));
            for (int t = 0; t < b.tubes; ++t)
            {
                EXPECT_EQ(b.count[static_cast<std::size_t>(t)], t < colours ? kCapacity : 0);
                EXPECT_FALSE(tube_complete(b, t));
            }
            EXPECT_EQ(sorted_count(b), 0);
            EXPECT_FALSE(solved(b));

            // The solver's moves are legal and finish the board.
            std::vector<Move> path;
            ASSERT_EQ(solve(b, kSolveBudget, &path), Verdict::solvable);
            Board play = b;
            for (const Move &m : path)
                ASSERT_GT(pour(play, m.from, m.to), 0);
            EXPECT_TRUE(solved(play));
            EXPECT_EQ(sorted_count(play), colours);
        }
        std::printf("[ colorsort ] %d colours: worst generation %.2f ms\n", colours, worst_ms);
        // The same seed gives the same board.
        const Board a = generate(77, colours);
        const Board c = generate(77, colours);
        EXPECT_EQ(a.ball, c.ball);
    }
}

TEST(ColorSort, PoursMoveTheTopRunAsFarAsItFits)
{
    // Tube 0: A A B B (top B B); tube 1: C B; tube 2: empty; tube 3: A C C C.
    Board b = make_board(3, {{0, 0, 1, 1}, {2, 1}, {}, {0, 2, 2, 2}});
    EXPECT_EQ(top_run(b, 0), 2);
    EXPECT_EQ(pour_amount(b, 0, 1), 2); // B B onto B, two free slots
    EXPECT_EQ(pour_amount(b, 0, 3), 0); // full target
    EXPECT_EQ(pour_amount(b, 1, 3), 0); // full target
    EXPECT_EQ(pour_amount(b, 3, 1), 0); // C onto B
    EXPECT_EQ(pour_amount(b, 3, 2), 3); // the whole run into the empty tube
    EXPECT_EQ(pour_amount(b, 2, 0), 0); // nothing to pour
    EXPECT_EQ(pour_amount(b, 0, 0), 0); // onto itself
    EXPECT_EQ(pour_amount(b, 0, 9), 0); // no such tube

    // Tube 1 has room for only one more after a partial pour.
    Board partial = make_board(2, {{0, 1, 1, 1}, {0, 0, 0}, {1}});
    EXPECT_EQ(pour(partial, 0, 2), 3);
    EXPECT_EQ(partial.count[2], 4);
    EXPECT_TRUE(tube_complete(partial, 2));
    Board limited = make_board(2, {{1, 0, 0, 0}, {1, 1, 0}, {}});
    EXPECT_EQ(pour(limited, 0, 1), 1); // three zeros, room for one
    EXPECT_EQ(limited.count[0], 3);
    EXPECT_EQ(top_run(limited, 0), 2);

    Board done = make_board(2, {{0, 0, 0, 0}, {}, {1, 1, 1, 1}, {}});
    EXPECT_TRUE(solved(done));
    EXPECT_EQ(sorted_count(done), 2);
    EXPECT_TRUE(valid(make_board(2, {{0, 0, 0, 0}, {1, 1, 1, 1}, {}, {}})));
    EXPECT_FALSE(valid(make_board(2, {{0, 0, 0, 0}, {1, 1, 1}, {}, {}})));
}

TEST(ColorSort, SolverRecognisesDeadEnds)
{
    // Two colours interleaved with no spare room: nothing can move.
    const Board stuck = make_board(2, {{0, 1, 0, 1}, {1, 0, 1, 0}});
    EXPECT_EQ(solve(stuck, kSolveBudget), Verdict::unsolvable);
    const Board easy = make_board(2, {{0, 0, 0, 1}, {1, 1, 1, 0}, {}, {}});
    std::vector<Move> path;
    EXPECT_EQ(solve(easy, kSolveBudget, &path), Verdict::solvable);
    EXPECT_FALSE(path.empty());
}

TEST(ColorSortScene, PlaysUndoesSavesAndSolves)
{
    ui::Fonts fonts;
    ColorSortScene scene(fonts);
    scene.start({}, {});
    scene.new_puzzle(99, 0);
    std::vector<audio::Cue> cues;
    const Board initial = scene.board();
    std::vector<Move> path;
    ASSERT_EQ(solve(initial, kSolveBudget, &path), Verdict::solvable);
    ASSERT_FALSE(path.empty());

    // Lift, then put back with Circle (which must not open the pause menu).
    go_to(scene, path[0].from, cues);
    scene.update(press(Action::confirm), 0.016f, cues);
    EXPECT_EQ(scene.lifted(), path[0].from);
    EXPECT_EQ(cues.back(), audio::Cue::pickup);
    scene.update(press(Action::back), 0.016f, cues);
    EXPECT_EQ(scene.lifted(), -1);
    EXPECT_FALSE(scene.menu_open());
    EXPECT_FALSE(scene.in_progress());

    // Lifting an empty tube is rejected.
    go_to(scene, initial.tubes - 1, cues);
    scene.update(press(Action::confirm), 0.016f, cues);
    EXPECT_EQ(scene.lifted(), -1);
    EXPECT_EQ(cues.back(), audio::Cue::invalid);

    // A pour onto a mismatched tube is rejected and keeps the selection.
    int bad_to = -1;
    for (int t = 0; t < initial.tubes && bad_to < 0; ++t)
        if (t != path[0].from && pour_amount(initial, path[0].from, t) == 0)
            bad_to = t;
    ASSERT_GE(bad_to, 0);
    go_to(scene, path[0].from, cues);
    scene.update(press(Action::confirm), 0.016f, cues);
    go_to(scene, bad_to, cues);
    scene.update(press(Action::confirm), 0.016f, cues);
    EXPECT_EQ(cues.back(), audio::Cue::invalid);
    EXPECT_EQ(scene.lifted(), path[0].from);
    EXPECT_EQ(scene.board().ball, initial.ball);

    // The first solution move, then undo and redo it.
    go_to(scene, path[0].to, cues);
    scene.update(press(Action::confirm), 0.016f, cues);
    EXPECT_EQ(cues.back(), audio::Cue::drop);
    EXPECT_EQ(scene.lifted(), -1);
    Board after = initial;
    pour(after, path[0].from, path[0].to);
    EXPECT_EQ(scene.board().count, after.count);
    EXPECT_TRUE(scene.in_progress());
    scene.update(press(Action::page_prev), 0.016f, cues);
    EXPECT_EQ(scene.board().count, initial.count);
    scene.update(press(Action::page_next), 0.016f, cues);
    EXPECT_EQ(scene.board().count, after.count);
    for (int frame = 0; frame < 40; ++frame)
        scene.update(InputFrame{}, 1.0f / 60.0f, cues);

    // The save restores the same board.
    ColorSortScene restored(fonts);
    restored.start(scene.save(), scene.stats());
    EXPECT_EQ(restored.board().ball, scene.board().ball);
    EXPECT_EQ(restored.board().count, scene.board().count);
    EXPECT_EQ(restored.size(), 0);
    ColorSortScene broken(fonts);
    std::string corrupt = scene.save();
    corrupt.resize(corrupt.size() / 2);
    broken.start(corrupt, {}); // falls back to a fresh puzzle
    EXPECT_TRUE(valid(broken.board()));

    // Play the rest of the solution.
    for (std::size_t i = 1; i < path.size(); ++i)
    {
        go_to(scene, path[i].from, cues);
        scene.update(press(Action::confirm), 0.016f, cues);
        go_to(scene, path[i].to, cues);
        scene.update(press(Action::confirm), 0.016f, cues);
        for (int frame = 0; frame < 3; ++frame)
            scene.update(InputFrame{}, 1.0f / 60.0f, cues);
    }
    EXPECT_TRUE(scene.is_solved());
    EXPECT_NE(std::find(cues.begin(), cues.end(), audio::Cue::complete), cues.end());
    EXPECT_FALSE(scene.in_progress());
    for (int frame = 0; frame < 120; ++frame)
        scene.update(InputFrame{}, 1.0f / 60.0f, cues);
    EXPECT_TRUE(scene.menu_open()); // the solved menu follows the flourish
}

TEST(ColorSortScene, CircleOpensPauseAndSizesUseTwoRows)
{
    ui::Fonts fonts;
    ColorSortScene scene(fonts);
    scene.start({}, {});
    std::vector<audio::Cue> cues;
    scene.update(press(Action::back), 0.016f, cues);
    EXPECT_TRUE(scene.menu_open());
    scene.update(press(Action::confirm), 0.016f, cues); // Resume
    EXPECT_FALSE(scene.menu_open());
    scene.new_puzzle(5, 0);
    EXPECT_EQ(scene.board().tubes, 6);
    EXPECT_EQ(scene.columns(), 6);
    scene.new_puzzle(5, 2);
    EXPECT_EQ(scene.board().colours, 8);
    EXPECT_EQ(scene.columns(), 5);
    EXPECT_EQ(scene.size(), 2);
    // Two rows of five: the cursor moves between them.
    EXPECT_EQ(scene.cursor_row(), 1);
    InputFrame up;
    up.nav = Direction::up;
    scene.update(up, 0.016f, cues);
    EXPECT_EQ(scene.cursor_row(), 0);
}
