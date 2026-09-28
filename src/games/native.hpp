// ProsperoPuzzles - The native puzzles built on the shared kit.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "games/game_scene.hpp"
#include "ui/theme.hpp"

#include <memory>
#include <span>

namespace ppz::games
{

using Factory = std::unique_ptr<GameScene> (*)(const ui::Fonts &fonts);

struct NativeGame
{
    const char *id;
    const char *name;
    const char *tagline;
    const char *objective;
    Factory create;
};

std::span<const NativeGame> native_games();

} // namespace ppz::games
