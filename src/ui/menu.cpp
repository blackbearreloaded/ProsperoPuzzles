// ProsperoPuzzles - Modal menu overlay (pause, win and game-over dialogs).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/menu.hpp"

namespace ppz::ui
{

namespace
{

constexpr float kRowHeight = 72.0f;
constexpr float kPanelWidth = 620.0f;

} // namespace

void Menu::open(std::string title, std::vector<Item> items, std::string subtitle)
{
    title_ = std::move(title);
    subtitle_ = std::move(subtitle);
    items_ = std::move(items);
    focus_ = 0;
    while (focus_ < static_cast<int>(items_.size()) &&
           !items_[static_cast<std::size_t>(focus_)].enabled)
        ++focus_;
    highlight_.snap(static_cast<float>(focus_));
    open_ = true;
}

void Menu::close()
{
    open_ = false;
}

int Menu::update(const InputFrame &input, float dt, std::vector<audio::Cue> &cues, bool cancellable)
{
    fade_.target = open_ ? 1.0f : 0.0f;
    fade_.update(dt, 16.0f);
    highlight_.target = static_cast<float>(focus_);
    highlight_.update(dt, 20.0f);
    if (!open_ || items_.empty())
        return kNone;
    const int count = static_cast<int>(items_.size());
    if (input.nav == Direction::up || input.nav == Direction::down)
    {
        const int step = input.nav == Direction::up ? count - 1 : 1;
        int next = focus_;
        for (int tries = 0; tries < count; ++tries)
        {
            next = (next + step) % count;
            if (items_[static_cast<std::size_t>(next)].enabled)
                break;
        }
        if (next != focus_)
        {
            focus_ = next;
            cues.push_back(audio::Cue::ui_focus);
        }
    }
    if (input.is_pressed(Action::confirm))
    {
        open_ = false;
        cues.push_back(audio::Cue::ui_select);
        return items_[static_cast<std::size_t>(focus_)].id;
    }
    if (cancellable && (input.is_pressed(Action::back) || input.is_pressed(Action::menu)))
    {
        open_ = false;
        cues.push_back(audio::Cue::ui_pause_close);
        return kCancelled;
    }
    return kNone;
}

void Menu::draw(gfx::DrawList &list, const Fonts &fonts) const
{
    const float fade = fade_.value;
    if (fade <= 0.01f)
        return;
    list.push_opacity(fade);
    list.rounded_rect({0, 0, 1920, 1080}, 0, gfx::Color::rgb(0x05070f, 0.6f));
    const float header = subtitle_.empty() ? 96.0f : 132.0f;
    const float height = header + kRowHeight * static_cast<float>(items_.size()) + 24.0f;
    const gfx::Rect panel{960 - kPanelWidth * 0.5f, 540 - height * 0.5f + 24.0f * (1.0f - fade),
                          kPanelWidth, height};
    list.shadow({panel.x, panel.y + 18, panel.w, panel.h}, 30, 44, theme::kShadow);
    list.rounded_rect(panel, 30, theme::kPaper);
    list.text(*fonts.semibold, fonts.semibold_texture, title_, panel.x + 44, panel.y + 66, 38,
              theme::kInk);
    if (!subtitle_.empty())
        list.text(*fonts.regular, fonts.regular_texture, subtitle_, panel.x + 44, panel.y + 106, 24,
                  theme::kInkMuted);
    const float first = panel.y + header;
    list.rounded_rect(
        {panel.x + 20, first + highlight_.value * kRowHeight, panel.w - 40, kRowHeight - 8}, 18,
        gfx::Color::rgb(0x1b1d2b));
    for (std::size_t i = 0; i < items_.size(); ++i)
    {
        const bool focused = static_cast<int>(i) == focus_;
        const float y = first + static_cast<float>(i) * kRowHeight;
        list.text(*fonts.semibold, fonts.semibold_texture, items_[i].label, panel.x + 52, y + 44,
                  28,
                  focused              ? theme::kTextOnDark
                  : !items_[i].enabled ? theme::kInkMuted.with_alpha(0.45f)
                                       : theme::kInk);
    }
    list.pop_opacity();
}

} // namespace ppz::ui
