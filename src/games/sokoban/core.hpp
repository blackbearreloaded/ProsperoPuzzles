// ProsperoPuzzles - Sokoban rules: push every crate onto a target, never pull.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace ppz::sokoban
{

constexpr int kMaxSide = 10; // board side including the walls
constexpr int kMaxCells = kMaxSide * kMaxSide;
constexpr int kMaxCrates = 6;

enum Cell : std::uint8_t
{
    kVoid = 0,   // outside the room
    kWall = 1,   // a wall block
    kFloor = 2,  // walkable floor
    kTarget = 3, // floor with a target
};

// Directions: up, down, left, right.
constexpr int kDirCol[4] = {0, 0, -1, 1};
constexpr int kDirRow[4] = {-1, 1, 0, 0};

struct State
{
    std::uint8_t keeper = 0; // cell index
    // Cell indices; the first Puzzle::crates are used.
    std::array<std::uint8_t, kMaxCrates> crates{};
};

struct Puzzle
{
    int cols = 0;
    int rows = 0;
    std::array<std::uint8_t, kMaxCells> cell{}; // Cell, row-major
    int crates = 0;                             // crates (and targets)
    State start;                                // the starting position
    int min_pushes = -1;                        // the solver's optimum, -1 when not solved
};

// A room for size 0..2 (Small, Medium, Large), solvable by construction.
Puzzle generate(std::uint64_t seed, int size);

bool is_floor(const Puzzle &puzzle, int cell);
// The cell one step from cell in direction dir, or -1 off the board.
int neighbour(const Puzzle &puzzle, int cell, int dir);
// The crate slot at cell, or -1.
int crate_at(const Puzzle &puzzle, const State &state, int cell);

enum class Step : std::uint8_t
{
    none,    // a wall or the board edge: nothing happens
    blocked, // a crate that cannot move
    walk,    // the keeper stepped
    push,    // the keeper stepped and pushed a crate
};
// Moves the keeper one step (pushing a crate when the cell beyond is free).
// pushed receives the crate slot that moved (or -1).
Step move(const Puzzle &puzzle, State *state, int dir, int *pushed = nullptr);

int crates_on_targets(const Puzzle &puzzle, const State &state);
bool solved(const Puzzle &puzzle, const State &state);
// A crate off target in a corner of walls can never move again.
bool dead_corner(const Puzzle &puzzle, int cell);

// Breadth-first search over pushes. Returns the minimum number of pushes, or
// -1 when unsolvable within node_limit states. When moves is given it
// receives a full keeper move sequence (directions 0..3) for that solution.
int solve(const Puzzle &puzzle, const State &state, int node_limit,
          std::vector<int> *moves = nullptr);

} // namespace ppz::sokoban
