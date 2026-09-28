// ProsperoPuzzles - Tenfold game rules (imported from the ps5-tenfold WIP project).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace ppz::tenfold
{
constexpr std::size_t kSide = 6;
constexpr std::size_t kCells = kSide * kSide;

enum class Direction : std::uint8_t
{
    up,
    right,
    down,
    left
};

struct Group
{
    std::array<std::uint8_t, kCells> cells{};
    std::uint8_t count = 0;

    bool contains(std::uint8_t cell) const
    {
        for (std::uint8_t i = 0; i < count; ++i)
            if (cells[i] == cell)
                return true;
        return false;
    }
};

struct TileMotion
{
    std::int8_t from_row = 0;
    bool spawned = false;
};

struct MoveResult
{
    std::array<TileMotion, kCells> motion{};
    std::uint8_t destination = 0xff;
    std::uint8_t value = 0;
    std::uint8_t count = 0;
    bool changed = false;
    bool first_ten = false;
    bool game_over = false;
};

struct Snapshot
{
    std::array<std::uint8_t, kCells> cells{};
    std::uint64_t score = 0;
    std::uint64_t best = 0;
    std::uint64_t random_state = 0;
    std::uint32_t moves = 0;
    std::uint8_t highest = 1;
    bool won = false;
    bool game_over = false;
};

class Game
{
  public:
    explicit Game(std::uint64_t seed = UINT64_C(0x74656e666f6c64))
    {
        new_game(seed);
    }

    void new_game(std::uint64_t seed)
    {
        const auto best = state_.best;
        state_ = {};
        state_.best = best;
        state_.random_state = seed ? seed : 1;
        for (auto &cell : state_.cells)
            cell = random_tile(3);
        if (!moves_available())
            state_.cells[0] = state_.cells[1];
        state_.highest = *std::max_element(state_.cells.begin(), state_.cells.end());
        can_undo_ = false;
    }

    const Snapshot &snapshot() const
    {
        return state_;
    }

    // ProsperoPuzzles: resumes a saved game. Cells must hold 1..254 and the
    // flags must agree with the board; undo history is not restored.
    bool restore(const Snapshot &candidate)
    {
        std::uint8_t highest = 0;
        for (std::uint8_t value : candidate.cells)
        {
            if (value == 0 || value == std::numeric_limits<std::uint8_t>::max())
                return false;
            highest = std::max(highest, value);
        }
        if (candidate.random_state == 0 || candidate.best < candidate.score ||
            candidate.highest < highest)
            return false;
        const Snapshot keep = state_;
        state_ = candidate;
        if (state_.game_over != !moves_available())
        {
            state_ = keep;
            return false;
        }
        can_undo_ = false;
        return true;
    }

    void set_best(std::uint64_t best)
    {
        state_.best = std::max({state_.best, state_.score, best});
    }

    bool can_undo() const
    {
        return can_undo_;
    }

    bool undo()
    {
        if (!can_undo_)
            return false;
        const auto best = state_.best;
        state_ = previous_;
        state_.best = std::max(state_.best, best);
        can_undo_ = false;
        return true;
    }

    Group group_at(std::uint8_t start) const
    {
        Group group{};
        if (start >= kCells)
            return group;
        std::array<bool, kCells> seen{};
        std::array<std::uint8_t, kCells> pending{};
        std::uint8_t waiting = 1;
        pending[0] = start;
        seen[start] = true;
        const auto value = state_.cells[start];
        while (waiting)
        {
            const auto cell = pending[--waiting];
            group.cells[group.count++] = cell;
            const auto row = cell / kSide, col = cell % kSide;
            const auto visit = [&](std::uint8_t next)
            {
                if (!seen[next] && state_.cells[next] == value)
                {
                    seen[next] = true;
                    pending[waiting++] = next;
                }
            };
            if (row > 0)
                visit(static_cast<std::uint8_t>(cell - kSide));
            if (row + 1 < kSide)
                visit(static_cast<std::uint8_t>(cell + kSide));
            if (col > 0)
                visit(static_cast<std::uint8_t>(cell - 1));
            if (col + 1 < kSide)
                visit(static_cast<std::uint8_t>(cell + 1));
        }
        return group;
    }

    bool moves_available() const
    {
        for (std::size_t i = 0; i < kCells; ++i)
        {
            if (i % kSide + 1 < kSide && state_.cells[i] == state_.cells[i + 1])
                return true;
            if (i + kSide < kCells && state_.cells[i] == state_.cells[i + kSide])
                return true;
        }
        return false;
    }

    MoveResult merge(std::uint8_t selected, std::uint8_t anchor)
    {
        MoveResult result{};
        const auto group = group_at(selected);
        if (group.count < 2 || !group.contains(anchor) || state_.game_over ||
            state_.cells[anchor] == std::numeric_limits<std::uint8_t>::max())
            return result;

        previous_ = state_;
        can_undo_ = true;
        result.changed = true;
        result.value = static_cast<std::uint8_t>(state_.cells[anchor] + 1);
        result.count = group.count;
        for (std::uint8_t i = 0; i < group.count; ++i)
            state_.cells[group.cells[i]] = 0;
        state_.cells[anchor] = result.value;
        state_.highest = std::max(state_.highest, result.value);

        for (std::uint8_t col = 0; col < kSide; ++col)
        {
            struct Kept
            {
                std::uint8_t value, row;
                bool merged;
            };
            std::array<Kept, kSide> kept{};
            std::uint8_t count = 0;
            for (int row = static_cast<int>(kSide) - 1; row >= 0; --row)
            {
                const auto index = static_cast<std::uint8_t>(row * kSide + col);
                if (state_.cells[index])
                    kept[count++] = {state_.cells[index], static_cast<std::uint8_t>(row),
                                     index == anchor};
            }
            for (int row = static_cast<int>(kSide) - 1; row >= 0; --row)
            {
                const auto index = static_cast<std::uint8_t>(row * kSide + col);
                const auto offset = static_cast<std::uint8_t>(kSide - 1 - row);
                if (offset < count)
                {
                    const auto piece = kept[offset];
                    state_.cells[index] = piece.value;
                    result.motion[index] = {static_cast<std::int8_t>(piece.row), false};
                    if (piece.merged)
                        result.destination = index;
                }
                else
                {
                    state_.cells[index] = random_tile(state_.highest);
                    result.motion[index] = {
                        static_cast<std::int8_t>(row - static_cast<int>(kSide - count)), true};
                }
            }
        }

        const std::uint64_t gained = std::uint64_t{result.value} * group.count * 10;
        state_.score = std::numeric_limits<std::uint64_t>::max() - state_.score < gained
                           ? std::numeric_limits<std::uint64_t>::max()
                           : state_.score + gained;
        state_.best = std::max(state_.best, state_.score);
        ++state_.moves;
        result.first_ten = state_.highest >= 10 && !state_.won;
        state_.won |= result.first_ten;
        state_.game_over = !moves_available();
        result.game_over = state_.game_over;
        return result;
    }

  private:
    Snapshot state_{};
    Snapshot previous_{};
    bool can_undo_ = false;

    std::uint32_t random_below(std::uint32_t bound)
    {
        std::uint64_t value = (state_.random_state += UINT64_C(0x9e3779b97f4a7c15));
        value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
        return static_cast<std::uint32_t>((value ^ (value >> 31)) >> 32) % bound;
    }

    std::uint8_t random_tile(std::uint8_t highest)
    {
        const auto limit = std::min(5, std::max(3, static_cast<int>(highest) - 2));
        return static_cast<std::uint8_t>(random_below(limit) + 1);
    }
};
} // namespace ppz::tenfold
