// ProsperoPuzzles - Link Up rules: join every pair of dots and fill the board.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace ppz::linkup
{

constexpr int kMinSide = 5;
constexpr int kMaxSide = 9;
constexpr int kMaxCells = kMaxSide * kMaxSide;
constexpr int kMaxPairs = 12;
constexpr int kNone = -1;

struct Puzzle
{
    int side = 0;
    int pairs = 0;
    // The generator's cover of the board: each pair's cells in order, pair after
    // pair, from its first dot to its second. length[p] cells belong to pair p.
    std::array<std::uint8_t, kMaxCells> route{};
    std::array<std::uint8_t, kMaxPairs> length{};

    int cells() const
    {
        return side * side;
    }
    // Cell of dot 0 or 1 of a pair.
    int dot(int pair, int which) const;
    // The pair whose dot sits on cell, or kNone.
    int dot_at(int cell) const;
    // The generator's path for a pair, first dot to second.
    std::vector<std::uint8_t> solution(int pair) const;
};

// The player's path per pair: an ordered run of cells starting on one of its
// dots. Empty when the pair has not been started.
using Paths = std::array<std::vector<std::uint8_t>, kMaxPairs>;

bool adjacent(int side, int a, int b);

// A full cover by pairs; side is 5..9 and pairs is roughly the side.
Puzzle generate(std::uint64_t seed, int side);
// Checks a puzzle read from a save: sizes, a permutation route, adjacent steps.
bool valid_puzzle(const Puzzle &puzzle);
// Checks the player's paths: start on a dot, adjacent steps, no shared cells,
// no foreign dots, the pair's other dot only as the last cell.
bool valid_paths(const Puzzle &puzzle, const Paths &paths);

// The pair whose path covers cell, or kNone.
int owner(const Paths &paths, int cell);
bool connected(const Puzzle &puzzle, const Paths &paths, int pair);
int connected_pairs(const Puzzle &puzzle, const Paths &paths);
// Every pair is connected and every cell is covered.
bool solved(const Puzzle &puzzle, const Paths &paths);

// Starts drawing from cell: a dot restarts its pair's path from that dot, a
// path cell cuts its path back to that cell. Returns the pair, or kNone.
int pick_up(const Puzzle &puzzle, Paths &paths, int cell);

enum class Step : std::uint8_t
{
    rejected,  // off the board, not a neighbour, or another pair's dot
    extended,  // the path grew into an empty cell
    retracted, // the path was cut back onto one of its own cells
    cut,       // the path grew through another pair's path, cutting it
    connected, // the path reached its pair's other dot
};
// Moves the head of pair's path into cell (a neighbour of the head).
Step extend(const Puzzle &puzzle, Paths &paths, int pair, int cell);

} // namespace ppz::linkup
