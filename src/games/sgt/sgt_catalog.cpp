// ProsperoPuzzles - Catalog of the vendored Tatham puzzles.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/sgt/sgt_catalog.hpp"

#include "games/sgt/sgt_c.hpp"

namespace ppz::sgt
{

namespace
{

#define SGT_GAME(id, name, description, objective) {&id, #id, name, description, objective},
constexpr GameEntry kCatalog[] = {
#include "games/sgt/upstream_meta.inc"
};
#undef SGT_GAME

} // namespace

std::span<const GameEntry> catalog()
{
    return kCatalog;
}

const GameEntry *find_game(std::string_view id)
{
    for (const GameEntry &entry : kCatalog)
    {
        if (id == entry.id)
            return &entry;
    }
    return nullptr;
}

} // namespace ppz::sgt
