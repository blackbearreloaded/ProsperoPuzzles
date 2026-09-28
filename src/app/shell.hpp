// ProsperoPuzzles - App shell: library, game screens, transitions and saves.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "audio/cues.hpp"
#include "core/input.hpp"
#include "core/library.hpp"
#include "core/tween.hpp"
#include "games/sgt/sgt_scene.hpp"
#include "gfx/draw_list.hpp"
#include "gfx/gl_batch.hpp"
#include "ui/library_scene.hpp"
#include "ui/theme.hpp"

#include <memory>
#include <string>
#include <vector>

namespace ppz::app
{

// Owns the scene flow: Library -> game -> Library. Saves favorites and games
// in progress as they change (a shell close or GPU fail-stop skips quit
// handlers, so nothing waits for exit).
class Shell
{
  public:
    Shell(gfx::GlBatch &batch, const ui::Fonts &fonts, float surface_scale, std::string data_root);

    void update(const InputFrame &input, float dt);
    void draw(gfx::DrawList &list) const;

    // Sounds requested since the last call.
    std::vector<audio::Cue> take_cues();
    // Game id whose sounds should use per-game overrides ("" in the library).
    std::string active_game() const;

  private:
    enum class Stage
    {
        library,
        entering, // library zooms out, game fades in
        game,
        leaving, // game fades out, library returns
    };

    void launch(const std::string &id);
    void save_library();
    void save_game();
    std::string game_path(const std::string &id) const;
    void toast(const std::string &text);

    gfx::GlBatch &batch_;
    ui::Fonts fonts_;
    float surface_scale_;
    std::string root_;
    Library library_;
    ui::LibraryScene library_scene_;
    std::unique_ptr<sgt::SgtScene> game_;
    Stage stage_ = Stage::library;
    tween::Timer transition_;
    tween::Timer toast_timer_;
    std::string toast_text_;
    std::vector<audio::Cue> cues_;
    float autosave_ = 0.0f;
};

} // namespace ppz::app
