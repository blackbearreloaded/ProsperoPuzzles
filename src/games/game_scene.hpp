// ProsperoPuzzles - Interface every playable game screen implements.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "audio/cues.hpp"
#include "core/input.hpp"
#include "gfx/draw_list.hpp"

#include <string>
#include <vector>

namespace ppz::kit
{
class PuzzleScene;
}

namespace ppz::games
{

enum class SceneExit
{
    none,
    library, // the player asked to return to the library
    howto,   // show the How to play card; the game stays as it is
};

class GameScene
{
  public:
    virtual ~GameScene() = default;

    // Starts a new game, or resumes a serialised one when save is not empty.
    // stats is the game's long-lived record (best score, games played), kept
    // even when no game is in progress.
    virtual void start(const std::string &save, const std::string &stats) = 0;
    virtual std::string stats()
    {
        return {};
    }
    virtual SceneExit update(const InputFrame &input, float dt, std::vector<audio::Cue> &cues) = 0;
    virtual void draw(gfx::DrawList &list) const = 0;

    // Serialised state to resume later; in_progress() says whether it is worth keeping.
    virtual std::string save() = 0;
    virtual bool in_progress() = 0;
    virtual const std::string &id() const = 0;

    // The shared-kit view of a native puzzle (previews, tests); the PS5
    // build has no RTTI, so this stands in for dynamic_cast.
    virtual kit::PuzzleScene *as_puzzle()
    {
        return nullptr;
    }
};

} // namespace ppz::games
