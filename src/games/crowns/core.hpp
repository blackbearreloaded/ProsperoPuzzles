// ProsperoPuzzles - Crowns rules: one crown per row, column and region, none touching.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace ppz::crowns
{

constexpr int kMaxSide = 9;
constexpr int kMaxCells = kMaxSide * kMaxSide;

enum Mark : std::uint8_t
{
    kEmpty = 0,
    kCross = 1, // the player ruled the cell out
    kCrown = 2,
};

struct Puzzle
{
    int side = 0;
    std::array<std::uint8_t, kMaxCells> region{};  // 0..side-1, row-major
    std::array<std::uint8_t, kMaxSide> solution{}; // crown column for each row
};

// A puzzle with exactly one solution; side is 5..9.
Puzzle generate(std::uint64_t seed, int side);
// Counts solutions up to limit (the generator asks for 2 to prove uniqueness).
int count_solutions(const Puzzle &puzzle, int limit,
                    std::array<std::uint8_t, kMaxSide> *first = nullptr);

// Cells whose crown breaks a rule (row, column, region or touching).
std::vector<bool> conflicts(const Puzzle &puzzle, const std::array<std::uint8_t, kMaxCells> &marks);
// Every row, column and region holds exactly one crown and none touch.
bool solved(const Puzzle &puzzle, const std::array<std::uint8_t, kMaxCells> &marks);
int crowns_placed(const Puzzle &puzzle, const std::array<std::uint8_t, kMaxCells> &marks);

} // namespace ppz::crowns
