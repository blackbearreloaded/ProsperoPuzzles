// ProsperoPuzzles - Library screen: A-Z grid of games with favorites.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "audio/cues.hpp"
#include "core/input.hpp"
#include "core/library.hpp"
#include "core/tween.hpp"
#include "gfx/draw_list.hpp"
#include "ui/theme.hpp"

#include <string>
#include <vector>

namespace ppz::ui
{

// What the library asks the shell to do after an update.
struct LibraryRequest
{
    enum class Kind
    {
        none,
        launch,  // start or resume game_id
        details, // open game details for game_id
        settings,
        favorites_changed,
    } kind = Kind::none;
    std::string game_id;
};

class LibraryScene
{
  public:
    static constexpr int kColumns = 6;

    explicit LibraryScene(Library &library);

    // Sounds requested during update are appended to cues.
    LibraryRequest update(const InputFrame &input, float dt, std::vector<audio::Cue> &cues);
    void draw(gfx::DrawList &list, const Fonts &fonts) const;

    void focus_game(const std::string &id);
    std::string focused_id() const;
    bool reduced_motion = false;

  private:
    struct Cell
    {
        int item = 0; // index into library items
        int row = 0;
        int column = 0;
        float x = 0.0f;
        float y = 0.0f; // content space (before scrolling)
    };
    struct Header
    {
        Section section;
        float y = 0.0f;
        int count = 0;
    };

    void relayout();
    void move_focus(int item, std::vector<audio::Cue> &cues, audio::Cue cue = audio::Cue::ui_focus);
    int cell_in_row(int row, int column) const;
    void draw_card(gfx::DrawList &list, const Fonts &fonts, const Cell &cell) const;

    Library &library_;
    std::vector<Cell> cells_;
    std::vector<Header> headers_;
    int rows_ = 0;
    int focus_ = 0;
    float time_ = 0.0f;
    tween::Spring scroll_;
    std::vector<tween::Spring> lift_; // per item focus animation
    tween::Spring accent_r_, accent_g_, accent_b_;
    tween::Timer favorite_burst_;
    int burst_item_ = -1;
    float content_height_ = 0.0f;
};

} // namespace ppz::ui
