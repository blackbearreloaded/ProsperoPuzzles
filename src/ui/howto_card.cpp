// ProsperoPuzzles - How to play card: rules and controls for one game.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/howto_card.hpp"

#include <algorithm>
#include <string_view>

namespace ppz::ui
{

namespace
{

constexpr float kWidth = 1180.0f;
constexpr float kPad = 56.0f;
constexpr float kBody = 26.0f;
constexpr float kLine = 38.0f;
constexpr float kParagraphGap = 14.0f;
constexpr float kHeading = 22.0f;

// Shared by every Tatham puzzle; the game's own controls come first.
constexpr const char *kSgtCommon = "D-pad moves the cursor. The left stick moves a free pointer "
                                   "for dragging. L1 undoes, R1 redoes, Options pauses.";

struct Block
{
    const gfx::Font *font;
    std::uint32_t texture;
    float size;
    gfx::Color color;
    std::vector<std::string> lines;
    float gap_after;
};

std::vector<Block> layout(const Fonts &fonts, const std::string &rules, const std::string &controls)
{
    const float width = kWidth - 2 * kPad;
    std::vector<Block> blocks;
    auto heading = [&](const char *text)
    {
        blocks.push_back(
            {fonts.semibold, fonts.semibold_texture, kHeading, theme::kInkMuted, {text}, 6.0f});
    };
    auto paragraphs = [&](const std::string &text)
    {
        std::string_view rest = text;
        while (!rest.empty())
        {
            const std::size_t end = rest.find('\n');
            const std::string_view para = rest.substr(0, end);
            blocks.push_back({fonts.regular, fonts.regular_texture, kBody, theme::kInk,
                              fonts.regular->wrap(para, kBody, width), kParagraphGap});
            rest = end == std::string_view::npos ? std::string_view{} : rest.substr(end + 1);
        }
        if (!blocks.empty())
            blocks.back().gap_after = 30.0f;
    };
    heading("HOW TO PLAY");
    paragraphs(rules);
    heading("CONTROLS");
    paragraphs(controls);
    return blocks;
}

} // namespace

void HowToCard::open(const games::GameInfo &game)
{
    title_ = game.name;
    tagline_ = game.tagline;
    rules_ = game.rules[0] != '\0' ? game.rules : game.objective;
    controls_ = game.controls;
    if (game.kind == games::Kind::sgt)
        controls_ += std::string(controls_.empty() ? "" : "\n") + kSgtCommon;
    accent_ = game.accent;
    open_ = true;
}

void HowToCard::animate(float dt)
{
    fade_.target = open_ ? 1.0f : 0.0f;
    fade_.update(dt, 16.0f);
}

void HowToCard::update(const InputFrame &input, float, std::vector<audio::Cue> &cues)
{
    if (!open_)
        return;
    if (input.is_pressed(Action::confirm) || input.is_pressed(Action::back) ||
        input.is_pressed(Action::menu))
    {
        open_ = false;
        cues.push_back(audio::Cue::ui_back);
    }
}

void HowToCard::draw(gfx::DrawList &list, const Fonts &fonts) const
{
    const float fade = fade_.value;
    if (fade <= 0.01f)
        return;
    const std::vector<Block> blocks = layout(fonts, rules_, controls_);
    float body = 0.0f;
    for (const Block &b : blocks)
        body +=
            static_cast<float>(b.lines.size()) * (b.size == kHeading ? 30.0f : kLine) + b.gap_after;
    const float header = 150.0f;
    const float footer = 84.0f;
    const float height = std::min(1000.0f, header + body + footer);
    const gfx::Rect panel{960 - kWidth * 0.5f, 540 - height * 0.5f + 28.0f * (1.0f - fade), kWidth,
                          height};

    list.push_opacity(fade);
    list.rounded_rect({0, 0, 1920, 1080}, 0, gfx::Color::rgb(0x05070f, 0.66f));
    list.shadow({panel.x, panel.y + 18, panel.w, panel.h}, 32, 48, theme::kShadow);
    list.rounded_rect(panel, 32, theme::kPaper);
    // Accent band across the top, clipped to the card's rounded corners.
    list.push_clip({panel.x, panel.y, panel.w, 12});
    list.rounded_rect(panel, 32, accent_);
    list.pop_clip();

    list.text(*fonts.semibold, fonts.semibold_texture, title_, panel.x + kPad, panel.y + 86, 48,
              theme::kInk);
    list.text(*fonts.regular, fonts.regular_texture, tagline_, panel.x + kPad, panel.y + 124, 24,
              theme::kInkMuted);

    float y = panel.y + header + 24.0f;
    list.push_clip({panel.x, panel.y + header - 10, panel.w, height - header - footer + 10});
    for (const Block &b : blocks)
    {
        const float step = b.size == kHeading ? 30.0f : kLine;
        for (const std::string &line : b.lines)
        {
            list.text(*b.font, b.texture, line, panel.x + kPad, y, b.size, b.color);
            y += step;
        }
        y += b.gap_after;
    }
    list.pop_clip();

    const float fy = panel.y + height - 44.0f;
    draw_face_button(list, FaceButton::circle, panel.x + panel.w - kPad - 110, fy - 9, 36);
    list.text(*fonts.semibold, fonts.semibold_texture, "Close", panel.x + panel.w - kPad - 84, fy,
              24, theme::kInk);
    list.pop_opacity();
}

} // namespace ppz::ui
