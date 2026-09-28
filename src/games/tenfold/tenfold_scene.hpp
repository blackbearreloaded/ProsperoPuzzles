// ProsperoPuzzles - Tenfold play screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/tween.hpp"
#include "games/game_scene.hpp"
#include "games/tenfold/core.hpp"
#include "ui/menu.hpp"
#include "ui/theme.hpp"

#include <string>

namespace ppz::tenfold
{

struct Stats
{
    std::uint64_t best = 0;
    std::uint32_t played = 0;
    std::uint32_t reached_ten = 0;
    std::uint8_t highest = 0;
};

std::string encode_game(const Snapshot &state);
bool decode_game(std::string_view data, Snapshot *state);
std::string encode_stats(const Stats &stats);
bool decode_stats(std::string_view data, Stats *stats);

class TenfoldScene final : public games::GameScene
{
  public:
    explicit TenfoldScene(const ui::Fonts &fonts);

    void start(const std::string &save, const std::string &stats) override;
    games::SceneExit update(const InputFrame &input, float dt,
                            std::vector<audio::Cue> &cues) override;
    void draw(gfx::DrawList &list) const override;
    std::string save() override;
    std::string stats() override;
    bool in_progress() override;
    const std::string &id() const override
    {
        return id_;
    }

    // Test hooks.
    const Game &game() const
    {
        return game_;
    }
    int cursor() const
    {
        return cursor_;
    }
    bool has_selection() const
    {
        return selected_ >= 0;
    }
    bool menu_open() const
    {
        return menu_.is_open();
    }

  private:
    enum MenuId
    {
        kResume,
        kNewGame,
        kUndo,
        kLibrary,
        kKeepPlaying,
        kHowTo,
    };

    void new_game();
    void press_cross(std::vector<audio::Cue> &cues);
    void hint(std::vector<audio::Cue> &cues);
    void undo(std::vector<audio::Cue> &cues);
    void open_game_over();
    void record_finish();
    Group selection() const;

    std::string id_ = "tenfold";
    ui::Fonts fonts_;
    Game game_;
    Stats stats_;
    int cursor_ = 14;
    int selected_ = -1; // any cell of the selected group
    bool finish_recorded_ = false;
    ui::Menu menu_;
    MoveResult last_{};
    tween::Timer fall_;
    tween::Timer shake_;
    tween::Timer hint_;
    tween::Spring cursor_x_;
    tween::Spring cursor_y_;
    float time_ = 0.0f;
};

} // namespace ppz::tenfold
