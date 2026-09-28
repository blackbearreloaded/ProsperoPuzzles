// ProsperoPuzzles - The native puzzles built on the shared kit.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/native.hpp"

#include "games/crowns/crowns_scene.hpp"
#include "games/sokoban/sokoban_scene.hpp"

namespace ppz::games
{

namespace
{

template <typename Scene> std::unique_ptr<GameScene> make(const ui::Fonts &fonts)
{
    return std::make_unique<Scene>(fonts);
}

constexpr NativeGame kNative[] = {
    {"crowns", "Crowns", "Royal placement puzzle",
     "Place one crown in every row, column and colour region, with no two touching.",
     &make<crowns::CrownsScene>},
    {"sokoban", "Sokoban", "Warehouse crate pushing",
     "Push every crate onto a target. Crates can only be pushed, one at a time.",
     &make<sokoban::SokobanScene>},
};

} // namespace

std::span<const NativeGame> native_games()
{
    return kNative;
}

} // namespace ppz::games
