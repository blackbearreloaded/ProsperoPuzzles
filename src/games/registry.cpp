// ProsperoPuzzles - Registry of every game in the collection.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/registry.hpp"

#include "games/sgt/sgt_catalog.hpp"

#include <algorithm>

namespace ppz::games
{

namespace
{

struct HowTo
{
    const char *id;
    const char *rules;
    const char *controls;
};

constexpr HowTo kHowTo[] = {
#define HOWTO(id, rules, controls) {#id, rules, controls},
#include "games/howto.inc"
#undef HOWTO
};

// Curated accents, assigned in A-Z order so neighbouring cards differ.
constexpr std::uint32_t kAccents[] = {0xf2b179, 0xffd166, 0x4cc9f0, 0x80ed99, 0xc77dff, 0xff8fa3,
                                      0x5ec2b7, 0xf4a261, 0x90be6d, 0x7b9cff, 0xe9c46a, 0xf28482};

std::vector<GameInfo> build()
{
    std::vector<GameInfo> games;
    for (const sgt::GameEntry &entry : sgt::catalog())
    {
        GameInfo info;
        info.id = entry.id;
        info.name = entry.display_name;
        info.tagline = entry.description;
        info.objective = entry.objective;
        info.kind = Kind::sgt;
        info.sgt = &entry;
        games.push_back(info);
    }
    games.push_back({"g2048",
                     "2048",
                     "Slide-and-merge number puzzle",
                     "Slide the tiles to merge equal numbers and build a 2048 tile.",
                     {},
                     Kind::g2048,
                     nullptr});
    games.push_back({"tenfold",
                     "Tenfold",
                     "Group-merging number puzzle",
                     "Merge groups of equal numbers and climb all the way to ten.",
                     {},
                     Kind::tenfold,
                     nullptr});
    for (const NativeGame &native : native_games())
    {
        GameInfo info;
        info.id = native.id;
        info.name = native.name;
        info.tagline = native.tagline;
        info.objective = native.objective;
        info.kind = Kind::native;
        info.create = native.create;
        games.push_back(info);
    }
    std::sort(games.begin(), games.end(),
              [](const GameInfo &a, const GameInfo &b) { return a.id < b.id; });
    for (GameInfo &game : games)
    {
        for (const HowTo &howto : kHowTo)
        {
            if (game.id == howto.id)
            {
                game.rules = howto.rules;
                game.controls = howto.controls;
            }
        }
    }

    std::vector<LibraryEntry> entries;
    for (const GameInfo &game : games)
        entries.push_back({game.id, game.name, game.tagline});
    std::sort(entries.begin(), entries.end(), Library::name_less);
    for (std::size_t order = 0; order < entries.size(); ++order)
    {
        for (GameInfo &game : games)
        {
            if (game.id == entries[order].id)
                game.accent =
                    gfx::Color::rgb(kAccents[order % (sizeof(kAccents) / sizeof(kAccents[0]))]);
        }
    }
    return games;
}

} // namespace

const std::vector<GameInfo> &all()
{
    static const std::vector<GameInfo> games = build();
    return games;
}

const GameInfo *find(std::string_view id)
{
    for (const GameInfo &game : all())
    {
        if (game.id == id)
            return &game;
    }
    return nullptr;
}

std::vector<LibraryEntry> library_entries()
{
    std::vector<LibraryEntry> entries;
    for (const GameInfo &game : all())
        entries.push_back({game.id, game.name, game.tagline});
    return entries;
}

} // namespace ppz::games
