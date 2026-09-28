// ProsperoPuzzles - Nurikabe rules: numbered islands in one connected sea, no 2x2 pools.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace ppz::nurikabe
{

constexpr int kMinSide = 5;
constexpr int kMaxSide = 10;
constexpr int kMaxCells = kMaxSide * kMaxSide;

enum Mark : std::uint8_t
{
    kUnknown = 0,
    kSea = 1,
    kDot = 2, // the player marked the cell as island
};

using Cells = std::array<std::uint8_t, kMaxCells>;

struct Puzzle
{
    int side = 0;
    Cells clue{};     // island size on its numbered cell, 0 elsewhere (row-major)
    Cells solution{}; // 1 where the solution has sea
};

// A puzzle whose solution the solver proves unique; side is 5..10.
Puzzle generate(std::uint64_t seed, int side);
// Counts solutions up to limit by propagation and backtracking. Returns -1 when
// the work budget runs out before the answer is known.
int count_solutions(const Puzzle &puzzle, int limit, Cells *first = nullptr, int budget = 1 << 20);
// Runs the deductions from the bare puzzle: state gets 0 unknown, 1 sea, 2 island.
// False when they meet a contradiction.
bool deduce(const Puzzle &puzzle, Cells *state);
// Whether the deductions alone (no guessing) settle every cell.
bool solves_by_logic(const Puzzle &puzzle);
// sea[i] != 0 marks sea; checks every rule.
bool valid_solution(const Puzzle &puzzle, const Cells &sea);

struct Errors
{
    std::vector<bool> bad;  // numbered cells (and dot groups) in an island that cannot be right
    std::vector<int> pools; // top-left cells of 2x2 blocks that are all sea
};
Errors errors(const Puzzle &puzzle, const Cells &marks);
// The marks solve the puzzle (cells not marked sea count as island).
bool solved(const Puzzle &puzzle, const Cells &marks);
int sea_marked(const Puzzle &puzzle, const Cells &marks);
int sea_target(const Puzzle &puzzle);

} // namespace ppz::nurikabe
