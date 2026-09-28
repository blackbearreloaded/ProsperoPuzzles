// ProsperoPuzzles - How to play content tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/registry.hpp"

#include <gtest/gtest.h>

#include <cstring>

TEST(HowTo, EveryGameHasRulesAndControls)
{
    for (const ppz::games::GameInfo &game : ppz::games::all())
    {
        EXPECT_GT(std::strlen(game.rules), 40u) << game.id;
        EXPECT_GT(std::strlen(game.controls), 10u) << game.id;
    }
}
