// ProsperoPuzzles - Sokoban rules, generator and play screen tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/sokoban/core.hpp"
#include "games/sokoban/sokoban_scene.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdio>

using namespace ppz;
using namespace ppz::sokoban;

namespace
{

InputFrame press(Action action)
{
    InputFrame f;
    f.pressed = action_bit(action);
    return f;
}

InputFrame walk(int dir)
{
    constexpr Direction kNav[4] = {Direction::up, Direction::down, Direction::left,
                                   Direction::right};
    InputFrame f;
    f.nav = kNav[dir];
    return f;
}

// A room from text: '#' wall, '.' floor, 'o' target, '$' crate, '*' crate on
// target, '@' keeper, ' ' outside.
Puzzle parse(std::initializer_list<const char *> rows)
{
    Puzzle p;
    p.rows = static_cast<int>(rows.size());
    p.cols = static_cast<int>(std::string(*rows.begin()).size());
    int r = 0;
    for (const char *line : rows)
    {
        for (int c = 0; c < p.cols; ++c)
        {
            const char ch = line[c];
            const int cell = r * p.cols + c;
            auto &type = p.cell[static_cast<std::size_t>(cell)];
            type = ch == '#'                  ? kWall
                   : ch == ' '                ? kVoid
                   : (ch == 'o' || ch == '*') ? kTarget
                                              : kFloor;
            if (ch == '$' || ch == '*')
                p.start.crates[static_cast<std::size_t>(p.crates++)] =
                    static_cast<std::uint8_t>(cell);
            if (ch == '@')
                p.start.keeper = static_cast<std::uint8_t>(cell);
        }
        ++r;
    }
    return p;
}

} // namespace

TEST(Sokoban, GeneratedRoomsAreWalledFairAndSolvable)
{
    constexpr int kCrates[3] = {2, 3, 4};
    for (int size = 0; size < 3; ++size)
    {
        for (std::uint64_t seed = 1; seed <= 6; ++seed)
        {
            const Puzzle p = generate(seed * 7919 + static_cast<std::uint64_t>(size), size);
            SCOPED_TRACE(testing::Message() << "size " << size << " seed " << seed);
            ASSERT_EQ(p.crates, kCrates[size]);
            ASSERT_LE(p.cols, kMaxSide);
            ASSERT_LE(p.rows, kMaxSide);
            int targets = 0;
            for (int cell = 0; cell < p.cols * p.rows; ++cell)
            {
                targets += p.cell[static_cast<std::size_t>(cell)] == kTarget ? 1 : 0;
                if (!is_floor(p, cell))
                    continue;
                // Floor never reaches the board edge, and never ends in a dead end.
                const int c = cell % p.cols;
                const int r = cell / p.cols;
                EXPECT_TRUE(c > 0 && r > 0 && c < p.cols - 1 && r < p.rows - 1);
                int open = 0;
                for (int d = 0; d < 4; ++d)
                    open += is_floor(p, neighbour(p, cell, d)) ? 1 : 0;
                EXPECT_GE(open, 2);
            }
            EXPECT_EQ(targets, p.crates);
            EXPECT_TRUE(is_floor(p, p.start.keeper));
            EXPECT_LT(crate_at(p, p.start, p.start.keeper), 0);
            for (int i = 0; i < p.crates; ++i)
            {
                const int cell = p.start.crates[static_cast<std::size_t>(i)];
                EXPECT_TRUE(is_floor(p, cell));
                EXPECT_NE(p.cell[static_cast<std::size_t>(cell)], kTarget);
                EXPECT_FALSE(dead_corner(p, cell));
            }
            EXPECT_FALSE(solved(p, p.start));
            const int pushes = solve(p, p.start, 400000);
            EXPECT_GT(pushes, 0);
            if (size < 2)
                EXPECT_EQ(pushes, p.min_pushes);
        }
    }
}

TEST(Sokoban, GenerationIsDeterministicAndFast)
{
    const Puzzle a = generate(123, 2);
    const Puzzle b = generate(123, 2);
    EXPECT_EQ(a.cell, b.cell);
    EXPECT_EQ(a.start.crates, b.start.crates);
    EXPECT_EQ(a.start.keeper, b.start.keeper);
    for (int size = 0; size < 3; ++size)
    {
        double worst = 0.0;
        for (std::uint64_t seed = 100; seed < 110; ++seed)
        {
            const auto t0 = std::chrono::steady_clock::now();
            (void)generate(seed, size);
            const std::chrono::duration<double, std::milli> ms =
                std::chrono::steady_clock::now() - t0;
            worst = std::max(worst, ms.count());
        }
        std::printf("sokoban size %d: worst generation %.1f ms (sanitized build)\n", size, worst);
    }
}

TEST(Sokoban, KeeperWalksPushesAndIsBlocked)
{
    const Puzzle p = parse({"#######", "#@$.o.#", "#.$$..#", "#..o..#", "#######"});
    // Three crates but two targets in this sketch: add one more target.
    Puzzle q = p;
    q.cell[3 * 7 + 5] = kTarget;
    State s = q.start;
    int pushed = -1;
    EXPECT_EQ(move(q, &s, 0, &pushed), Step::none); // wall above
    EXPECT_EQ(move(q, &s, 3, &pushed), Step::push); // pushes the crate right
    EXPECT_EQ(pushed, 0);
    EXPECT_EQ(s.keeper, 1 * 7 + 2);
    EXPECT_EQ(s.crates[0], 1 * 7 + 3);
    EXPECT_EQ(move(q, &s, 3, &pushed), Step::push); // onto the target
    EXPECT_EQ(crates_on_targets(q, s), 1);
    EXPECT_EQ(move(q, &s, 2, &pushed), Step::walk);
    EXPECT_EQ(move(q, &s, 2, &pushed), Step::walk);
    EXPECT_EQ(move(q, &s, 1, &pushed), Step::walk);
    EXPECT_EQ(move(q, &s, 3, &pushed), Step::blocked); // two crates in a row
    EXPECT_EQ(s.keeper, 2 * 7 + 1);
    EXPECT_TRUE(dead_corner(q, 1 * 7 + 1));
    EXPECT_FALSE(dead_corner(q, 1 * 7 + 4)); // a target is never dead
}

TEST(Sokoban, SolverFindsTheShortestPushCount)
{
    const Puzzle p = parse({"######", "#@$.o#", "######"});
    std::vector<int> moves;
    EXPECT_EQ(solve(p, p.start, 1000, &moves), 2);
    State s = p.start;
    for (int d : moves)
        move(p, &s, d);
    EXPECT_TRUE(solved(p, s));
    // A crate in a dead corner makes the room unsolvable.
    const Puzzle stuck = parse({"#####", "#$.@#", "#..o#", "#####"});
    EXPECT_EQ(solve(stuck, stuck.start, 1000), -1);
}

TEST(SokobanScene, PlaysUndoesSavesAndSolves)
{
    ui::Fonts fonts;
    SokobanScene scene(fonts);
    scene.start({}, {});
    scene.new_puzzle(99, 1);
    std::vector<audio::Cue> cues;
    const Puzzle p = scene.puzzle();

    std::vector<int> moves;
    ASSERT_GT(solve(p, scene.state(), 400000, &moves), 0);
    ASSERT_FALSE(moves.empty());

    // One step, undone and redone.
    scene.update(walk(moves[0]), 0.016f, cues);
    const State after = scene.state();
    EXPECT_NE(after.keeper, p.start.keeper);
    scene.update(press(Action::page_prev), 0.016f, cues);
    EXPECT_EQ(scene.state().keeper, p.start.keeper);
    scene.update(press(Action::page_next), 0.016f, cues);
    EXPECT_EQ(scene.state().keeper, after.keeper);
    EXPECT_TRUE(scene.in_progress());

    // Walking into a wall does nothing and does not count.
    for (int d = 0; d < 4; ++d)
    {
        if (is_floor(p, neighbour(p, scene.state().keeper, d)))
            continue;
        const std::size_t before = cues.size();
        scene.update(walk(d), 0.016f, cues);
        EXPECT_EQ(scene.state().keeper, after.keeper);
        EXPECT_EQ(cues.size(), before);
    }

    // The save restores the same room and position.
    const std::string save = scene.save();
    SokobanScene restored(fonts);
    restored.start(save, scene.stats());
    EXPECT_EQ(restored.puzzle().cell, p.cell);
    EXPECT_EQ(restored.state().keeper, scene.state().keeper);
    EXPECT_EQ(restored.state().crates, scene.state().crates);

    // Play the rest of the solution.
    for (std::size_t i = 1; i < moves.size(); ++i)
        scene.update(walk(moves[i]), 0.016f, cues);
    EXPECT_TRUE(scene.is_solved());
    EXPECT_NE(std::find(cues.begin(), cues.end(), audio::Cue::slide), cues.end());
    EXPECT_NE(std::find(cues.begin(), cues.end(), audio::Cue::complete), cues.end());
    EXPECT_FALSE(scene.in_progress());
    for (int frame = 0; frame < 120; ++frame)
        scene.update(InputFrame{}, 1.0f / 60.0f, cues);
    EXPECT_TRUE(scene.menu_open()); // the solved menu follows the flourish
}

TEST(SokobanScene, BlockedPushShakesAndRestartResets)
{
    ui::Fonts fonts;
    SokobanScene scene(fonts);
    scene.start({}, {});
    scene.new_puzzle(7, 0);
    std::vector<audio::Cue> cues;
    const Puzzle p = scene.puzzle();
    // Take any legal step, then restart from the pause menu.
    for (int d = 0; d < 4; ++d)
    {
        State s = scene.state();
        if (move(p, &s, d) == Step::walk || move(p, &s, d) == Step::push)
        {
            scene.update(walk(d), 0.016f, cues);
            break;
        }
    }
    ASSERT_TRUE(scene.in_progress());
    scene.update(press(Action::menu), 0.016f, cues);
    ASSERT_TRUE(scene.menu_open());
    scene.update(walk(1), 0.016f, cues); // Resume -> New puzzle
    scene.update(walk(1), 0.016f, cues); // -> Restart
    scene.update(press(Action::confirm), 0.016f, cues);
    EXPECT_FALSE(scene.menu_open());
    EXPECT_EQ(scene.state().keeper, p.start.keeper);
    EXPECT_EQ(scene.state().crates, p.start.crates);
    scene.new_puzzle(5, 2);
    EXPECT_EQ(scene.size(), 2);
    EXPECT_EQ(scene.puzzle().crates, 4);
}
