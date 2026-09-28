// ProsperoPuzzles - App shell: library, game screens, transitions and saves.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "app/thumbnails.hpp"
#include "audio/cues.hpp"
#include "core/input.hpp"
#include "core/library.hpp"
#include "core/settings.hpp"
#include "core/tween.hpp"
#include "games/game_scene.hpp"
#include "gfx/draw_list.hpp"
#include "gfx/gl_batch.hpp"
#include "ui/confetti.hpp"
#include "ui/howto_card.hpp"
#include "ui/library_scene.hpp"
#include "ui/menu.hpp"
#include "ui/settings_scene.hpp"
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

    const Settings &settings() const
    {
        return settings_;
    }
    // True once after the settings changed (the caller applies them).
    bool take_settings_changed()
    {
        const bool changed = settings_changed_;
        settings_changed_ = false;
        return changed;
    }

  private:
    enum class Stage
    {
        library,
        entering, // library zooms out, game fades in
        game,
        leaving, // game fades out, library returns
        settings,
    };

    void launch(const std::string &id);
    void save_library();
    void save_game();
    std::string game_path(const std::string &id) const;
    std::string stats_path(const std::string &id) const;
    void toast(const std::string &text);
    void save_settings();
    void open_details(const std::string &id);
    void start_game(const std::string &id, bool fresh);

    gfx::GlBatch &batch_;
    ui::Fonts fonts_;
    float surface_scale_;
    std::string root_;
    Library library_;
    ui::LibraryScene library_scene_;
    std::unique_ptr<games::GameScene> game_;
    Stage stage_ = Stage::library;
    tween::Timer transition_;
    tween::Timer toast_timer_;
    std::string toast_text_;
    std::vector<audio::Cue> cues_;
    float autosave_ = 0.0f;
    Settings settings_;
    bool settings_changed_ = true;
    ui::SettingsScene settings_scene_{settings_};
    ui::Menu details_;
    Thumbnails thumbnails_{batch_, fonts_, surface_scale_};
    std::string details_id_;
    ui::Confetti confetti_;
    ui::HowToCard howto_;
    std::uint32_t celebrations_ = 0;
};

} // namespace ppz::app
