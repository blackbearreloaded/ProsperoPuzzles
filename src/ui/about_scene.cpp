// ProsperoPuzzles - About screen: credits, sources and version.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/about_scene.hpp"

namespace ppz::ui
{

namespace
{

constexpr float kTop = 262.0f;
constexpr float kBottom = 930.0f;
constexpr float kCreditsLeft = theme::kSafeMargin;
constexpr float kCreditsWidth = 1010.0f;
constexpr float kInfoLeft = kCreditsLeft + kCreditsWidth + 36.0f;
constexpr float kInfoWidth = 1920.0f - theme::kSafeMargin - kInfoLeft;
constexpr float kPad = 48.0f;

const gfx::Color kPanel = gfx::Color::rgb(0xffffff, 0.07f);
const gfx::Color kDivider = gfx::Color::rgb(0xffffff, 0.14f);

struct Writer
{
    gfx::DrawList &list;
    const Fonts &fonts;
    float x;
    float y;

    void kicker(const char *text)
    {
        list.text(*fonts.semibold, fonts.semibold_texture, text, x, y, 20, theme::kFocus);
        y += 52;
    }
    void lead(const char *text)
    {
        list.text(*fonts.semibold, fonts.semibold_texture, text, x, y, theme::kTextHeading,
                  theme::kTextOnDark);
        y += 50;
    }
    void body(const char *text)
    {
        list.text(*fonts.regular, fonts.regular_texture, text, x, y, 24, theme::kTextOnDark);
        y += 36;
    }
    void link(const char *text)
    {
        y += 6;
        list.text(*fonts.semibold, fonts.semibold_texture, text, x, y, 22, theme::kCross);
        y += 36;
    }
    void divider(float width)
    {
        y += 14;
        list.rounded_rect({x, y, width, 2}, 1, kDivider);
        y += 58;
    }
    void item(const char *name, const char *detail)
    {
        list.text(*fonts.semibold, fonts.semibold_texture, name, x, y, 24, theme::kTextOnDark);
        list.text(*fonts.regular, fonts.regular_texture, detail, x, y + 30, 20,
                  theme::kTextOnDarkMuted);
        y += 70;
    }
};

} // namespace

bool AboutScene::update(const InputFrame &input, std::vector<audio::Cue> &cues)
{
    if (input.is_pressed(Action::back) || input.is_pressed(Action::menu) ||
        input.is_pressed(Action::confirm))
    {
        cues.push_back(audio::Cue::ui_back);
        return true;
    }
    return false;
}

void AboutScene::draw(gfx::DrawList &list, const Fonts &fonts, const std::string &version) const
{
    list.gradient_rect({0, 0, 1920, 1080}, 0, theme::kBackgroundTop, theme::kBackgroundBottom);
    list.text(*fonts.semibold, fonts.semibold_texture, "About ProsperoPuzzles", theme::kSafeMargin,
              150, theme::kTextDisplay, theme::kTextOnDark);
    list.text(*fonts.regular, fonts.regular_texture, "Credits and sources", theme::kSafeMargin, 200,
              theme::kTextBody, theme::kTextOnDarkMuted);

    // Project credits: where the puzzles come from, and who made this edition.
    list.rounded_rect({kCreditsLeft, kTop, kCreditsWidth, kBottom - kTop}, theme::kRadiusCard,
                      kPanel);
    Writer credits{list, fonts, kCreditsLeft + kPad, kTop + kPad + 20};
    credits.kicker("PROJECT CREDITS");
    credits.lead("Puzzles by Simon Tatham");
    credits.body("40 of the puzzles come from Simon Tatham's Portable Puzzle");
    credits.body("Collection. All credit for their design and original code goes");
    credits.body("to Simon Tatham and the collection's contributors.");
    credits.link("chiark.greenend.org.uk/~sgtatham/puzzles");
    credits.divider(kCreditsWidth - 2 * kPad);
    credits.kicker("PS5 EDITION");
    credits.body("ProsperoPuzzles is an unofficial PS5 homebrew collection");
    credits.body("brought to you by BlackBearReloaded.");
    credits.link("github.com/blackbearreloaded/ProsperoPuzzles");

    // Everything else the app is built on, plus the version.
    list.rounded_rect({kInfoLeft, kTop, kInfoWidth, kBottom - kTop}, theme::kRadiusCard, kPanel);
    Writer info{list, fonts, kInfoLeft + kPad, kTop + kPad + 20};
    info.kicker("ALSO THANKS TO");
    info.item("2048", "Original game by Gabriele Cirulli");
    info.item("ps5-opengl", "OpenGL 4.6 for PS5, built on Mesa");
    info.item("Inter", "Typeface by Rasmus Andersson");
    info.item("stb_vorbis", "Music decoding by Sean Barrett");
    info.item("Sound effects", "Created with ElevenLabs");
    info.divider(kInfoWidth - 2 * kPad);
    info.kicker("VERSION");
    list.text(*fonts.semibold, fonts.semibold_texture,
              version.empty() ? "Unknown" : version.c_str(), info.x, info.y, 26,
              theme::kTextOnDark);
    list.text(*fonts.regular, fonts.regular_texture, "Licensed under GPL-3.0-or-later", info.x,
              info.y + 34, 20, theme::kTextOnDarkMuted);

    list.text(*fonts.regular, fonts.regular_texture, "Circle  Back", 1920.0f - theme::kSafeMargin,
              1019, theme::kTextBody, theme::kTextOnDarkMuted, gfx::Align::right);
}

} // namespace ppz::ui
