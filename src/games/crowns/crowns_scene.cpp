// ProsperoPuzzles - Crowns play screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/crowns/crowns_scene.hpp"

#include "core/bytes.hpp"

#include <algorithm>
#include <cmath>

namespace ppz::crowns
{

namespace
{

constexpr int kSides[3] = {6, 7, 8};
constexpr std::uint8_t kFormat = 1;

// A pastel of the region's hue, so crowns and marks stay readable on it.
gfx::Color region_color(int region)
{
    const gfx::Color h = kit::look::hue(region);
    const gfx::Color t = kit::look::kTile;
    constexpr float k = 0.55f;
    return {t.r + (h.r - t.r) * k, t.g + (h.g - t.g) * k, t.b + (h.b - t.b) * k, 1.0f};
}

// A crown: a band with three points, each tipped with a jewel.
void draw_crown(gfx::DrawList &list, float cx, float cy, float size, gfx::Color color)
{
    const float w = size * 0.62f;
    const float h = size * 0.46f;
    const float x0 = cx - w * 0.5f;
    const float y1 = cy + h * 0.5f;
    const float y0 = cy - h * 0.5f;
    const float body[] = {x0,
                          y1,
                          x0,
                          y0 + h * 0.18f,
                          x0 + w * 0.25f,
                          y0 + h * 0.55f,
                          cx,
                          y0,
                          x0 + w * 0.75f,
                          y0 + h * 0.55f,
                          x0 + w,
                          y0 + h * 0.18f,
                          x0 + w,
                          y1};
    list.polygon(body, 7, color);
    list.rounded_rect({x0, y1 - h * 0.05f, w, h * 0.2f}, h * 0.08f, color);
    const float jewel = size * 0.055f;
    list.circle(x0, y0 + h * 0.14f, jewel, color);
    list.circle(cx, y0 - h * 0.04f, jewel, color);
    list.circle(x0 + w, y0 + h * 0.14f, jewel, color);
}

} // namespace

CrownsScene::CrownsScene(const ui::Fonts &fonts)
    : kit::PuzzleScene(fonts, {"crowns",
                               "Crowns",
                               "One crown in every row, column and colour.",
                               {"6 \xC3\x97 6", "7 \xC3\x97 7", "8 \xC3\x97 8"},
                               0})
{
}

void CrownsScene::generate(std::uint64_t seed, int size)
{
    puzzle_ = crowns::generate(seed, kSides[std::clamp(size, 0, 2)]);
    marks_.fill(kEmpty);
    pop_.fill(0.0f);
}

void CrownsScene::restart()
{
    marks_.fill(kEmpty);
}

std::string CrownsScene::serialize() const
{
    bytes::Writer w;
    w.put(kFormat);
    w.put(static_cast<std::uint8_t>(puzzle_.side));
    const int cells = puzzle_.side * puzzle_.side;
    for (int i = 0; i < cells; ++i)
        w.put(puzzle_.region[static_cast<std::size_t>(i)]);
    for (int r = 0; r < puzzle_.side; ++r)
        w.put(puzzle_.solution[static_cast<std::size_t>(r)]);
    for (int i = 0; i < cells; ++i)
        w.put(marks_[static_cast<std::size_t>(i)]);
    return w.data();
}

bool CrownsScene::deserialize(std::string_view data)
{
    bytes::Reader r(data);
    if (r.get<std::uint8_t>() != kFormat)
        return false;
    Puzzle p;
    p.side = r.get<std::uint8_t>();
    if (p.side < 5 || p.side > kMaxSide)
        return false;
    const int cells = p.side * p.side;
    for (int i = 0; i < cells; ++i)
    {
        p.region[static_cast<std::size_t>(i)] = r.get<std::uint8_t>();
        if (p.region[static_cast<std::size_t>(i)] >= p.side)
            return false;
    }
    for (int row = 0; row < p.side; ++row)
        p.solution[static_cast<std::size_t>(row)] = r.get<std::uint8_t>();
    std::array<std::uint8_t, kMaxCells> marks{};
    for (int i = 0; i < cells; ++i)
    {
        marks[static_cast<std::size_t>(i)] = r.get<std::uint8_t>();
        if (marks[static_cast<std::size_t>(i)] > kCrown)
            return false;
    }
    if (!r.finished())
        return false;
    puzzle_ = p;
    marks_ = marks;
    return true;
}

bool CrownsScene::solved() const
{
    return crowns::solved(puzzle_, marks_);
}

void CrownsScene::set_mark(int cell, std::uint8_t mark, std::vector<audio::Cue> &cues)
{
    remember();
    marks_[static_cast<std::size_t>(cell)] = mark;
    if (mark == kCrown)
    {
        pop_[static_cast<std::size_t>(cell)] = 1.0f;
        const std::vector<bool> bad = conflicts(puzzle_, marks_);
        cues.push_back(bad[static_cast<std::size_t>(cell)] ? audio::Cue::invalid
                                                           : audio::Cue::place);
    }
    else
    {
        cues.push_back(mark == kCross ? audio::Cue::mark : audio::Cue::erase);
    }
}

void CrownsScene::play(const InputFrame &input, std::vector<audio::Cue> &cues)
{
    const int cell = cursor_row() * puzzle_.side + cursor_col();
    const std::uint8_t mark = marks_[static_cast<std::size_t>(cell)];
    if (input.is_pressed(Action::confirm))
        set_mark(cell, mark == kCrown ? kEmpty : kCrown, cues);
    else if (input.is_pressed(Action::west))
        set_mark(cell, mark == kCross ? kEmpty : kCross, cues);
}

void CrownsScene::animate(float dt)
{
    for (float &pop : pop_)
        pop = std::max(0.0f, pop - dt * 3.5f);
}

kit::Grid CrownsScene::grid() const
{
    return kit::fit_grid(puzzle_.side, puzzle_.side, 0.07f);
}

void CrownsScene::draw_board(gfx::DrawList &list) const
{
    const kit::Grid g = grid();
    const int n = puzzle_.side;
    const std::vector<bool> bad = conflicts(puzzle_, marks_);
    const float solved_t = solved_progress();
    const auto region_at = [&](int col, int row)
    { return static_cast<int>(puzzle_.region[static_cast<std::size_t>(row * n + col)]); };

    // Regions as whole shapes: cells of one colour are joined across the gap,
    // so the gaps that remain are exactly the region borders.
    const float radius = g.cell * 0.16f;
    for (int row = 0; row < n; ++row)
        for (int col = 0; col < n; ++col)
        {
            const gfx::Rect r = g.cell_rect(col, row);
            list.rounded_rect({r.x, r.y + 3, r.w, r.h}, radius, kit::look::kTileShadow);
        }
    for (int row = 0; row < n; ++row)
    {
        for (int col = 0; col < n; ++col)
        {
            const gfx::Rect r = g.cell_rect(col, row);
            const int region = region_at(col, row);
            const gfx::Color fill = region_color(region);
            list.rounded_rect(r, radius, fill);
            const bool right = col + 1 < n && region_at(col + 1, row) == region;
            const bool down = row + 1 < n && region_at(col, row + 1) == region;
            if (right)
                list.rounded_rect({r.x + r.w - radius, r.y, g.gap + 2 * radius, r.h}, 0, fill);
            if (down)
                list.rounded_rect({r.x, r.y + r.h - radius, r.w, g.gap + 2 * radius}, 0, fill);
            if (right && down && region_at(col + 1, row + 1) == region)
                list.rounded_rect({r.x + r.w - radius, r.y + r.h - radius, g.gap + 2 * radius,
                                   g.gap + 2 * radius},
                                  0, fill);
        }
    }
    // A faint cell grid inside each region, so cells can still be counted.
    const float hair = std::max(1.5f, g.cell * 0.018f);
    for (int row = 0; row < n; ++row)
    {
        for (int col = 0; col < n; ++col)
        {
            const gfx::Rect r = g.cell_rect(col, row);
            const int region = region_at(col, row);
            const gfx::Color line = kit::look::hue(region).with_alpha(0.35f);
            if (col + 1 < n && region_at(col + 1, row) == region)
                list.rounded_rect(
                    {r.x + r.w + g.gap * 0.5f - hair * 0.5f, r.y + r.h * 0.2f, hair, r.h * 0.6f},
                    hair * 0.5f, line);
            if (row + 1 < n && region_at(col, row + 1) == region)
                list.rounded_rect(
                    {r.x + r.w * 0.2f, r.y + r.h + g.gap * 0.5f - hair * 0.5f, r.w * 0.6f, hair},
                    hair * 0.5f, line);
        }
    }

    for (int row = 0; row < n; ++row)
    {
        for (int col = 0; col < n; ++col)
        {
            const int i = row * n + col;
            gfx::Rect r = g.cell_rect(col, row);
            // Solved: a wave of pops runs across the crowns from the top left.
            if (solved_t > 0.0f && solved_t < 1.0f)
            {
                const float wave = std::clamp(solved_t * 2.2f - (row + col) * 0.08f, 0.0f, 1.0f);
                const float s = 1.0f + 0.3f * std::sin(wave * 3.14159265f);
                r = {r.x - r.w * (s - 1) * 0.5f, r.y - r.h * (s - 1) * 0.5f, r.w * s, r.h * s};
            }
            const std::uint8_t mark = marks_[static_cast<std::size_t>(i)];
            const float cx = r.x + r.w * 0.5f;
            const float cy = r.y + r.h * 0.5f;
            if (mark == kCrown)
            {
                const float pop = pop_[static_cast<std::size_t>(i)];
                const float scale = 1.0f + 0.25f * std::sin(pop * 3.14159265f);
                const bool wrong = bad[static_cast<std::size_t>(i)];
                if (wrong)
                    list.bordered_rect({r.x + 3, r.y + 3, r.w - 6, r.h - 6}, r.w * 0.16f,
                                       gfx::Color{0, 0, 0, 0}, std::max(3.0f, r.w * 0.05f),
                                       kit::look::kError);
                draw_crown(list, cx, cy + r.h * 0.03f, r.w * scale,
                           wrong ? kit::look::kError : kit::look::kInk);
            }
            else if (mark == kCross)
            {
                const float s = r.w * 0.13f;
                const float thick = std::max(2.0f, r.w * 0.055f);
                list.line(cx - s, cy - s, cx + s, cy + s, thick, kit::look::kInk.with_alpha(0.45f));
                list.line(cx - s, cy + s, cx + s, cy - s, thick, kit::look::kInk.with_alpha(0.45f));
            }
        }
    }
}

std::vector<ui::Hint> CrownsScene::hints() const
{
    return {{ui::Button::cross, "Crown"}, {ui::Button::square, "Mark"}};
}

bool CrownsScene::extra_stat(std::string *label, std::string *value) const
{
    *label = "CROWNS";
    *value = std::to_string(crowns_placed(puzzle_, marks_)) + " / " + std::to_string(puzzle_.side);
    return true;
}

} // namespace ppz::crowns
