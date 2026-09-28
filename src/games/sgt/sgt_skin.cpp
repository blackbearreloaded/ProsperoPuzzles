// ProsperoPuzzles - Modern skin for the Tatham puzzles: palette and shape styling.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/sgt/sgt_skin.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ppz::sgt
{

namespace
{

using gfx::Color;

// Shared with Tenfold: board, raised tile and ink.
constexpr std::uint32_t kBoard = 0xe6e3dc;
constexpr std::uint32_t kTile = 0xfffefa;
constexpr std::uint32_t kInk = 0x28334f;

// Saturated colours snap to the nearest of these hues.
struct Swatch
{
    float hue; // degrees
    std::uint32_t rgb;
};
constexpr Swatch kSwatches[] = {
    {0.0f, 0xe46f6f},   // red
    {25.0f, 0xee8f5a},  // orange
    {50.0f, 0xf0c555},  // yellow
    {100.0f, 0x7eb85c}, // green
    {160.0f, 0x5fb8a4}, // teal
    {190.0f, 0x56b8d6}, // cyan
    {228.0f, 0x4b8ed4}, // blue
    {258.0f, 0x7775c5}, // indigo
    {288.0f, 0xae75bb}, // purple
    {330.0f, 0xd97799}, // pink
};

Color mix(Color a, Color b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, 1.0f};
}

float luminance(Color c)
{
    return 0.299f * c.r + 0.587f * c.g + 0.114f * c.b;
}

void hue_sat_light(Color c, float *h, float *s, float *l)
{
    const float hi = std::max({c.r, c.g, c.b});
    const float lo = std::min({c.r, c.g, c.b});
    *l = (hi + lo) * 0.5f;
    const float d = hi - lo;
    *s = d; // chroma: what matters here is how colourful, not HSL saturation
    if (d < 1e-5f)
    {
        *h = 0.0f;
        return;
    }
    float hue;
    if (hi == c.r)
        hue = std::fmod((c.g - c.b) / d, 6.0f);
    else if (hi == c.g)
        hue = (c.b - c.r) / d + 2.0f;
    else
        hue = (c.r - c.g) / d + 4.0f;
    hue *= 60.0f;
    *h = hue < 0.0f ? hue + 360.0f : hue;
}

Color snap(Color c)
{
    float h, s, l;
    hue_sat_light(c, &h, &s, &l);
    const Swatch *best = &kSwatches[0];
    float best_d = 1e9f;
    for (const Swatch &sw : kSwatches)
    {
        float d = std::fabs(h - sw.hue);
        d = std::min(d, 360.0f - d);
        if (d < best_d)
        {
            best_d = d;
            best = &sw;
        }
    }
    const Color swatch = Color::rgb(best->rgb);
    const float swatch_l = luminance(swatch);
    const float lum = luminance(c);
    // Keep the original's light/dark role: pastels lean to paper, deep tones to ink.
    if (lum > swatch_l + 0.08f)
        return mix(swatch, Color::rgb(kTile), (lum - swatch_l) / (1.0f - swatch_l) * 0.85f);
    if (lum < swatch_l - 0.12f)
        return mix(swatch, Color::rgb(kInk), (swatch_l - lum) / swatch_l * 0.7f);
    return swatch;
}

Color grey(float lum, float background_lum)
{
    // Ink at black, the board at Tatham's background, raised tile at white.
    if (lum <= background_lum)
    {
        // Lifted mid-tones: Tatham's 50% greys read as heavy slabs otherwise.
        const float t = std::pow(lum / std::max(background_lum, 0.01f), 0.7f);
        return mix(Color::rgb(kInk), Color::rgb(kBoard), t);
    }
    return mix(Color::rgb(kBoard), Color::rgb(kTile),
               (lum - background_lum) / std::max(1.0f - background_lum, 0.01f));
}

struct Override
{
    const char *game;
    int index;
    std::uint32_t rgb;
};

// Colours the generic mapping gets wrong for a particular game.
constexpr Override kOverrides[] = {
    // Loopy / Palisade: unknown edges are drawn in a faint yellow; keep them faint.
    {"loopy", 2, 0xd3cec3},    // COL_LINEUNKNOWN
    {"palisade", 3, 0xd3cec3}, // COL_LINE_MAYBE
    // Mosaic: undecided squares are teal upstream; make them quiet tiles.
    {"mosaic", 1, 0xdad6cd}, // COL_UNMARKED
    // Pattern: quiet undecided cells, white blanks, slate filled cells.
    {"pattern", 1, 0xfffefa}, // COL_EMPTY
    {"pattern", 2, 0x3b4668}, // COL_FULL
    {"pattern", 4, 0xd8d3c8}, // COL_UNKNOWN
    // Unruly: white and slate pieces on quiet undecided cells.
    {"unruly", 1, 0xb9b3a7}, // COL_GRID
    {"unruly", 2, 0xd8d3c8}, // COL_EMPTY
    {"unruly", 3, 0xfffefa}, // COL_0
    {"unruly", 4, 0xffffff}, // COL_0_HIGHLIGHT
    {"unruly", 5, 0xe0dbd1}, // COL_0_LOWLIGHT
    {"unruly", 6, 0x4b5b86}, // COL_1
    {"unruly", 7, 0x6273a0}, // COL_1_HIGHLIGHT
    {"unruly", 8, 0x36436a}, // COL_1_LOWLIGHT
    // Black Box: the covered interior is a quiet tile, not a grey slab.
    {"blackbox", 1, 0xd8d3c8}, // COL_COVER
};

} // namespace

std::vector<gfx::Color> restyle_palette(std::string_view game_id, const std::vector<float> &rgb)
{
    std::vector<Color> out;
    if (rgb.size() < 3)
        return out;
    const Color background{rgb[0], rgb[1], rgb[2], 1.0f};
    const float background_lum = luminance(background);
    for (std::size_t i = 0; i + 2 < rgb.size(); i += 3)
    {
        const Color c{rgb[i], rgb[i + 1], rgb[i + 2], 1.0f};
        float h, s, l;
        hue_sat_light(c, &h, &s, &l);
        if (i == 0)
            out.push_back(Color::rgb(kBoard));
        else if (s < 0.14f)
            out.push_back(grey(luminance(c), background_lum));
        else
            out.push_back(snap(c));
    }
    for (const Override &o : kOverrides)
    {
        if (game_id == o.game && o.index >= 0 && o.index < static_cast<int>(out.size()))
            out[static_cast<std::size_t>(o.index)] = Color::rgb(o.rgb);
    }
    return out;
}

Style style_for(std::string_view game_id)
{
    Style style;
    // Tile games: every piece is its own card, like 2048 and Tenfold.
    constexpr std::string_view kTiles[] = {"fifteen", "sixteen", "twiddle",
                                           "flip",    "unequal", "magnets"};
    for (std::string_view id : kTiles)
    {
        if (game_id == id)
            style.round_min_fraction = 0.06f;
    }
    constexpr std::string_view kCards[] = {"fifteen", "sixteen"};
    for (std::string_view id : kCards)
    {
        if (game_id == id)
            style.bevel_cards = true;
    }
    constexpr std::string_view kDiscs[] = {"pegs",     "guess",   "bridges",  "pearl",
                                           "untangle", "inertia", "galaxies", "slant"};
    for (std::string_view id : kDiscs)
    {
        if (game_id == id)
            style.flat_discs = true;
    }
    return style;
}

} // namespace ppz::sgt
