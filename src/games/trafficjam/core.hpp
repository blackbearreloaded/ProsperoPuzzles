// ProsperoPuzzles - Traffic Jam rules: slide cars and trucks to let the red car out.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace ppz::trafficjam
{

constexpr int kSide = 6;
constexpr int kCells = kSide * kSide;
constexpr int kExitRow = 2;             // the red car's row; the exit is on its right edge
constexpr int kMaxVehicles = 16;        // vehicle 0 is always the red car
constexpr int kMinPar[3] = {5, 10, 16}; // minimum solution length per level

struct Vehicle
{
    std::uint8_t length = 2; // 2 = car, 3 = truck
    bool vertical = false;
    std::uint8_t lane = 0; // the row of a horizontal vehicle, the column of a vertical one
};

// Each vehicle's position along its axis: the leftmost column or topmost row.
using Positions = std::array<std::uint8_t, kMaxVehicles>;

struct Puzzle
{
    int count = 0;
    std::array<Vehicle, kMaxVehicles> vehicles{};
    Positions start{};
    int par = 0; // fewest moves to free the red car
};

// One move: a vehicle slid (any distance) to a new position.
struct Move
{
    int vehicle = 0;
    int to = 0;
};

// The cell index (row * kSide + col) of a vehicle's i-th cell at position pos.
inline int vehicle_cell(const Vehicle &v, int pos, int i)
{
    return v.vertical ? (pos + i) * kSide + v.lane : v.lane * kSide + pos + i;
}

// Which vehicle covers each cell (-1 for empty).
std::array<std::int8_t, kCells> occupancy(const Puzzle &puzzle, const Positions &pos);
// Every vehicle inside the lot, the red car in place and no two overlapping.
bool valid(const Puzzle &puzzle, const Positions &pos);
// How many free cells vehicle v can slide towards dir (-1 or +1).
int slide_room(const Puzzle &puzzle, const Positions &pos, int v, int dir);
// The red car's right end has reached the exit.
bool solved(const Puzzle &puzzle, const Positions &pos);
// Fewest moves from pos to solved (-1 when there is no way out); fills path when given.
int solve(const Puzzle &puzzle, const Positions &pos, std::vector<Move> *path = nullptr);
// A puzzle for level 0..2 (Easy, Medium, Hard) needing at least kMinPar[level] moves.
Puzzle generate(std::uint64_t seed, int level);

} // namespace ppz::trafficjam
