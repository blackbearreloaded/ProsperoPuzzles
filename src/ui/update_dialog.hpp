// ProsperoPuzzles - The update dialog: offer, progress, and the close for the update.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "audio/cues.hpp"
#include "core/input.hpp"
#include "core/tween.hpp"
#include "core/updater.hpp"
#include "gfx/draw_list.hpp"
#include "ui/theme.hpp"

#include <string>
#include <vector>

namespace ppz::ui
{

// Offers a newer release (Update now / Later). On yes, a ring fills while the
// release downloads and unpacks; then the app closes and the update helper
// replaces its files. Nothing is changed before that, and Circle cancels.
class UpdateDialog
{
  public:
    enum class Result : std::uint8_t
    {
        none,
        staged, // once: the update is ready and the app is about to close
        quit,   // close the app now; the helper takes over
    };

    void open(Updater &updater, const UpdateOffer &offer, bool reduced_motion);
    bool is_open() const
    {
        return open_;
    }
    // Owns the input while open. Also animates the fade once closed.
    Result update(const InputFrame &input, float dt, std::vector<audio::Cue> &cues);
    void draw(gfx::DrawList &list, const Fonts &fonts) const;

  private:
    enum class Stage : std::uint8_t
    {
        offer,
        notice, // newer, but this install can't update itself: tell only
        working,
        cancelling,
        closing,
        failed,
    };

    void enter(Stage stage);
    void begin(std::vector<audio::Cue> &cues);
    void fail(const std::string &reason, std::vector<audio::Cue> &cues);
    void close(std::vector<audio::Cue> &cues);

    Updater *updater_ = nullptr;
    UpdateOffer offer_;
    UpdateProgress progress_;
    Stage stage_ = Stage::offer;
    bool open_ = false;
    bool reduced_motion_ = false;
    int choice_ = 0;
    float stage_time_ = 0.0f;
    float time_ = 0.0f;
    std::string error_;
    tween::Spring fade_;
    tween::Spring choice_x_;
    tween::Spring fraction_;
    tween::Spring height_;
};

} // namespace ppz::ui
