// ProsperoPuzzles - The native puzzles built on the shared kit.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/native.hpp"

#include "games/crowns/crowns_scene.hpp"
#include "games/colorsort/colorsort_scene.hpp"
#include "games/kakuro/kakuro_scene.hpp"
#include "games/trail/trail_scene.hpp"
#include "games/linkup/linkup_scene.hpp"
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
    {"linkup", "Link Up", "Colour path puzzle",
     "Join every pair of matching dots with a path and fill the whole board.",
     &make<linkup::LinkUpScene>},
    {"trail", "Trail", "One-line path puzzle",
     "Draw one path through every cell, passing the numbers in order from 1 to the last.",
     &make<trail::TrailScene>},
    {"kakuro", "Kakuro", "Cross-sum number puzzle",
     "Fill the white cells with 1 to 9 so every run adds up to its clue, with no digit repeated in "
     "a run.",
     &make<kakuro::KakuroScene>},
    {"colorsort", "Color Sort", "Ball sorting puzzle",
     "Pour the coloured balls between tubes until every tube holds a single colour.",
     &make<colorsort::ColorSortScene>},
};

} // namespace

std::span<const NativeGame> native_games()
{
    return kNative;
}

} // namespace ppz::games
