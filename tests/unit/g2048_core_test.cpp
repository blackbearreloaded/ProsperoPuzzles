// ProsperoPuzzles - 2048 rules tests (from the ps5-2048 WIP suite, extended).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/g2048/core.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>

namespace
{

using ppz::g2048::Direction;
using ppz::g2048::Game;
using ppz::g2048::kCells;

Game board(std::array<std::uint8_t, kCells> cells)
{
    Game result{7};
    auto state = result.snapshot();
    state.cells = cells;
    state.random_state = 7;
    EXPECT_TRUE(result.restore(state));
    return result;
}

TEST(G2048, MergesOnceAndScoresTheResult)
{
    auto g = board({1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    const auto move = g.move(Direction::left);
    EXPECT_TRUE(move.changed);
    EXPECT_EQ(move.score_gained, 8u);
    EXPECT_EQ(move.tile_count, 4);
    EXPECT_EQ(g.snapshot().cells[0], 2);
    EXPECT_EQ(g.snapshot().cells[1], 2);

    g = board({1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    EXPECT_EQ(g.move(Direction::left).score_gained, 4u);
    EXPECT_EQ(g.snapshot().cells[0], 2);
    EXPECT_EQ(g.snapshot().cells[1], 2);

    g = board({1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    EXPECT_EQ(g.move(Direction::left).score_gained, 4u);
    EXPECT_EQ(g.snapshot().cells[0], 2);
    EXPECT_EQ(g.snapshot().cells[1], 1);
}

TEST(G2048, EveryDirectionUsesEdgeFirstOrder)
{
    auto up = board({1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    EXPECT_EQ(up.move(Direction::up).score_gained, 4u);
    EXPECT_EQ(up.snapshot().cells[0], 2);
    auto down = board({0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0});
    EXPECT_EQ(down.move(Direction::down).score_gained, 4u);
    EXPECT_EQ(down.snapshot().cells[15], 2);
    auto right = board({1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    EXPECT_EQ(right.move(Direction::right).score_gained, 4u);
    EXPECT_EQ(right.snapshot().cells[3], 2);
}

TEST(G2048, InvalidMovesAndWinPausePreserveState)
{
    auto g = board({1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    const auto before = g.snapshot();
    EXPECT_FALSE(g.move(Direction::left).changed);
    EXPECT_EQ(g.snapshot().cells, before.cells);
    EXPECT_EQ(g.snapshot().random_state, before.random_state);

    g = board({10, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    const auto win = g.move(Direction::left);
    EXPECT_TRUE(win.first_win);
    EXPECT_TRUE(g.snapshot().won);
    EXPECT_EQ(win.score_gained, 2048u);
    const auto won = g.snapshot();
    EXPECT_FALSE(g.move(Direction::right).changed);
    EXPECT_EQ(g.snapshot().random_state, won.random_state);
    g.keep_playing();
    EXPECT_TRUE(g.move(Direction::right).changed);
}

TEST(G2048, GameOverAndReplayAreDeterministic)
{
    auto g = board({1, 2, 1, 2, 2, 1, 2, 1, 1, 2, 1, 2, 2, 1, 2, 1});
    EXPECT_FALSE(g.moves_available());
    Game a{1234};
    Game b{1234};
    for (Direction move : {Direction::left, Direction::up, Direction::left, Direction::down,
                           Direction::right, Direction::up, Direction::right, Direction::down})
    {
        a.move(move);
        b.move(move);
        EXPECT_EQ(a.snapshot().cells, b.snapshot().cells);
        EXPECT_EQ(a.snapshot().score, b.snapshot().score);
        EXPECT_EQ(a.snapshot().random_state, b.snapshot().random_state);
    }
    auto bad = a.snapshot();
    bad.cells[0] = 64;
    EXPECT_FALSE(a.restore(bad));
}

TEST(G2048, WinningAndLosingOnTheSameMoveReportsBoth)
{
    // The final merge makes 2048 and leaves no moves: the scene must show the
    // win and then game over (the WIP left a dead board).
    auto g = board({10, 10, 3, 4, 3, 4, 5, 6, 5, 6, 7, 8, 7, 8, 9, 3});
    const auto move = g.move(Direction::left);
    EXPECT_TRUE(move.first_win);
    if (move.game_over)
    {
        g.keep_playing();
        EXPECT_FALSE(g.move(Direction::up).changed);
        EXPECT_TRUE(g.snapshot().game_over);
    }
}

TEST(G2048, SpawnsFoursAboutOneTimeInTen)
{
    Game g{99};
    int fours = 0;
    int total = 0;
    for (int round = 0; round < 3000; ++round)
    {
        g.new_game(static_cast<std::uint64_t>(round) + 1);
        for (std::uint8_t exponent : g.snapshot().cells)
        {
            if (exponent != 0)
            {
                ++total;
                fours += exponent == 2 ? 1 : 0;
            }
        }
    }
    EXPECT_NEAR(static_cast<double>(fours) / total, 0.1, 0.02);
}

TEST(G2048, ScoreSaturatesInsteadOfWrapping)
{
    auto g = board({62, 62, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    auto state = g.snapshot();
    state.score = UINT64_MAX - 10;
    state.best = UINT64_MAX - 10;
    ASSERT_TRUE(g.restore(state));
    g.move(Direction::left);
    EXPECT_EQ(g.snapshot().score, UINT64_MAX);
}

} // namespace
