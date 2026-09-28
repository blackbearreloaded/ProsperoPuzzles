// ProsperoPuzzles - Settings screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/settings_scene.hpp"

#include <algorithm>
#include <cstdio>

namespace ppz::ui
{

namespace
{

constexpr float kRowHeight = 88.0f;
constexpr float kTop = 262.0f;
constexpr float kLeft = 420.0f;
constexpr float kWidth = 1080.0f;

const char *row_label(int row)
{
    static const char *const labels[] = {
        "Music volume",   "Sound effects volume",  "Interface sounds",
        "Reduced motion", "Swap Cross and Circle", "Show FPS",
        "Resolution"};
    return labels[row];
}

const char *row_help(int row)
{
    static const char *const help[] = {
        "Background music in the library and in games.",
        "Moves, merges, completions and other game sounds.",
        "Menu navigation and selection sounds.",
        "Replaces zooms, slides and bounces with quick fades.",
        "Circle confirms and Cross goes back.",
        "Shows frames per second in the top-right corner.",
        "Rendering size. The PS5 scales the picture to your TV.",
    };
    return help[row];
}

} // namespace

SettingsScene::SettingsScene(Settings &settings) : settings_(settings)
{
}

SettingsScene::Result SettingsScene::update(const InputFrame &input, float dt,
                                            std::vector<audio::Cue> &cues)
{
    highlight_.target = static_cast<float>(focus_);
    highlight_.update(dt, settings_.reduced_motion ? 60.0f : 18.0f);
    if (input.is_pressed(Action::back) || input.is_pressed(Action::menu))
    {
        cues.push_back(audio::Cue::ui_back);
        return Result::close;
    }
    if (input.nav == Direction::up && focus_ > 0)
    {
        --focus_;
        cues.push_back(audio::Cue::ui_focus);
    }
    else if (input.nav == Direction::down && focus_ + 1 < kRowCount)
    {
        ++focus_;
        cues.push_back(audio::Cue::ui_focus);
    }
    int *volume = nullptr;
    if (focus_ == kMusic)
        volume = &settings_.music_volume;
    else if (focus_ == kEffects)
        volume = &settings_.sfx_volume;
    else if (focus_ == kInterface)
        volume = &settings_.ui_volume;
    if (volume != nullptr)
    {
        const int before = *volume;
        if (input.nav == Direction::left)
            *volume = std::max(0, *volume - 1);
        else if (input.nav == Direction::right)
            *volume = std::min(10, *volume + 1);
        if (*volume != before)
        {
            // Preview the bus being changed.
            cues.push_back(focus_ == kEffects ? audio::Cue::place : audio::Cue::ui_slider);
            return Result::changed;
        }
        return Result::none;
    }
    if (focus_ == kResolution)
    {
        const int count = Settings::kResolutionCount;
        const int before = settings_.resolution;
        if (input.nav == Direction::left)
            settings_.resolution = (settings_.resolution + count - 1) % count;
        else if (input.nav == Direction::right || input.is_pressed(Action::confirm))
            settings_.resolution = (settings_.resolution + 1) % count;
        if (settings_.resolution == before)
            return Result::none;
        cues.push_back(audio::Cue::ui_toggle);
        return Result::changed;
    }
    const bool toggle = input.is_pressed(Action::confirm) || input.nav == Direction::left ||
                        input.nav == Direction::right;
    if (toggle)
    {
        bool &flag = focus_ == kReducedMotion ? settings_.reduced_motion
                     : focus_ == kSwapConfirm ? settings_.swap_confirm
                                              : settings_.show_fps;
        flag = !flag;
        cues.push_back(audio::Cue::ui_toggle);
        return Result::changed;
    }
    return Result::none;
}

void SettingsScene::draw(gfx::DrawList &list, const Fonts &fonts, const std::string &version) const
{
    using gfx::Align;
    using gfx::Color;
    list.gradient_rect({0, 0, 1920, 1080}, 0, theme::kBackgroundTop, theme::kBackgroundBottom);
    list.text(*fonts.semibold, fonts.semibold_texture, "Settings", theme::kSafeMargin, 150,
              theme::kTextDisplay, theme::kTextOnDark);
    list.text(*fonts.regular, fonts.regular_texture, "Changes are saved automatically.",
              theme::kSafeMargin, 200, theme::kTextBody, theme::kTextOnDarkMuted);

    list.rounded_rect(
        {kLeft - 20, kTop - 18 + highlight_.value * kRowHeight, kWidth + 40, kRowHeight - 2}, 20,
        Color::rgb(0xffffff, 0.10f));
    for (int row = 0; row < kRowCount; ++row)
    {
        const float y = kTop + static_cast<float>(row) * kRowHeight;
        const bool focused = row == focus_;
        list.text(*fonts.semibold, fonts.semibold_texture, row_label(row), kLeft, y + 32, 30,
                  focused ? theme::kTextOnDark : theme::kTextOnDark.with_alpha(0.8f));
        list.text(*fonts.regular, fonts.regular_texture, row_help(row), kLeft, y + 62, 20,
                  theme::kTextOnDarkMuted);
        const float right = kLeft + kWidth;
        if (row <= kInterface)
        {
            const int value = row == kMusic     ? settings_.music_volume
                              : row == kEffects ? settings_.sfx_volume
                                                : settings_.ui_volume;
            for (int step = 0; step < 10; ++step)
            {
                const float x = right - 330.0f + static_cast<float>(step) * 30.0f;
                list.rounded_rect({x, y + 12, 22, 34}, 6,
                                  step < value ? theme::kFocus : Color::rgb(0xffffff, 0.14f));
            }
            char text[8];
            std::snprintf(text, sizeof(text), "%d", value);
            list.text(*fonts.semibold, fonts.semibold_texture, text, right - 360, y + 40, 28,
                      theme::kTextOnDark, Align::right);
        }
        else if (row == kResolution)
        {
            // A value pill with the choices on either side hinted by arrows.
            const char *label = Settings::kResolutions[settings_.resolution].label;
            list.rounded_rect({right - 190, y + 10, 190, 48}, 24, Color::rgb(0xffffff, 0.16f));
            list.text(*fonts.semibold, fonts.semibold_texture, label, right - 95, y + 44, 26,
                      theme::kTextOnDark, Align::center);
            list.text(*fonts.semibold, fonts.semibold_texture, "<", right - 168, y + 43, 24,
                      theme::kTextOnDarkMuted, Align::center);
            list.text(*fonts.semibold, fonts.semibold_texture, ">", right - 22, y + 43, 24,
                      theme::kTextOnDarkMuted, Align::center);
        }
        else
        {
            const bool on = row == kReducedMotion ? settings_.reduced_motion
                            : row == kSwapConfirm ? settings_.swap_confirm
                                                  : settings_.show_fps;
            list.rounded_rect({right - 96, y + 10, 96, 48}, 24,
                              on ? Color::rgb(0x46d6b0) : Color::rgb(0xffffff, 0.16f));
            list.circle(on ? right - 24 : right - 72, y + 34, 18, Color::rgb(0xffffff));
        }
    }
    list.text(*fonts.regular, fonts.regular_texture,
              "Left / Right  Adjust      Cross  Toggle      Circle  Back",
              1920.0f - theme::kSafeMargin, 1019, theme::kTextBody, theme::kTextOnDarkMuted,
              Align::right);
    list.text(*fonts.regular, fonts.regular_texture, version, theme::kSafeMargin, 1019, 20,
              theme::kTextOnDarkMuted);
}

} // namespace ppz::ui
