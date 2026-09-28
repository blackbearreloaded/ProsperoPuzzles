// ProsperoPuzzles - Kakuro rules: digit runs that add up to their clues.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace ppz::kakuro
{

constexpr int kMinSide = 4;
constexpr int kMaxSide = 10; // including the clue row and column
constexpr int kMaxCells = kMaxSide * kMaxSide;
constexpr int kMaxRun = 9;

using Digits = std::array<std::uint8_t, kMaxCells>; // row-major, 0 = empty

struct Puzzle
{
    int side = 0;
    Digits solution{};                           // 0 = block, else the cell's digit
    std::array<std::uint8_t, kMaxCells> right{}; // across clue on a block (0 = none)
    std::array<std::uint8_t, kMaxCells> down{};  // down clue on a block (0 = none)

    bool white(int cell) const
    {
        return solution[static_cast<std::size_t>(cell)] != 0;
    }
};

// A maximal line of white cells and the block that holds its clue.
struct Run
{
    int clue = 0; // the block cell
    int sum = 0;
    bool across = true;
    int length = 0;
    std::array<std::uint8_t, kMaxRun> cells{};
};

// Every run, across runs first; the clues come from puzzle.right/down.
std::vector<Run> runs(const Puzzle &puzzle);
// Sets right/down from the solution's digits.
void compute_clues(Puzzle *puzzle);
// Row 0 and column 0 are blocks, every white cell sits in an across and a
// down run of length 2..9, no run repeats a digit and the clues match.
bool well_formed(const Puzzle &puzzle);

// A puzzle with exactly one solution; side is the full grid (clues included), 4..10.
Puzzle generate(std::uint64_t seed, int side);
// Counts solutions up to limit (the generator asks for 2 to prove uniqueness).
int count_solutions(const Puzzle &puzzle, int limit, Digits *first = nullptr);

// Cells whose digit repeats in a run or sits in a full run with the wrong sum.
std::vector<bool> conflicts(const Puzzle &puzzle, const Digits &digits);
// Every white cell is filled and every run is correct.
bool solved(const Puzzle &puzzle, const Digits &digits);
int white_cells(const Puzzle &puzzle);
int filled_cells(const Puzzle &puzzle, const Digits &digits);

} // namespace ppz::kakuro
