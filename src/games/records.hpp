// ProsperoPuzzles - Personal records: best times and scores, read back for the library.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "games/registry.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ppz::games
{

// Timed puzzles without a fixed size list (the Tatham puzzles): best time per
// named board size, plus how many games were started and solved.
struct TimedStats
{
    std::uint32_t played = 0;
    std::uint32_t solved = 0;
    std::vector<std::pair<std::string, std::uint32_t>> best; // size label -> seconds

    // Records a solve; returns true when it beats (or sets) the best for size.
    bool record(const std::string &size, std::uint32_t seconds);
    std::uint32_t best_for(const std::string &size) const;
};
std::string encode_timed(const TimedStats &stats);
bool decode_timed(std::string_view data, TimedStats *stats);

// What the library shows about a game's records.
struct Record
{
    bool beaten = false; // solved at least once (2048: reached 2048; Tenfold: made ten)
    std::string line;    // e.g. "Best 1:23 on 7 × 7  ·  Solved 5", empty when nothing yet
};
// stats is the game's decoded stats payload (the .stats file contents).
Record describe(const GameInfo &game, std::string_view stats);

// Seconds as m:ss.
std::string clock_text(std::uint32_t seconds);

} // namespace ppz::games
