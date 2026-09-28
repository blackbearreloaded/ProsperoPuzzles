// ProsperoPuzzles - Nurikabe play screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/nurikabe/nurikabe_scene.hpp"

#include "core/bytes.hpp"

#include <algorithm>
#include <cmath>

namespace ppz::nurikabe
{

namespace
{

constexpr int kSides[3] = {6, 8, 10};
constexpr std::uint8_t kFormat = 1;
constexpr float kPi = 3.14159265f;

// The sea: a deep teal blue with a darker rim below its surface.
const gfx::Color kWater = gfx::Color::rgb(0x2f86b8);
const gfx::Color kWaterRim = gfx::Color::rgb(0x236a93);
const gfx::Color kGlint = gfx::Color::rgb(0xffffff);

std::size_t z(int i)
{
    return static_cast<std::size_t>(i);
}

float smooth(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

gfx::Rect inset(const gfx::Rect &r, float by)
{
    return {r.x + by, r.y + by, r.w - 2 * by, r.h - 2 * by};
}

gfx::Rect scaled(const gfx::Rect &r, float s)
{
    return {r.x - r.w * (s - 1) * 0.5f, r.y - r.h * (s - 1) * 0.5f, r.w * s, r.h * s};
}

} // namespace

NurikabeScene::NurikabeScene(const ui::Fonts &fonts)
    : kit::PuzzleScene(fonts, {"nurikabe",
                               "Nurikabe",
                               "Shade the sea around numbered islands.",
                               {"6 \xC3\x97 6", "8 \xC3\x97 8", "10 \xC3\x97 10"},
                               0})
{
}

void NurikabeScene::settle()
{
    pop_.fill(0.0f);
    for (int i = 0; i < kMaxCells; ++i)
        wet_[z(i)] = marks_[z(i)] == kSea ? 1.0f : 0.0f;
}

void NurikabeScene::generate(std::uint64_t seed, int size)
{
    puzzle_ = nurikabe::generate(seed, kSides[std::clamp(size, 0, 2)]);
    marks_.fill(kUnknown);
    settle();
}

void NurikabeScene::restart()
{
    marks_.fill(kUnknown);
    pop_.fill(0.0f);
}

std::string NurikabeScene::serialize() const
{
    bytes::Writer w;
    w.put(kFormat);
    w.put(static_cast<std::uint8_t>(puzzle_.side));
    const int cells = puzzle_.side * puzzle_.side;
    for (int i = 0; i < cells; ++i)
        w.put(puzzle_.clue[z(i)]);
    for (int i = 0; i < cells; ++i)
        w.put(puzzle_.solution[z(i)]);
    for (int i = 0; i < cells; ++i)
        w.put(marks_[z(i)]);
    return w.data();
}

bool NurikabeScene::deserialize(std::string_view data)
{
    bytes::Reader r(data);
    if (r.get<std::uint8_t>() != kFormat)
        return false;
    Puzzle p;
    p.side = r.get<std::uint8_t>();
    if (p.side < kMinSide || p.side > kMaxSide)
        return false;
    const int cells = p.side * p.side;
    for (int i = 0; i < cells; ++i)
    {
        p.clue[z(i)] = r.get<std::uint8_t>();
        if (p.clue[z(i)] > cells)
            return false;
    }
    for (int i = 0; i < cells; ++i)
    {
        p.solution[z(i)] = r.get<std::uint8_t>();
        if (p.solution[z(i)] > 1)
            return false;
    }
    Cells marks{};
    for (int i = 0; i < cells; ++i)
    {
        marks[z(i)] = r.get<std::uint8_t>();
        if (marks[z(i)] > kDot || (p.clue[z(i)] != 0 && marks[z(i)] != kUnknown))
            return false;
    }
    if (!r.finished() || !valid_solution(p, p.solution))
        return false;
    const bool same = p.side == puzzle_.side && p.clue == puzzle_.clue;
    puzzle_ = p;
    marks_ = marks;
    if (!same)
        settle(); // a different puzzle: no animation carries over (undo keeps them)
    return true;
}

bool NurikabeScene::solved() const
{
    return nurikabe::solved(puzzle_, marks_);
}

void NurikabeScene::set_mark(int cell, std::uint8_t mark, std::vector<audio::Cue> &cues)
{
    remember();
    marks_[z(cell)] = mark;
    pop_[z(cell)] = 1.0f;
    if (mark == kSea)
    {
        // Completing a 2x2 pool sounds wrong straight away.
        const int n = puzzle_.side;
        const std::vector<int> pools = errors(puzzle_, marks_).pools;
        const int r = cell / n;
        const int c = cell % n;
        const bool pool =
            std::any_of(pools.begin(), pools.end(),
                        [&](int top_left)
                        {
                            const int pr = top_left / n;
                            const int pc = top_left % n;
                            return (r == pr || r == pr + 1) && (c == pc || c == pc + 1);
                        });
        cues.push_back(pool ? audio::Cue::invalid : audio::Cue::place);
    }
    else
    {
        cues.push_back(mark == kDot ? audio::Cue::mark : audio::Cue::erase);
    }
}

void NurikabeScene::play(const InputFrame &input, std::vector<audio::Cue> &cues)
{
    const int cell = cursor_row() * puzzle_.side + cursor_col();
    if (cell < 0 || cell >= kMaxCells)
        return;
    const std::uint8_t mark = marks_[z(cell)];
    const bool numbered = puzzle_.clue[z(cell)] != 0;
    if (input.is_pressed(Action::confirm))
    {
        if (numbered)
            reject(cues);
        else
            set_mark(cell, mark == kSea ? kUnknown : kSea, cues);
    }
    else if (input.is_pressed(Action::west))
    {
        if (numbered)
            reject(cues);
        else
            set_mark(cell, mark == kDot ? kUnknown : kDot, cues);
    }
}

void NurikabeScene::animate(float dt)
{
    for (float &pop : pop_)
        pop = std::max(0.0f, pop - dt * 3.5f);
    for (int i = 0; i < kMaxCells; ++i)
    {
        const float target = marks_[z(i)] == kSea ? 1.0f : 0.0f;
        float &wet = wet_[z(i)];
        wet = wet < target ? std::min(target, wet + dt * 6.0f) : std::max(target, wet - dt * 6.0f);
    }
}

kit::Grid NurikabeScene::grid() const
{
    return kit::fit_grid(puzzle_.side, puzzle_.side, 0.08f);
}

void NurikabeScene::draw_board(gfx::DrawList &list) const
{
    const kit::Grid g = grid();
    const int n = puzzle_.side;
    if (n < kMinSide || n > kMaxSide)
        return;
    const Errors err = errors(puzzle_, marks_);
    const float solved_t = solved_progress();
    const bool done = solved_t > 0.0f;
    const float radius = g.cell * 0.18f;
    const float t = time();
    const auto at = [&](int col, int row) { return z(row * n + col); };
    // Solved: a wave runs from the top left, lifting the numbers and lighting the sea.
    const auto wave = [&](int col, int row)
    {
        if (solved_t <= 0.0f || solved_t >= 1.0f)
            return 0.0f;
        const float w =
            std::clamp(solved_t * 2.2f - static_cast<float>(row + col) * 0.07f, 0.0f, 1.0f);
        return std::sin(w * kPi);
    };
    const auto flood_of = [&](int col, int row) { return smooth(wet_[at(col, row)]); };
    // The solved wave lightens the water it passes.
    const auto shine = [](gfx::Color c, float light)
    {
        const float k = 0.4f * light;
        return gfx::Color{c.r + (1.0f - c.r) * k, c.g + (1.0f - c.g) * k, c.b + (1.0f - c.b) * k,
                          c.a};
    };

    // Wells under every plain cell.
    for (int row = 0; row < n; ++row)
        for (int col = 0; col < n; ++col)
            if (puzzle_.clue[at(col, row)] == 0)
                list.rounded_rect(g.cell_rect(col, row), radius, kit::look::kWell);

    // The sea as one body: each cell rises from its centre as it floods, and
    // neighbouring sea cells are bridged across the gap, first a darker rim a
    // little lower, then the surface.
    const float rim = std::max(2.0f, g.cell * 0.05f);
    for (int pass = 0; pass < 2; ++pass)
    {
        const gfx::Color base = pass == 0 ? kWaterRim : kWater;
        const float dy = pass == 0 ? rim : 0.0f;
        for (int row = 0; row < n; ++row)
        {
            for (int col = 0; col < n; ++col)
            {
                const float f = flood_of(col, row);
                if (f <= 0.0f)
                    continue;
                gfx::Rect r = g.cell_rect(col, row);
                r.y += dy;
                const float lit = wave(col, row);
                const float lit_right = col + 1 < n ? std::max(lit, wave(col + 1, row)) : lit;
                const float lit_down = row + 1 < n ? std::max(lit, wave(col, row + 1)) : lit;
                list.rounded_rect(inset(r, (1.0f - f) * g.cell * 0.3f), radius,
                                  shine(base, lit).with_alpha(f));
                const float right = col + 1 < n ? std::min(f, flood_of(col + 1, row)) : 0.0f;
                const float down = row + 1 < n ? std::min(f, flood_of(col, row + 1)) : 0.0f;
                if (right > 0.0f)
                    list.rounded_rect({r.x + r.w - radius, r.y, g.gap + 2 * radius, r.h}, 0,
                                      shine(base, lit_right).with_alpha(right));
                if (down > 0.0f)
                    list.rounded_rect({r.x, r.y + r.h - radius, r.w, g.gap + 2 * radius}, 0,
                                      shine(base, lit_down).with_alpha(down));
                const float corner = std::min(right, down) > 0.0f
                                         ? std::min({right, down, flood_of(col + 1, row + 1)})
                                         : 0.0f;
                if (corner > 0.0f)
                    list.rounded_rect(
                        {r.x + r.w - radius, r.y + r.h - radius, g.gap + 2 * radius,
                         g.gap + 2 * radius},
                        0, shine(base, std::max(lit_right, lit_down)).with_alpha(corner));
            }
        }
    }
    // A slow shimmer of glints on the surface.
    const float glint = std::max(2.0f, g.cell * 0.04f);
    for (int row = 0; row < n; ++row)
    {
        for (int col = 0; col < n; ++col)
        {
            const float f = flood_of(col, row);
            if (f <= 0.0f)
                continue;
            const gfx::Rect r = g.cell_rect(col, row);
            const float phase =
                t * 1.7f + static_cast<float>(col) * 0.9f + static_cast<float>(row) * 1.3f;
            const float a = (0.12f + 0.10f * std::sin(phase)) * f;
            const float drift = 0.05f * std::sin(phase * 0.6f);
            float x = r.x + r.w * (0.2f + drift);
            float y = r.y + r.h * 0.38f;
            list.line(x, y, x + r.w * 0.26f, y, glint, kGlint.with_alpha(a));
            x = r.x + r.w * (0.48f - drift);
            y = r.y + r.h * 0.64f;
            list.line(x, y, x + r.w * 0.22f, y, glint, kGlint.with_alpha(a * 0.8f));
        }
    }

    // Pools of 2x2 sea, outlined in red.
    const float line = std::max(3.0f, g.cell * 0.055f);
    for (int top_left : err.pools)
    {
        const gfx::Rect a = g.cell_rect(top_left % n, top_left / n);
        const float span = g.cell * 2 + g.gap;
        const float out = g.gap * 0.5f;
        list.bordered_rect({a.x - out, a.y - out, span + 2 * out, span + 2 * out}, radius * 1.3f,
                           kit::look::kError.with_alpha(0.08f), line, kit::look::kError);
    }

    // Island tiles: numbers, dots, and (once solved) every unshaded cell.
    for (int row = 0; row < n; ++row)
    {
        for (int col = 0; col < n; ++col)
        {
            const std::size_t i = at(col, row);
            const int clue = puzzle_.clue[i];
            const std::uint8_t mark = marks_[i];
            if (clue == 0 && mark != kDot && !(done && mark == kUnknown))
                continue;
            const float pop = pop_[i];
            const float lift = clue != 0 ? 0.22f * wave(col, row) : 0.0f;
            const gfx::Rect r =
                scaled(g.cell_rect(col, row), 1.0f + 0.12f * std::sin(pop * kPi) + lift);
            kit::draw_tile(list, r, kit::look::kTile, 0.18f);
            const float cx = r.x + r.w * 0.5f;
            const float cy = r.y + r.h * 0.5f;
            const bool bad = err.bad[i];
            if (clue != 0)
            {
                list.circle(cx, cy, r.w * 0.32f,
                            bad ? kit::look::kError.with_alpha(0.16f)
                                : kit::look::kWell.with_alpha(0.6f));
                const float size = r.w * (clue >= 10 ? 0.4f : 0.46f);
                list.text(*fonts().semibold, fonts().semibold_texture, std::to_string(clue), cx,
                          cy + size * 0.36f, size, bad ? kit::look::kError : kit::look::kInk,
                          gfx::Align::center);
            }
            else if (mark == kDot)
            {
                list.circle(cx, cy, r.w * 0.1f, bad ? kit::look::kError : kit::look::kInkSoft);
            }
        }
    }
}

std::vector<ui::Hint> NurikabeScene::hints() const
{
    return {{ui::Button::cross, "Sea"}, {ui::Button::square, "Island"}};
}

bool NurikabeScene::extra_stat(std::string *label, std::string *value) const
{
    *label = "SEA";
    *value =
        std::to_string(sea_marked(puzzle_, marks_)) + " / " + std::to_string(sea_target(puzzle_));
    return true;
}

} // namespace ppz::nurikabe
