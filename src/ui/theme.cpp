// ProsperoPuzzles - Visual theme tokens and shared UI resources.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/theme.hpp"

namespace ppz::ui
{

void draw_face_button(gfx::DrawList &list, FaceButton button, float cx, float cy, float size)
{
    const float half = size * 0.5f;
    const float stroke = size * 0.11f;
    // Dark disc behind the symbol, like the physical button.
    list.circle(cx, cy, half, gfx::Color::rgb(0x11131f, 0.92f));
    const float inner = half * 0.52f;
    switch (button)
    {
    case FaceButton::cross:
        list.line(cx - inner, cy - inner, cx + inner, cy + inner, stroke, theme::kCross);
        list.line(cx - inner, cy + inner, cx + inner, cy - inner, stroke, theme::kCross);
        break;
    case FaceButton::circle:
        list.ring(cx, cy, inner + stroke * 0.3f, stroke, theme::kCircle);
        break;
    case FaceButton::square:
    {
        const float s = inner * 1.7f;
        list.bordered_rect({cx - s * 0.5f, cy - s * 0.5f, s, s}, stroke * 0.3f,
                           theme::kSquare.with_alpha(0.0f), stroke, theme::kSquare);
        break;
    }
    case FaceButton::triangle:
    {
        const float w = inner * 2.1f;
        const float h = inner * 1.85f;
        list.triangle({cx - w * 0.5f, cy - h * 0.58f, w, h}, theme::kTriangle, stroke);
        break;
    }
    }
}

namespace
{

const gfx::Color kGlyphBody = gfx::Color::rgb(0x11131f, 0.92f);
const gfx::Color kGlyphEdge = gfx::Color::rgb(0x6c7196, 0.9f);
const gfx::Color kGlyphInk = gfx::Color::rgb(0xe4e3f5);

void glyph_label(gfx::DrawList &list, const Fonts &fonts, const char *text, float cx, float cy,
                 float size)
{
    list.text(*fonts.semibold, fonts.semibold_texture, text, cx, cy + size * 0.36f, size, kGlyphInk,
              gfx::Align::center);
}

// A thumbstick seen from above: cap, grip ring and four direction ticks.
void draw_stick(gfx::DrawList &list, const Fonts &fonts, const char *letter, float cx, float cy,
                float size)
{
    const float half = size * 0.5f;
    list.circle(cx, cy, half, kGlyphBody);
    list.ring(cx, cy, half - 1.0f, 1.5f, kGlyphEdge);
    list.ring(cx, cy, half * 0.6f, size * 0.055f, kGlyphInk.with_alpha(0.85f));
    const float t = size * 0.075f; // tick half-width
    const float r = half * 0.9f;   // tick tip distance
    const float b = r - t * 1.3f;  // tick base distance
    const gfx::Color tick = kGlyphInk.with_alpha(0.75f);
    const float up[] = {cx, cy - r, cx + t, cy - b, cx - t, cy - b};
    const float down[] = {cx, cy + r, cx - t, cy + b, cx + t, cy + b};
    const float left[] = {cx - r, cy, cx - b, cy - t, cx - b, cy + t};
    const float right[] = {cx + r, cy, cx + b, cy + t, cx + b, cy - t};
    list.polygon(up, 3, tick);
    list.polygon(down, 3, tick);
    list.polygon(left, 3, tick);
    list.polygon(right, 3, tick);
    glyph_label(list, fonts, letter, cx, cy, size * 0.34f);
}

} // namespace

float button_width(Button button, float size)
{
    switch (button)
    {
    case Button::l1:
    case Button::r1:
        return size * 1.45f;
    case Button::l2:
    case Button::r2:
        return size * 1.3f;
    case Button::options:
        return size * 1.05f;
    case Button::touchpad:
        return size * 1.6f;
    case Button::none:
        return 0.0f;
    default:
        return size;
    }
}

void draw_button(gfx::DrawList &list, const Fonts &fonts, Button button, float x, float cy,
                 float size)
{
    const float w = button_width(button, size);
    const float cx = x + w * 0.5f;
    switch (button)
    {
    case Button::none:
        return;
    case Button::cross:
        draw_face_button(list, FaceButton::cross, cx, cy, size);
        return;
    case Button::circle:
        draw_face_button(list, FaceButton::circle, cx, cy, size);
        return;
    case Button::square:
        draw_face_button(list, FaceButton::square, cx, cy, size);
        return;
    case Button::triangle:
        draw_face_button(list, FaceButton::triangle, cx, cy, size);
        return;
    case Button::l1:
    case Button::r1:
    {
        // Bumper: a low, wide key with a bright top lip.
        const float h = size * 0.74f;
        const gfx::Rect r{x, cy - h * 0.5f, w, h};
        list.bordered_rect(r, h * 0.34f, kGlyphBody, 1.5f, kGlyphEdge);
        list.line(x + h * 0.45f, r.y + 3.0f, x + w - h * 0.45f, r.y + 3.0f, 2.0f,
                  kGlyphInk.with_alpha(0.5f));
        glyph_label(list, fonts, button == Button::l1 ? "L1" : "R1", cx, cy + 1.0f, size * 0.4f);
        return;
    }
    case Button::l2:
    case Button::r2:
    {
        // Trigger: taller, with a lit, rounded top like the DualSense trigger.
        const float h = size * 0.92f;
        const gfx::Rect r{x, cy - h * 0.5f, w, h};
        list.bordered_rect(r, h * 0.32f, kGlyphBody, 1.5f, kGlyphEdge);
        list.rounded_rect({x + 4, r.y + 4, w - 8, h * 0.24f}, h * 0.12f,
                          kGlyphInk.with_alpha(0.16f));
        glyph_label(list, fonts, button == Button::l2 ? "L2" : "R2", cx, cy + 3.0f, size * 0.4f);
        return;
    }
    case Button::options:
    {
        // Options: a small pill carrying the three-line menu mark.
        const float h = size * 0.7f;
        list.bordered_rect({x, cy - h * 0.5f, w, h}, h * 0.5f, kGlyphBody, 1.5f, kGlyphEdge);
        const float half = w * 0.2f;
        const float stroke = size * 0.07f;
        for (int i = -1; i <= 1; ++i)
        {
            const float y = cy + static_cast<float>(i) * size * 0.13f;
            list.line(cx - half, y, cx + half, y, stroke, kGlyphInk);
        }
        return;
    }
    case Button::left_stick:
        draw_stick(list, fonts, "L", cx, cy, size);
        return;
    case Button::right_stick:
        draw_stick(list, fonts, "R", cx, cy, size);
        return;
    case Button::dpad:
    {
        const float half = size * 0.5f;
        list.circle(cx, cy, half, kGlyphBody);
        list.ring(cx, cy, half - 1.0f, 1.5f, kGlyphEdge);
        const float arm = size * 0.3f;
        const float thick = size * 0.2f;
        list.rounded_rect({cx - thick * 0.5f, cy - arm, thick, arm * 2}, thick * 0.3f, kGlyphInk);
        list.rounded_rect({cx - arm, cy - thick * 0.5f, arm * 2, thick}, thick * 0.3f, kGlyphInk);
        list.circle(cx, cy, thick * 0.22f, kGlyphBody);
        return;
    }
    case Button::touchpad:
    {
        const float h = size * 0.72f;
        list.bordered_rect({x, cy - h * 0.5f, w, h}, h * 0.22f, kGlyphBody, 1.5f, kGlyphEdge);
        list.line(cx, cy - h * 0.3f, cx, cy + h * 0.3f, 1.5f, kGlyphEdge);
        return;
    }
    }
}

float draw_hints(gfx::DrawList &list, const Fonts &fonts, const Hint *hints, int count, float x,
                 bool right_align, gfx::Color label_color)
{
    constexpr float kSize = 40.0f;
    constexpr float kIconY = 1010.0f;
    constexpr float kBaseline = 1019.0f;
    constexpr float kIconGap = 12.0f;
    constexpr float kPairGap = 6.0f;
    constexpr float kItemGap = 44.0f;
    auto item_width = [&](const Hint &h)
    {
        float w = button_width(h.button, kSize) + kIconGap +
                  fonts.regular->measure(h.label, theme::kTextBody);
        if (h.second != Button::none)
            w += kPairGap + button_width(h.second, kSize);
        return w;
    };
    float total = 0.0f;
    for (int i = 0; i < count; ++i)
        total += item_width(hints[i]) + (i + 1 < count ? kItemGap : 0.0f);
    float cursor = right_align ? x - total : x;
    for (int i = 0; i < count; ++i)
    {
        const Hint &h = hints[i];
        draw_button(list, fonts, h.button, cursor, kIconY, kSize);
        cursor += button_width(h.button, kSize);
        if (h.second != Button::none)
        {
            cursor += kPairGap;
            draw_button(list, fonts, h.second, cursor, kIconY, kSize);
            cursor += button_width(h.second, kSize);
        }
        cursor += kIconGap;
        list.text(*fonts.regular, fonts.regular_texture, h.label, cursor, kBaseline,
                  theme::kTextBody, label_color);
        cursor += fonts.regular->measure(h.label, theme::kTextBody) + kItemGap;
    }
    return total;
}

} // namespace ppz::ui
