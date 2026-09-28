// ProsperoPuzzles - System presentation art (icon0, pic0/pic1) drawn with the app renderer.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "art.hpp"

#include "app/thumbnails.hpp"
#include "games/registry.hpp"
#include "ui/theme.hpp"

#include <cmath>

namespace ppz::host
{

namespace
{

using gfx::Align;
using gfx::Color;

// Tenfold's tile colours.
const std::uint32_t kTiles[] = {0x469fca, 0x64b8a6, 0x7775c5, 0xae75bb, 0xd97799,
                                0xed836d, 0xdf9a43, 0x7eac59, 0x3c9290};

void backdrop(gfx::DrawList &list, const gfx::Rect &area)
{
    list.gradient_rect(area, 0, ui::theme::kBackgroundTop, ui::theme::kBackgroundBottom);
    // Soft coloured glows, like the library.
    list.shadow({area.x + area.w * 0.62f, area.y + area.h * 0.18f, area.w * 0.3f, area.h * 0.5f},
                area.h * 0.25f, area.h * 0.3f, Color::rgb(0x7775c5, 0.30f));
    list.shadow({area.x + area.w * 0.05f, area.y + area.h * 0.55f, area.w * 0.3f, area.h * 0.4f},
                area.h * 0.2f, area.h * 0.3f, Color::rgb(0x469fca, 0.22f));
}

// A Tenfold-style tile with a white numeral or star.
void tile(gfx::DrawList &list, const ui::Fonts &fonts, float x, float y, float size,
          std::uint32_t rgb, const char *label, bool star)
{
    list.shadow({x + size * 0.04f, y + size * 0.1f, size * 0.92f, size}, size * 0.2f, size * 0.12f,
                Color::rgb(0x000000, 0.35f));
    list.rounded_rect({x, y, size, size}, size * 0.2f, Color::rgb(rgb));
    list.rounded_rect({x + size * 0.06f, y + size * 0.05f, size * 0.88f, size * 0.38f},
                      size * 0.16f, Color::rgb(0xffffff, 0.10f));
    if (star)
        list.star(x + size * 0.5f, y + size * 0.52f, size * 0.3f, Color::rgb(0xfffefa));
    else if (label != nullptr)
        list.text(*fonts.semibold, fonts.semibold_texture, label, x + size * 0.5f,
                  y + size * 0.5f + size * 0.19f, size * 0.52f, Color::rgb(0xfffefa),
                  Align::center);
}

} // namespace

bool render_art(gfx::DrawList &list, const ui::Fonts &fonts, app::Thumbnails &thumbnails,
                const std::function<bool(const char *)> &write)
{
    bool ok = true;

    // icon0: a 3x3 board of tiles in the centred 1080x1080 square (cropped later).
    list.clear();
    const gfx::Rect square{420, 0, 1080, 1080};
    list.gradient_rect(square, 0, Color::rgb(0x18204a), Color::rgb(0x2a1a55));
    list.shadow({520, 180, 880, 760}, 300, 260, Color::rgb(0x7775c5, 0.35f));
    const float cell = 270.0f;
    const float gap = 34.0f;
    const float origin_x = 960.0f - (3 * cell + 2 * gap) * 0.5f;
    const float origin_y = 540.0f - (3 * cell + 2 * gap) * 0.5f;
    struct Spec
    {
        std::uint32_t rgb;
        const char *label;
        bool star;
    };
    const Spec icon[9] = {
        {0x469fca, "2", false}, {0x64b8a6, "4", false},    {0x7775c5, "8", false},
        {0xd97799, "5", false}, {0xf0c555, nullptr, true}, {0xed836d, "9", false},
        {0x7eac59, "3", false}, {0xae75bb, "7", false},    {0x3c9290, "1", false}};
    for (int i = 0; i < 9; ++i)
    {
        const float x = origin_x + static_cast<float>(i % 3) * (cell + gap);
        const float y = origin_y + static_cast<float>(i / 3) * (cell + gap);
        tile(list, fonts, x, y, cell, icon[i].rgb, icon[i].label, icon[i].star);
    }
    ok = write("art-icon") && ok;

    // pic0 / pic1: title on the left, a drift of real puzzle cards on the right.
    list.clear();
    const gfx::Rect full{0, 0, 1920, 1080};
    backdrop(list, full);
    // The most colourful boards, so the wall reads as the collection at a glance.
    const char *const wall[16] = {"flood",    "net",      "fifteen", "map",     "g2048", "lightup",
                                  "bridges",  "samegame", "tents",   "tenfold", "solo",  "pegs",
                                  "signpost", "pattern",  "magnets", "unruly"};
    const float card_w = 300.0f;
    const float card_h = 250.0f;
    int n = 0;
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column, ++n)
        {
            const games::GameInfo &game = *games::find(wall[n]);
            // Rows step right as they go down.
            const float x = 880.0f + static_cast<float>(column) * (card_w + 36.0f) +
                            static_cast<float>(row % 2) * 150.0f - 60.0f;
            const float y = 70.0f + static_cast<float>(row) * (card_h + 36.0f) - 40.0f;
            list.push_opacity(1.0f);
            list.shadow({x + 10, y + 22, card_w - 20, card_h}, 26, 34, ui::theme::kShadow);
            list.rounded_rect({x, y, card_w, card_h}, 26, Color::rgb(0xe6e3dc));
            std::uint32_t texture = 0;
            float tw = 0.0f;
            float th = 0.0f;
            if (thumbnails.thumbnail(game.id, &texture, &tw, &th))
            {
                const float scale = std::min((card_w - 40) / tw, (card_h - 40) / th);
                const float w = tw * scale;
                const float h = th * scale;
                list.image(texture, {x + (card_w - w) * 0.5f, y + (card_h - h) * 0.5f, w, h},
                           {0.0f, 1.0f, 1.0f, -1.0f}, Color{1, 1, 1, 1});
            }
            else
            {
                // 2048 and Tenfold: a small board of their own tiles.
                const bool g2048 = game.id == "g2048";
                const char *labels[4] = {g2048 ? "2" : "3", g2048 ? "8" : "5", g2048 ? "32" : "7",
                                         g2048 ? "4" : "10"};
                for (int k = 0; k < 4; ++k)
                    tile(list, fonts, x + card_w * 0.5f - 96 + static_cast<float>(k % 2) * 100,
                         y + card_h * 0.5f - 96 + static_cast<float>(k / 2) * 100, 92,
                         kTiles[static_cast<std::size_t>(n + k * 2) % 9], labels[k], false);
            }
            list.pop_opacity();
        }
    }
    // Vignette: the wall sinks into the night at the edges, and the title
    // side stays dark enough to read.
    list.shadow({1760, -200, 500, 1480}, 200, 220, Color::rgb(0x0a0f28, 0.75f));
    list.shadow({700, -260, 1400, 260}, 100, 160, Color::rgb(0x0a0f28, 0.7f));
    list.shadow({700, 1080, 1400, 260}, 100, 160, Color::rgb(0x0a0f28, 0.7f));
    list.shadow({-200, 260, 1150, 560}, 200, 260, Color::rgb(0x0a0f28, 0.85f));
    list.text(*fonts.semibold, fonts.semibold_texture, "Prospero", 120, 470, 150,
              ui::theme::kTextOnDark);
    list.text(*fonts.semibold, fonts.semibold_texture, "Puzzles", 120, 630, 150, ui::theme::kFocus);
    list.text(*fonts.regular, fonts.regular_texture, "42 puzzles to play at your own pace", 126,
              716, 38, ui::theme::kTextOnDarkMuted);
    ok = write("art-background") && ok;
    return ok;
}

} // namespace ppz::host
