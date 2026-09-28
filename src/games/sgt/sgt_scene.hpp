// ProsperoPuzzles - Play screen for one Tatham puzzle.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "audio/cues.hpp"
#include "core/input.hpp"
#include "core/tween.hpp"
#include "games/sgt/sgt_canvas.hpp"
#include "games/sgt/sgt_session.hpp"
#include "gfx/draw_list.hpp"
#include "ui/theme.hpp"

#include <memory>
#include <string>
#include <vector>

namespace ppz::sgt
{

enum class SceneExit
{
    none,
    library, // the player asked to return to the library
};

// Controller mapping (PLAN.md 5.9): D-pad cursor keys, Cross/Square select
// and select2, L1/R1 undo/redo, Triangle key palette, left stick pointer,
// Options pause menu, Circle closes overlays.
class SgtScene
{
  public:
    SgtScene(const GameEntry &entry, gfx::GlBatch &batch, const ui::Fonts &fonts,
             float surface_scale);

    // Starts a new game, or restores a serialised one when save is not empty.
    void start(const std::string &save = {});

    SceneExit update(const InputFrame &input, float dt, std::vector<audio::Cue> &cues);
    void draw(gfx::DrawList &list) const;

    std::string save();
    bool in_progress();
    const GameEntry &entry() const
    {
        return entry_;
    }

  private:
    enum class Overlay
    {
        none,
        pause,
        palette,
    };

    void layout();
    void send(int button, std::vector<audio::Cue> &cues, int x = 0, int y = 0);
    void handle_pointer(const InputFrame &input, float dt, std::vector<audio::Cue> &cues);
    void run_pause_item(int item, std::vector<audio::Cue> &cues, SceneExit &exit);
    void draw_pause(gfx::DrawList &list) const;
    void draw_palette(gfx::DrawList &list) const;

    const GameEntry &entry_;
    ui::Fonts fonts_;
    float surface_scale_;
    std::unique_ptr<CanvasRenderer> renderer_;
    std::unique_ptr<Session> session_;
    gfx::Rect board_; // virtual pixels
    Overlay overlay_ = Overlay::none;
    int pause_focus_ = 0;
    int palette_focus_ = 0;
    std::vector<KeyLabel> keys_;
    bool pointer_active_ = false;
    float pointer_x_ = 0.0f; // canvas pixels
    float pointer_y_ = 0.0f;
    int pointer_button_ = 0; // LEFT_BUTTON/RIGHT_BUTTON while held, else 0
    float time_ = 0.0f;
    tween::Timer solved_banner_;
    tween::Timer shake_;
    tween::Spring overlay_fade_;
    bool assisted_ = false;
    int last_status_ = 0;
};

} // namespace ppz::sgt
