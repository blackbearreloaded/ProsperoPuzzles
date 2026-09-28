// ProsperoPuzzles - Color Sort rules: pour coloured balls until every tube holds one colour.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace ppz::colorsort
{

constexpr int kCapacity = 4;   // balls per tube, and balls per colour
constexpr int kMaxColours = 8; // Large
constexpr int kEmptyTubes = 2; // spare tubes every board starts with
constexpr int kMaxTubes = kMaxColours + kEmptyTubes;
// States the solver may visit before a board counts as too hard.
constexpr int kSolveBudget = 60000;

struct Board
{
    int colours = 0;
    int tubes = 0;
    // Ball colours 0..colours-1, bottom first; only the first count[t] are used.
    std::array<std::array<std::uint8_t, kCapacity>, kMaxTubes> ball{};
    std::array<std::uint8_t, kMaxTubes> count{};

    // The colour of the tube's top ball, or -1 when it is empty.
    int top(int tube) const;
};

struct Move
{
    std::uint8_t from = 0;
    std::uint8_t to = 0;
};

// A solvable board: colours full tubes shuffled at random, then kEmptyTubes
// empty ones; no tube starts complete. colours is 2..kMaxColours.
Board generate(std::uint64_t seed, int colours);

// Same-coloured balls on top of the tube.
int top_run(const Board &board, int tube);
// Balls a pour from one tube onto another would move (0 when not allowed).
int pour_amount(const Board &board, int from, int to);
// Pours and returns the balls moved (0 when not allowed; the board is unchanged).
int pour(Board &board, int from, int to);
// Full with a single colour.
bool tube_complete(const Board &board, int tube);
// Every tube is empty or complete.
bool solved(const Board &board);
// Complete tubes (sorted colours).
int sorted_count(const Board &board);
// Shape and ball counts are consistent (kCapacity balls of every colour).
bool valid(const Board &board);

enum class Verdict : std::uint8_t
{
    solvable,
    unsolvable,
    gave_up, // the budget ran out first
};
// Depth-first search over canonical states (tube order ignored). On success
// path (when given) receives the moves that solve the board.
Verdict solve(const Board &board, int budget, std::vector<Move> *path = nullptr);

} // namespace ppz::colorsort
