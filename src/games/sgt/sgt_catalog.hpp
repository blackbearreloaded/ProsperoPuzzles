// ProsperoPuzzles - Catalog of the vendored Tatham puzzles.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <span>
#include <string_view>

struct game;

namespace ppz::sgt
{

struct GameEntry
{
    const ::game *game;
    const char *id;           // upstream source name, e.g. "lightup"
    const char *display_name; // e.g. "Light Up"
    const char *description;  // e.g. "Light-bulb placing puzzle"
    const char *objective;
};

// All 40 official games, in upstream (alphabetical by id) order.
std::span<const GameEntry> catalog();

const GameEntry *find_game(std::string_view id);

} // namespace ppz::sgt
