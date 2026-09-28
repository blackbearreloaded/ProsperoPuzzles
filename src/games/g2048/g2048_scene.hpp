// ProsperoPuzzles - 2048 play screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/tween.hpp"
#include "games/g2048/core.hpp"
#include "games/game_scene.hpp"
#include "ui/menu.hpp"
#include "ui/theme.hpp"

#include <optional>
#include <string>

namespace ppz::g2048
{

struct Stats
{
    std::uint64_t best = 0;
    std::uint32_t played = 0;
    std::uint32_t won = 0;
    std::uint8_t highest = 0; // exponent
};

// Save payloads (tested on the host).
std::string encode_game(const Snapshot &state, std::uint32_t moves,
                        const std::optional<Snapshot> &undo);
bool decode_game(std::string_view data, Snapshot *state, std::uint32_t *moves,
                 std::optional<Snapshot> *undo);
std::string encode_stats(const Stats &stats);
bool decode_stats(std::string_view data, Stats *stats);

class G2048Scene final : public games::GameScene
{
  public:
    explicit G2048Scene(const ui::Fonts &fonts);

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
        kKeepGoing,
        kHowTo,
    };

    void new_game();
    void apply_move(Direction direction, std::vector<audio::Cue> &cues);
    void open_pause(std::vector<audio::Cue> &cues);
    void open_game_over();
    void record_finish(bool won);
    void draw_tile(gfx::DrawList &list, float cx, float cy, std::uint8_t exponent,
                   float scale) const;

    std::string id_ = "g2048";
    ui::Fonts fonts_;
    Game game_;
    std::optional<Snapshot> undo_;
    std::uint32_t moves_ = 0;
    Stats stats_;
    bool finish_recorded_ = false;
    ui::Menu menu_;
    MoveResult last_{};
    tween::Timer slide_;
    tween::Timer pop_;
    tween::Timer shake_;
    tween::Timer gain_;
    std::uint64_t gained_ = 0;
    float time_ = 0.0f;
};

} // namespace ppz::g2048
