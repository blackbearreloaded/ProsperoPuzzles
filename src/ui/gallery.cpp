// ProsperoPuzzles - Visual test scene exercising every 2D primitive.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/gallery.hpp"

#include <cmath>
#include <cstdio>

namespace ppz::ui
{

namespace
{

struct Card
{
    const char *name;
    const char *tagline;
    std::uint32_t accent;
    bool favorite;
};

constexpr Card kCards[] = {
    {"2048", "Slide and merge tiles", 0xf2b179, true},
    {"Light Up", "Light-bulb placing puzzle", 0xffd166, false},
    {"Net", "Network jigsaw puzzle", 0x4cc9f0, true},
    {"Solo", "Sudoku, and more", 0x80ed99, false},
    {"Tenfold", "Merge your way to ten", 0xc77dff, false},
};

constexpr const char *kActionLabels[] = {"Up", "Dn", "Lt", "Rt", "X",   "O",   "Tri", "Sq",
                                         "L1", "R1", "L2", "R2", "Opt", "Pad", "L3",  "R3"};

} // namespace

void draw_gallery(gfx::DrawList &list, const Fonts &fonts, const GalleryState &state)
{
    using gfx::Align;
    using gfx::Color;
    const float t = static_cast<float>(state.seconds);

    list.gradient_rect({0, 0, 1920, 1080}, 0, theme::kBackgroundTop, theme::kBackgroundBottom);
    // Slow drifting soft shapes in the background.
    for (int i = 0; i < 6; ++i)
    {
        const float phase = t * 0.15f + static_cast<float>(i) * 1.7f;
        const float x = 200.0f + static_cast<float>(i) * 300.0f + 60.0f * std::sin(phase);
        const float y = 820.0f + 40.0f * std::cos(phase * 1.3f);
        list.shadow({x, y, 180, 180}, 44, 60, Color::rgb(0x6d5cff, 0.10f));
    }

    const float left = theme::kSafeMargin;
    list.text(*fonts.semibold, fonts.semibold_texture, "ProsperoPuzzles", left, 150,
              theme::kTextDisplay, theme::kTextOnDark);
    list.text(*fonts.regular, fonts.regular_texture,
              "42 games  \xC2\xB7  sorted A\xE2\x80\x93Z  \xC2\xB7  favorites first", left, 200,
              theme::kTextBody, theme::kTextOnDarkMuted);

    // Game cards; the focused one lifts and scales.
    const float card_w = 320.0f;
    const float card_h = 300.0f;
    for (int index = 0; index < 5; ++index)
    {
        const Card &card = kCards[index];
        const float x = left + static_cast<float>(index) * (card_w + 36.0f);
        const float y = 270.0f;
        const bool focused = index == state.focused_card;
        const float lift = focused ? 1.06f + 0.01f * std::sin(t * 3.0f) : 1.0f;
        list.push_transform(lift, x + card_w * 0.5f, y + card_h * 0.5f, 0, focused ? -8.0f : 0.0f);
        list.shadow({x + 6, y + 18, card_w - 12, card_h}, theme::kRadiusCard,
                    focused ? 34.0f : 22.0f, theme::kShadow.with_alpha(focused ? 1.0f : 0.7f));
        list.gradient_rect({x, y, card_w, card_h}, theme::kRadiusCard, theme::kPaper,
                           theme::kPaperShade);
        list.rounded_rect({x + 24, y + 24, card_w - 48, 150}, 18, Color::rgb(card.accent, 0.9f));
        // A tiny board preview: 3x3 tiles on the accent panel.
        for (int cell = 0; cell < 9; ++cell)
        {
            const float cx = x + 60.0f + static_cast<float>(cell % 3) * 70.0f;
            const float cy = y + 40.0f + static_cast<float>(cell / 3) * 42.0f;
            list.rounded_rect({cx, cy, 58, 34}, 8,
                              Color::rgb(0xffffff, 0.35f + 0.05f * static_cast<float>(cell % 4)));
        }
        list.text(*fonts.semibold, fonts.semibold_texture, card.name, x + 24, y + 222,
                  theme::kTextHeading, theme::kInk);
        list.text(*fonts.regular, fonts.regular_texture, card.tagline, x + 24, y + 258,
                  theme::kTextCaption, theme::kInkMuted);
        if (card.favorite)
            list.circle(x + card_w - 44, y + 212, 12, theme::kFavorite);
        if (focused)
            list.bordered_rect({x - 6, y - 6, card_w + 12, card_h + 12}, theme::kRadiusCard + 6,
                               theme::kFocus.with_alpha(0.0f), 4, theme::kFocus);
        list.pop_transform();
    }

    // Type ramp.
    const float sizes[] = {theme::kTextCaption, theme::kTextBody, theme::kTextHeading,
                           theme::kTextTitle};
    float baseline = 690.0f;
    for (float size : sizes)
    {
        char line[64];
        std::snprintf(line, sizeof(line), "Inter %.0f px  AaBbCc 0123456789",
                      static_cast<double>(size));
        list.text(*fonts.regular, fonts.regular_texture, line, left, baseline, size,
                  theme::kTextOnDark);
        baseline += size + 16.0f;
    }

    // Primitives: capsule lines, rings, triangles.
    const float px = 1150.0f;
    for (int i = 0; i < 5; ++i)
    {
        const float a = t * 0.8f + static_cast<float>(i) * 0.6f;
        list.line(px, 700, px + 160.0f * std::cos(a), 700 + 160.0f * std::sin(a),
                  2.0f + static_cast<float>(i) * 2.0f, Color::rgb(0x4cc9f0, 0.9f));
    }
    list.ring(px + 330, 700, 70, 8, Color::rgb(0xff6b7d));
    list.circle(px + 330, 700, 30, Color::rgb(0xffd166));
    list.triangle({px + 470, 630, 150, 140}, Color::rgb(0x46d6b0));
    list.triangle({px + 470, 630, 150, 140}, Color::rgb(0xffffff), 5);

    // Controls hint bar with drawn face-button symbols.
    const float bar_y = 1000.0f;
    struct Hint
    {
        FaceButton button;
        const char *label;
    };
    const Hint hints[] = {{FaceButton::cross, "Play"},
                          {FaceButton::square, "Favorite"},
                          {FaceButton::triangle, "Details"},
                          {FaceButton::circle, "Back"}};
    float hx = 1920.0f - theme::kSafeMargin;
    for (int i = 3; i >= 0; --i)
    {
        const float width = fonts.regular->measure(hints[i].label, theme::kTextBody);
        list.text(*fonts.regular, fonts.regular_texture, hints[i].label, hx, bar_y + 9,
                  theme::kTextBody, theme::kTextOnDark, Align::right);
        draw_face_button(list, hints[i].button, hx - width - 28.0f, bar_y, 40.0f);
        hx -= width + 90.0f;
    }

    // Live input strip.
    for (int index = 0; index < 16; ++index)
    {
        const bool lit = (state.held_actions & (1u << static_cast<unsigned>(index))) != 0;
        const float x = left + static_cast<float>(index) * 52.0f;
        list.rounded_rect({x, 960, 46, 46}, 10,
                          lit ? Color::rgb(0x46d6b0) : Color::rgb(0xffffff, 0.08f));
        list.text(*fonts.semibold, fonts.semibold_texture, kActionLabels[index], x + 23, 990, 15,
                  lit ? theme::kInk : theme::kTextOnDarkMuted, Align::center);
    }
    if (state.status != nullptr && state.status[0] != '\0')
        list.text(*fonts.regular, fonts.regular_texture, state.status, left, 1050,
                  theme::kTextCaption, theme::kTextOnDarkMuted);
}

} // namespace ppz::ui
