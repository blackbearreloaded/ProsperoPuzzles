// ProsperoPuzzles - Settings screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "audio/cues.hpp"
#include "core/input.hpp"
#include "core/settings.hpp"
#include "core/tween.hpp"
#include "gfx/draw_list.hpp"
#include "ui/theme.hpp"

#include <string>
#include <vector>

namespace ppz::ui
{

class SettingsScene
{
  public:
    enum class Result
    {
        none,
        changed, // a value changed: apply and save
        close,
        about, // open the About screen
    };

    explicit SettingsScene(Settings &settings);

    Result update(const InputFrame &input, float dt, std::vector<audio::Cue> &cues);
    void draw(gfx::DrawList &list, const Fonts &fonts, const std::string &version) const;

  private:
    enum Row
    {
        kMusic,
        kEffects,
        kInterface,
        kReducedMotion,
        kSwapConfirm,
        kShowFps,
        kResolution,
        kAbout,
        kRowCount,
    };

    Settings &settings_;
    int focus_ = 0;
    tween::Spring highlight_;
};

} // namespace ppz::ui
