// ProsperoPuzzles - Visual theme tokens and shared UI resources.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "gfx/draw_list.hpp"
#include "gfx/font.hpp"

#include <cstdint>

namespace ppz::ui
{

struct Fonts
{
    const gfx::Font *regular = nullptr;
    const gfx::Font *semibold = nullptr;
    std::uint32_t regular_texture = 0;
    std::uint32_t semibold_texture = 0;
};

namespace theme
{
// Palette: deep night background, warm paper cards, one accent per game.
inline const gfx::Color kBackgroundTop = gfx::Color::rgb(0x0c1330);
inline const gfx::Color kBackgroundBottom = gfx::Color::rgb(0x1d1240);
inline const gfx::Color kPaper = gfx::Color::rgb(0xf4f1ea);
inline const gfx::Color kPaperShade = gfx::Color::rgb(0xe6e1d6);
inline const gfx::Color kInk = gfx::Color::rgb(0x1b1d2b);
inline const gfx::Color kInkMuted = gfx::Color::rgb(0x6b6f82);
inline const gfx::Color kTextOnDark = gfx::Color::rgb(0xf5f3ff);
inline const gfx::Color kTextOnDarkMuted = gfx::Color::rgb(0xa9a8c8);
inline const gfx::Color kShadow = gfx::Color::rgb(0x000000, 0.45f);
inline const gfx::Color kFocus = gfx::Color::rgb(0xffd166);
inline const gfx::Color kFavorite = gfx::Color::rgb(0xffb400);

// DualSense face-button colours used for on-screen hints.
inline const gfx::Color kCross = gfx::Color::rgb(0x7fa7ff);
inline const gfx::Color kCircle = gfx::Color::rgb(0xff6b7d);
inline const gfx::Color kSquare = gfx::Color::rgb(0xe98fd8);
inline const gfx::Color kTriangle = gfx::Color::rgb(0x46d6b0);

constexpr float kRadiusCard = 28.0f;
constexpr float kRadiusChip = 14.0f;
constexpr float kSafeMargin = 96.0f; // ~5% TV safe area at 1920x1080

// Type scale (pixel sizes at 1080p).
constexpr float kTextDisplay = 72.0f;
constexpr float kTextTitle = 44.0f;
constexpr float kTextHeading = 32.0f;
constexpr float kTextBody = 26.0f;
constexpr float kTextCaption = 20.0f;
} // namespace theme

enum class FaceButton : std::uint8_t
{
    cross,
    circle,
    square,
    triangle,
};

// Draws the PlayStation face-button symbol centred at (cx, cy).
void draw_face_button(gfx::DrawList &list, FaceButton button, float cx, float cy, float size);

// Every controller input the hints show.
enum class Button : std::uint8_t
{
    none,
    cross,
    circle,
    square,
    triangle,
    l1,
    r1,
    l2,
    r2,
    options,
    left_stick,
    right_stick,
    dpad,
    touchpad,
};

// Width of a button glyph drawn at the given height.
float button_width(Button button, float size);
// Draws a controller glyph with its left edge at x, vertically centred on cy.
void draw_button(gfx::DrawList &list, const Fonts &fonts, Button button, float x, float cy,
                 float size);

struct Hint
{
    Button button;
    const char *label;
    Button second = Button::none; // pairs such as L2 / R2
};

// Draws a row of [glyph label] hints on the 1080p hint line. With
// right_align the row ends at x; otherwise it starts there. Returns its width.
float draw_hints(gfx::DrawList &list, const Fonts &fonts, const Hint *hints, int count, float x,
                 bool right_align, gfx::Color label_color = theme::kTextOnDark);

} // namespace ppz::ui
