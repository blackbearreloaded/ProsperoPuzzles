// ProsperoPuzzles - Registry of every game in the collection.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/library.hpp"
#include "gfx/draw_list.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace ppz::sgt
{
struct GameEntry;
}

namespace ppz::games
{

enum class Kind
{
    sgt, // one of the 40 Tatham puzzles
    g2048,
    tenfold,
};

struct GameInfo
{
    std::string id; // stable save/library key
    std::string name;
    std::string tagline;
    std::string objective;
    gfx::Color accent;
    Kind kind = Kind::sgt;
    const sgt::GameEntry *sgt = nullptr;
};

// All 42 games (40 Tatham puzzles, 2048 and Tenfold), in id order.
const std::vector<GameInfo> &all();
const GameInfo *find(std::string_view id);
std::vector<LibraryEntry> library_entries();

} // namespace ppz::games
