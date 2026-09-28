// ProsperoPuzzles - Tenfold rules tests (from the ps5-tenfold WIP check, extended).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/tenfold/core.hpp"

#include <gtest/gtest.h>

#include <cstdint>

namespace
{

using ppz::tenfold::Game;
using ppz::tenfold::kCells;
using ppz::tenfold::kSide;

TEST(Tenfold, MergeScoresRefillsAndUndoes)
{
    Game board{123};
    ASSERT_TRUE(board.moves_available());
    const auto before = board.snapshot();
    for (std::uint8_t cell = 0; cell < kCells; ++cell)
    {
        const auto group = board.group_at(cell);
        if (group.count < 2)
            continue;
        const auto anchor = group.cells[group.count - 1];
        const auto result = board.merge(cell, anchor);
        ASSERT_TRUE(result.changed);
        EXPECT_EQ(result.count, group.count);
        EXPECT_EQ(result.value, before.cells[anchor] + 1);
        ASSERT_LT(result.destination, kCells);
        EXPECT_EQ(board.snapshot().cells[result.destination], result.value);
        EXPECT_EQ(result.motion[result.destination].from_row, anchor / kSide);
        EXPECT_EQ(board.snapshot().score, std::uint64_t{result.value} * group.count * 10);
        EXPECT_EQ(board.snapshot().moves, 1u);
        for (auto value : board.snapshot().cells)
            EXPECT_GE(value, 1);
        const auto best = board.snapshot().best;
        ASSERT_TRUE(board.undo());
        EXPECT_EQ(board.snapshot().cells, before.cells);
        EXPECT_EQ(board.snapshot().moves, 0u);
        EXPECT_EQ(board.snapshot().best, best);
        EXPECT_FALSE(board.undo());
        return;
    }
    FAIL() << "no group of two in a fresh board";
}

TEST(Tenfold, RejectsSinglesAndAnchorsOutsideTheGroup)
{
    Game board{5};
    for (std::uint8_t cell = 0; cell < kCells; ++cell)
    {
        const auto group = board.group_at(cell);
        if (group.count == 1)
            EXPECT_FALSE(board.merge(cell, cell).changed);
        else
        {
            for (std::uint8_t other = 0; other < kCells; ++other)
            {
                if (!group.contains(other))
                {
                    EXPECT_FALSE(board.merge(cell, other).changed);
                    break;
                }
            }
        }
    }
}

TEST(Tenfold, GravityKeepsColumnsFullAndSpawnsWithinRange)
{
    Game board{77};
    for (int step = 0; step < 200 && board.moves_available(); ++step)
    {
        for (std::uint8_t cell = 0; cell < kCells; ++cell)
        {
            const auto group = board.group_at(cell);
            if (group.count >= 2)
            {
                const auto result = board.merge(cell, group.cells[0]);
                const auto &state = board.snapshot();
                const int limit = std::min(5, std::max(3, static_cast<int>(state.highest) - 2));
                for (std::uint8_t i = 0; i < kCells; ++i)
                {
                    EXPECT_GE(state.cells[i], 1);
                    if (result.motion[i].spawned)
                    {
                        EXPECT_LE(state.cells[i], limit);
                        EXPECT_LT(result.motion[i].from_row, 0);
                    }
                }
                break;
            }
        }
    }
}

TEST(Tenfold, ReachingTenWinsOnceAndGameOverWhenStuck)
{
    Game board{1};
    auto state = board.snapshot();
    // A checkerboard of 1/2 with one pair of 9s: merging makes the first 10.
    for (std::uint8_t i = 0; i < kCells; ++i)
        state.cells[i] = static_cast<std::uint8_t>(((i / kSide + i % kSide) % 2) + 1);
    state.cells[0] = 9;
    state.cells[1] = 9;
    state.highest = 9;
    state.game_over = false;
    ASSERT_TRUE(board.restore(state));
    const auto result = board.merge(0, 1);
    ASSERT_TRUE(result.changed);
    EXPECT_TRUE(result.first_ten);
    EXPECT_TRUE(board.snapshot().won);
    EXPECT_EQ(result.game_over, !board.moves_available());
}

TEST(Tenfold, RestoreValidatesBoards)
{
    Game board{3};
    auto state = board.snapshot();
    auto bad = state;
    bad.cells[4] = 0;
    EXPECT_FALSE(board.restore(bad));
    bad = state;
    bad.random_state = 0;
    EXPECT_FALSE(board.restore(bad));
    bad = state;
    bad.game_over = !bad.game_over; // inconsistent with the board
    EXPECT_FALSE(board.restore(bad));
    EXPECT_TRUE(board.restore(state));
    EXPECT_FALSE(board.can_undo());
}

} // namespace
