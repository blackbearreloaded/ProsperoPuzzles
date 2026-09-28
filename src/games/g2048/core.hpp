// ProsperoPuzzles - 2048 game rules (imported from the ps5-2048 WIP project).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace ppz::g2048
{
constexpr std::size_t kSide = 4;
constexpr std::size_t kCells = kSide * kSide;
constexpr std::uint8_t kMaxExponent = 63;

enum class Direction : std::uint8_t
{
    up,
    right,
    down,
    left
};

struct TileTransition
{
    std::uint8_t from;
    std::uint8_t to;
    std::uint8_t from_exponent;
    std::uint8_t exponent;
    bool merged;
};

struct MoveResult
{
    std::array<TileTransition, kCells> tiles{};
    std::uint8_t tile_count = 0;
    std::uint8_t spawned_cell = 0xff;
    std::uint8_t spawned_exponent = 0;
    std::uint8_t highest_merge_exponent = 0;
    std::uint64_t score_gained = 0;
    bool changed = false;
    bool first_win = false;
    bool game_over = false;
};

struct Snapshot
{
    std::array<std::uint8_t, kCells> cells{};
    std::uint64_t score = 0;
    std::uint64_t best = 0;
    std::uint64_t random_state = 0;
    bool won = false;
    bool keep_playing = false;
    bool game_over = false;
};

class Game
{
  public:
    explicit Game(std::uint64_t seed = UINT64_C(0x2048c0ffee))
    {
        new_game(seed);
    }

    void new_game(std::uint64_t seed)
    {
        const std::uint64_t best = state_.best;
        state_ = {};
        state_.best = best;
        state_.random_state = seed ? seed : UINT64_C(0x9e3779b97f4a7c15);
        spawn();
        spawn();
    }

    const Snapshot &snapshot() const
    {
        return state_;
    }

    bool restore(const Snapshot &candidate)
    {
        bool has_tile = false;
        for (std::uint8_t exponent : candidate.cells)
        {
            if (exponent > kMaxExponent)
                return false;
            has_tile |= exponent != 0;
        }
        if ((!has_tile && !candidate.game_over) || candidate.best < candidate.score ||
            (!candidate.random_state))
            return false;
        state_ = candidate;
        return true;
    }

    void keep_playing()
    {
        state_.keep_playing = true;
    }

    MoveResult move(Direction direction)
    {
        MoveResult result{};
        result.game_over = state_.game_over;
        if (state_.game_over || (state_.won && !state_.keep_playing))
            return result;

        const auto before = state_.cells;
        for (std::size_t line = 0; line < kSide; ++line)
            move_line(direction, line, result);

        result.changed = state_.cells != before;
        if (result.changed)
        {
            spawn(&result);
            state_.best = state_.score > state_.best ? state_.score : state_.best;
            state_.game_over = !moves_available();
        }
        result.game_over = state_.game_over;
        return result;
    }

    bool moves_available() const
    {
        for (std::size_t i = 0; i < kCells; ++i)
        {
            const auto exponent = state_.cells[i];
            if (!exponent)
                return true;
            if (exponent == kMaxExponent)
                continue;
            if (i % kSide + 1 < kSide && state_.cells[i + 1] == exponent)
                return true;
            if (i + kSide < kCells && state_.cells[i + kSide] == exponent)
                return true;
        }
        return false;
    }

  private:
    struct SourceTile
    {
        std::uint8_t cell;
        std::uint8_t exponent;
    };

    Snapshot state_{};

    static std::uint64_t tile_value(std::uint8_t exponent)
    {
        return UINT64_C(1) << exponent;
    }

    static std::uint64_t add_saturated(std::uint64_t a, std::uint64_t b)
    {
        return UINT64_MAX - a < b ? UINT64_MAX : a + b;
    }

    std::uint64_t random()
    {
        std::uint64_t z = (state_.random_state += UINT64_C(0x9e3779b97f4a7c15));
        z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
        return z ^ (z >> 31);
    }

    std::uint32_t random_below(std::uint32_t bound)
    {
        const std::uint32_t threshold = static_cast<std::uint32_t>(-bound) % bound;
        for (;;)
        {
            const auto value = static_cast<std::uint32_t>(random() >> 32);
            if (value >= threshold)
                return value % bound;
        }
    }

    void spawn(MoveResult *result = nullptr)
    {
        std::array<std::uint8_t, kCells> empty{};
        std::uint8_t count = 0;
        for (std::uint8_t i = 0; i < kCells; ++i)
            if (!state_.cells[i])
                empty[count++] = i;
        if (!count)
            return;

        const auto cell = empty[random_below(count)];
        const auto exponent = static_cast<std::uint8_t>(random_below(10) == 0 ? 2 : 1);
        state_.cells[cell] = exponent;
        if (result)
        {
            result->spawned_cell = cell;
            result->spawned_exponent = exponent;
        }
    }

    static std::uint8_t line_cell(Direction direction, std::size_t line, std::size_t offset)
    {
        switch (direction)
        {
        case Direction::left:
            return static_cast<std::uint8_t>(line * kSide + offset);
        case Direction::right:
            return static_cast<std::uint8_t>(line * kSide + (kSide - 1 - offset));
        case Direction::up:
            return static_cast<std::uint8_t>(offset * kSide + line);
        case Direction::down:
            return static_cast<std::uint8_t>((kSide - 1 - offset) * kSide + line);
        }
        return 0;
    }

    void move_line(Direction direction, std::size_t line, MoveResult &result)
    {
        std::array<SourceTile, kSide> tiles{};
        std::size_t count = 0;
        for (std::size_t offset = 0; offset < kSide; ++offset)
        {
            const auto cell = line_cell(direction, line, offset);
            const auto exponent = state_.cells[cell];
            if (exponent)
                tiles[count++] = {cell, exponent};
        }

        for (std::size_t offset = 0; offset < kSide; ++offset)
            state_.cells[line_cell(direction, line, offset)] = 0;

        std::size_t output = 0;
        for (std::size_t i = 0; i < count;)
        {
            const auto destination = line_cell(direction, line, output++);
            if (i + 1 < count && tiles[i].exponent == tiles[i + 1].exponent &&
                tiles[i].exponent < kMaxExponent)
            {
                const auto merged = static_cast<std::uint8_t>(tiles[i].exponent + 1);
                state_.cells[destination] = merged;
                result.tiles[result.tile_count++] = {tiles[i].cell, destination, tiles[i].exponent,
                                                     merged, true};
                result.tiles[result.tile_count++] = {tiles[i + 1].cell, destination,
                                                     tiles[i + 1].exponent, merged, true};
                result.highest_merge_exponent = std::max(result.highest_merge_exponent, merged);
                const auto gain = tile_value(merged);
                state_.score = add_saturated(state_.score, gain);
                result.score_gained = add_saturated(result.score_gained, gain);
                if (merged == 11 && !state_.won)
                {
                    state_.won = true;
                    result.first_win = true;
                }
                i += 2;
            }
            else
            {
                state_.cells[destination] = tiles[i].exponent;
                if (tiles[i].cell != destination)
                    result.tiles[result.tile_count++] = {
                        tiles[i].cell, destination, tiles[i].exponent, tiles[i].exponent, false};
                ++i;
            }
        }
    }
};
} // namespace ppz::g2048
