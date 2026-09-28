// ProsperoPuzzles - Trail rules: one path through every cell, passing the numbers in order.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace ppz::trail
{

constexpr int kMaxSide = 8;
constexpr int kMaxCells = kMaxSide * kMaxSide; // fits one 64-bit mask

struct Puzzle
{
    int side = 0;
    int count = 0;                                  // the highest number, K
    std::array<std::uint8_t, kMaxCells> number{};   // 0 = plain cell, else 1..count
    std::array<std::uint8_t, kMaxCells> solution{}; // cells in path order
};

// A puzzle with exactly one solution; side is 4..kMaxSide.
Puzzle generate(std::uint64_t seed, int side);

// Counts solutions up to limit. Stops early (returns -1) when the search
// exceeds node_budget. first receives the first solution found.
int count_solutions(const Puzzle &puzzle, int limit, std::vector<std::uint8_t> *first = nullptr,
                    long node_budget = 0);

// The cell holding number n (1..count), or -1.
int cell_of(const Puzzle &puzzle, int n);
// The next number the path has to reach (count + 1 once it holds every number).
int next_number(const Puzzle &puzzle, const std::vector<std::uint8_t> &path);

// A legal partial path: starts on 1, orthogonal steps, no repeats, numbers in
// order, nothing after the highest number.
bool valid_path(const Puzzle &puzzle, const std::vector<std::uint8_t> &path);
// A legal path that covers every cell (so it ends on the highest number).
bool solved(const Puzzle &puzzle, const std::vector<std::uint8_t> &path);

enum class Step : std::uint8_t
{
    extended,     // the path grew into the neighbour
    retracted,    // the neighbour was the previous cell: the head was erased
    off_board,    // no cell there
    crossed,      // the neighbour is already on the path
    out_of_order, // the neighbour holds a number that is not next
    finished,     // the head is on the highest number; the path cannot go on
};
// Moves the path's head one cell (dcol, drow), applying the rules.
Step step(const Puzzle &puzzle, std::vector<std::uint8_t> *path, int dcol, int drow);

} // namespace ppz::trail
