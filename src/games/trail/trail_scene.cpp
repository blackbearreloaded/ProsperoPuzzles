// ProsperoPuzzles - Trail play screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/trail/trail_scene.hpp"

#include "core/bytes.hpp"

#include <algorithm>
#include <cmath>

namespace ppz::trail
{

namespace
{

constexpr int kSides[3] = {5, 6, 7};
constexpr std::uint8_t kFormat = 1;
constexpr float kPi = 3.14159265f;

gfx::Color mix(gfx::Color a, gfx::Color b, float t)
{
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t,
            a.a + (b.a - a.a) * t};
}

// The trail's colour at t (0 at number 1, 1 at the end): teal, blue, violet.
gfx::Color trail_color(float t)
{
    const gfx::Color stops[] = {kit::look::hue(5), kit::look::hue(0), kit::look::hue(7),
                                kit::look::hue(3)};
    constexpr int last = 3;
    const float x = std::clamp(t, 0.0f, 1.0f) * static_cast<float>(last);
    const int i = std::min(last - 1, static_cast<int>(x));
    return mix(stops[i], stops[i + 1], x - static_cast<float>(i));
}

gfx::Color white(float alpha = 1.0f)
{
    return gfx::Color::rgb(0xffffff, alpha);
}

gfx::Rect scaled(const gfx::Rect &r, float s)
{
    return {r.x - r.w * (s - 1) * 0.5f, r.y - r.h * (s - 1) * 0.5f, r.w * s, r.h * s};
}

} // namespace

TrailScene::TrailScene(const ui::Fonts &fonts)
    : kit::PuzzleScene(fonts, {"trail",
                               "Trail",
                               "One path through every cell, numbers in order.",
                               {"5 \xC3\x97 5", "6 \xC3\x97 6", "7 \xC3\x97 7"},
                               0})
{
}

void TrailScene::generate(std::uint64_t seed, int size)
{
    puzzle_ = trail::generate(seed, kSides[std::clamp(size, 0, 2)]);
    restart();
    pop_.fill(0.0f);
}

void TrailScene::restart()
{
    path_.assign(1, static_cast<std::uint8_t>(std::max(0, cell_of(puzzle_, 1))));
    cutting_ = false;
}

std::string TrailScene::serialize() const
{
    bytes::Writer w;
    w.put(kFormat);
    w.put(static_cast<std::uint8_t>(puzzle_.side));
    w.put(static_cast<std::uint8_t>(puzzle_.count));
    const int cells = puzzle_.side * puzzle_.side;
    for (int i = 0; i < cells; ++i)
        w.put(puzzle_.number[static_cast<std::size_t>(i)]);
    for (int i = 0; i < cells; ++i)
        w.put(puzzle_.solution[static_cast<std::size_t>(i)]);
    w.put(static_cast<std::uint8_t>(path_.size()));
    for (std::uint8_t cell : path_)
        w.put(cell);
    return w.data();
}

bool TrailScene::deserialize(std::string_view data)
{
    bytes::Reader r(data);
    if (r.get<std::uint8_t>() != kFormat)
        return false;
    Puzzle p;
    p.side = r.get<std::uint8_t>();
    p.count = r.get<std::uint8_t>();
    if (p.side < 4 || p.side > kMaxSide || p.count < 2)
        return false;
    const int cells = p.side * p.side;
    if (p.count > cells)
        return false;
    for (int i = 0; i < cells; ++i)
        p.number[static_cast<std::size_t>(i)] = r.get<std::uint8_t>();
    std::vector<std::uint8_t> solution;
    for (int i = 0; i < cells; ++i)
    {
        p.solution[static_cast<std::size_t>(i)] = r.get<std::uint8_t>();
        solution.push_back(p.solution[static_cast<std::size_t>(i)]);
    }
    const int length = r.get<std::uint8_t>();
    std::vector<std::uint8_t> path;
    for (int i = 0; i < length; ++i)
        path.push_back(r.get<std::uint8_t>());
    if (!r.finished())
        return false;
    // Every number 1..count exactly once, the stored solution valid, the path legal.
    std::vector<int> seen(static_cast<std::size_t>(p.count) + 1, 0);
    for (int i = 0; i < cells; ++i)
    {
        const int n = p.number[static_cast<std::size_t>(i)];
        if (n > p.count)
            return false;
        ++seen[static_cast<std::size_t>(n)];
    }
    for (int n = 1; n <= p.count; ++n)
        if (seen[static_cast<std::size_t>(n)] != 1)
            return false;
    if (!trail::solved(p, solution) || !valid_path(p, path))
        return false;
    puzzle_ = p;
    path_ = std::move(path);
    cutting_ = false;
    return true;
}

bool TrailScene::solved() const
{
    return trail::solved(puzzle_, path_);
}

int TrailScene::path_index(int cell) const
{
    const auto it = std::find(path_.begin(), path_.end(), static_cast<std::uint8_t>(cell));
    return it == path_.end() ? -1 : static_cast<int>(it - path_.begin());
}

void TrailScene::play(const InputFrame &input, std::vector<audio::Cue> &cues)
{
    if (input.is_pressed(Action::west))
    {
        // Square: erase the head, from either mode.
        cutting_ = false;
        if (path_.size() > 1)
        {
            remember();
            path_.pop_back();
            cues.push_back(audio::Cue::erase);
        }
        else
        {
            reject(cues);
        }
    }
    else if (cutting_)
    {
        cut_mode(cues, input);
    }
    else
    {
        draw_mode(cues, input);
    }
    if (!cutting_)
        set_cursor(path_.back() % puzzle_.side, path_.back() / puzzle_.side);
}

void TrailScene::draw_mode(std::vector<audio::Cue> &cues, const InputFrame &input)
{
    if (input.is_pressed(Action::confirm))
    {
        cutting_ = true; // the ring leaves the head to pick a cell to cut back to
        cues.push_back(audio::Cue::ui_toggle);
        return;
    }
    int dcol = 0;
    int drow = 0;
    switch (input.nav)
    {
    case Direction::up:
        drow = -1;
        break;
    case Direction::down:
        drow = 1;
        break;
    case Direction::left:
        dcol = -1;
        break;
    case Direction::right:
        dcol = 1;
        break;
    default:
        return;
    }
    std::vector<std::uint8_t> next = path_;
    switch (step(puzzle_, &next, dcol, drow))
    {
    case Step::extended:
    {
        remember();
        path_ = std::move(next);
        const std::uint8_t head = path_.back();
        pop_[head] = 1.0f;
        cues.push_back(puzzle_.number[head] != 0 ? audio::Cue::connect : audio::Cue::place);
        break;
    }
    case Step::retracted:
        remember();
        path_ = std::move(next);
        cues.push_back(audio::Cue::erase);
        break;
    default:
        // Held directions repeat; only the first push into a wall complains.
        if (!input.nav_repeat)
            reject(cues);
        break;
    }
}

void TrailScene::cut_mode(std::vector<audio::Cue> &cues, const InputFrame &input)
{
    if (input.is_pressed(Action::back))
    {
        cutting_ = false;
        cues.push_back(audio::Cue::ui_back);
        return;
    }
    if (!input.is_pressed(Action::confirm))
        return;
    // Cross again always returns to drawing; on a path cell it cuts there first.
    const int index = path_index(cursor_row() * puzzle_.side + cursor_col());
    cutting_ = false;
    if (index < 0 || index + 1 == static_cast<int>(path_.size()))
    {
        cues.push_back(audio::Cue::ui_back);
        return;
    }
    remember();
    path_.resize(static_cast<std::size_t>(index) + 1);
    cues.push_back(audio::Cue::erase);
}

void TrailScene::animate(float dt)
{
    for (float &pop : pop_)
        pop = std::max(0.0f, pop - dt * 5.0f);
    if (!cutting_ && !path_.empty())
        set_cursor(path_.back() % puzzle_.side, path_.back() / puzzle_.side);
}

kit::Grid TrailScene::grid() const
{
    return kit::fit_grid(puzzle_.side, puzzle_.side, 0.12f);
}

void TrailScene::draw_board(gfx::DrawList &list) const
{
    const kit::Grid g = grid();
    const int n = puzzle_.side;
    const int cells = n * n;
    if (n <= 1 || path_.empty())
        return;
    const float solved_t = solved_progress();
    const float radius = g.cell * 0.18f;
    const int length = static_cast<int>(path_.size());
    const int head = path_.back();
    const bool done = solved();
    const int next = next_number(puzzle_, path_);
    // In cut mode, the part of the path that Cross would erase is faded.
    int keep = length;
    if (cutting_)
    {
        const int index = path_index(cursor_row() * n + cursor_col());
        if (index >= 0)
            keep = index + 1;
    }

    std::vector<int> order(static_cast<std::size_t>(cells), -1);
    for (int i = 0; i < length; ++i)
        order[path_[static_cast<std::size_t>(i)]] = i;
    const auto center = [&](int cell)
    {
        const gfx::Rect r = g.cell_rect(cell % n, cell / n);
        return std::pair<float, float>{r.x + r.w * 0.5f, r.y + r.h * 0.5f};
    };
    // 0..1 along the whole trail, so a cell keeps its colour as the path grows.
    const auto along = [&](int index)
    { return static_cast<float>(index) / static_cast<float>(cells - 1); };
    // Solved: a light wave runs along the path from 1 to the end.
    const auto wave = [&](int index)
    {
        if (solved_t <= 0.0f || solved_t >= 1.0f)
            return 0.0f;
        const float w = std::clamp(solved_t * 2.0f - along(index), 0.0f, 1.0f);
        return std::sin(w * kPi);
    };
    const auto fill_of = [&](int index)
    {
        gfx::Color c = mix(kit::look::kTile, trail_color(along(index)), 0.72f);
        c = mix(c, white(), 0.6f * wave(index));
        if (index >= keep)
            c = mix(kit::look::kWell, c, 0.35f);
        return c;
    };

    // Wells, with a soft shadow.
    for (int cell = 0; cell < cells; ++cell)
    {
        const gfx::Rect r = g.cell_rect(cell % n, cell / n);
        list.rounded_rect({r.x, r.y + 3, r.w, r.h}, radius, kit::look::kTileShadow);
        list.rounded_rect(r, radius, kit::look::kWell);
    }

    // Bridges across the gaps between consecutive path cells.
    for (int i = 1; i < length; ++i)
    {
        const auto [ax, ay] = center(path_[static_cast<std::size_t>(i - 1)]);
        const auto [bx, by] = center(path_[static_cast<std::size_t>(i)]);
        const float grow = 1.0f - pop_[path_[static_cast<std::size_t>(i)]];
        const float ex = ax + (bx - ax) * grow;
        const float ey = ay + (by - ay) * grow;
        list.line(ax, ay, ex, ey, g.cell * 0.56f, mix(fill_of(i - 1), fill_of(i), 0.5f));
    }
    // Path cells: coloured along the trail; a new cell pops in.
    for (int i = 0; i < length; ++i)
    {
        const int cell = path_[static_cast<std::size_t>(i)];
        const float pop = pop_[static_cast<std::size_t>(cell)];
        const float p = 1.0f - pop;
        float s = pop > 0.0f ? 0.75f + 0.25f * p + 0.14f * std::sin(p * kPi) : 1.0f;
        s += 0.06f * wave(i);
        list.rounded_rect(scaled(g.cell_rect(cell % n, cell / n), s), radius, fill_of(i));
    }
    // The trail's line through the cell centres.
    const float thick = g.cell * 0.15f;
    for (int i = 1; i < length; ++i)
    {
        const auto [ax, ay] = center(path_[static_cast<std::size_t>(i - 1)]);
        const auto [bx, by] = center(path_[static_cast<std::size_t>(i)]);
        const float grow = 1.0f - pop_[path_[static_cast<std::size_t>(i)]];
        // Opaque, so the joins between segments do not show as beads.
        const gfx::Color stroke =
            i >= keep ? mix(fill_of(i), white(), 0.45f) : mix(fill_of(i), white(), 0.92f);
        list.line(ax, ay, ax + (bx - ax) * grow, ay + (by - ay) * grow, thick, stroke);
    }
    if (length == 1 || done)
    {
        const auto [hx, hy] = center(head);
        list.circle(hx, hy, thick * 0.5f, mix(fill_of(length - 1), white(), 0.92f));
    }
    else if (puzzle_.number[static_cast<std::size_t>(head)] == 0)
    {
        // The head: a bright dot the trail grows from.
        const auto [hx, hy] = center(head);
        list.circle(hx, hy, g.cell * 0.15f, white());
        list.circle(hx, hy, g.cell * 0.075f, trail_color(along(length - 1)));
    }

    // Numbers: white on ink badges; the next one breathes.
    const float badge = g.cell * 0.3f;
    const auto &font = *fonts().semibold;
    for (int cell = 0; cell < cells; ++cell)
    {
        const int number = puzzle_.number[static_cast<std::size_t>(cell)];
        if (number == 0)
            continue;
        auto [cx, cy] = center(cell);
        float r = badge;
        const int index = order[static_cast<std::size_t>(cell)];
        if (index >= 0)
            r *= 1.0f + 0.1f * wave(index);
        gfx::Color ink = kit::look::kInk;
        // The end reached too early: the path cannot go on from here.
        if (cell == head && number == puzzle_.count && !done)
            ink = kit::look::kError;
        if (number == next && !done)
        {
            const float beat = 0.5f + 0.5f * std::sin(time() * 4.0f);
            const float halo = r * (1.25f + 0.25f * beat);
            list.circle(cx, cy, halo, kit::look::kInk.with_alpha(0.14f * (1.0f - beat) + 0.06f));
            r *= 1.0f + 0.06f * beat;
        }
        if (index >= 0)
            list.circle(cx, cy, r + std::max(2.0f, g.cell * 0.03f), white(0.9f));
        list.circle(cx, cy, r, ink);
        const float size = r * (number >= 10 ? 1.0f : 1.15f);
        list.text(font, fonts().semibold_texture, std::to_string(number), cx, cy + size * 0.36f,
                  size, white(), gfx::Align::center);
    }
}

std::vector<ui::Hint> TrailScene::hints() const
{
    if (cutting_)
        return {{ui::Button::dpad, "Choose"},
                {ui::Button::cross, "Cut here"},
                {ui::Button::circle, "Cancel"}};
    return {{ui::Button::dpad, "Draw"}, {ui::Button::square, "Back"}, {ui::Button::cross, "Cut"}};
}

bool TrailScene::extra_stat(std::string *label, std::string *value) const
{
    *label = "CELLS";
    *value = std::to_string(path_.size()) + " / " + std::to_string(puzzle_.side * puzzle_.side);
    return true;
}

} // namespace ppz::trail
