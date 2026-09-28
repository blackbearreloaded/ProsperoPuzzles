// ProsperoPuzzles - Link Up play screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/linkup/linkup_scene.hpp"

#include "core/bytes.hpp"

#include <algorithm>
#include <cmath>

namespace ppz::linkup
{

namespace
{

constexpr int kSides[3] = {5, 7, 9};
constexpr std::uint8_t kFormat = 1;
constexpr float kPi = 3.14159265f;

std::size_t at(int index)
{
    return static_cast<std::size_t>(index);
}

// Palette slots in the order pairs use them: the most distinct hues first, so
// small boards never show two near-identical colours.
constexpr int kHueOrder[kMaxPairs] = {0, 1, 2, 3, 4, 5, 10, 7, 6, 9, 8, 11};

int hue_index(int pair)
{
    return kHueOrder[at(std::clamp(pair, 0, kMaxPairs - 1))];
}

gfx::Color pair_hue(int pair)
{
    return kit::look::hue(hue_index(pair));
}

} // namespace

LinkUpScene::LinkUpScene(const ui::Fonts &fonts)
    : kit::PuzzleScene(fonts, {"linkup",
                               "Link Up",
                               "Join every pair and fill the whole board.",
                               {"5 \xC3\x97 5", "7 \xC3\x97 7", "9 \xC3\x97 9"},
                               0})
{
}

void LinkUpScene::generate(std::uint64_t seed, int size)
{
    puzzle_ = linkup::generate(seed, kSides[std::clamp(size, 0, 2)]);
    paths_ = {};
    drawing_ = kNone;
    pop_.fill(0.0f);
    glow_.fill(0.0f);
}

void LinkUpScene::restart()
{
    paths_ = {};
    drawing_ = kNone;
}

std::string LinkUpScene::serialize() const
{
    bytes::Writer w;
    w.put(kFormat);
    w.put(static_cast<std::uint8_t>(puzzle_.side));
    w.put(static_cast<std::uint8_t>(puzzle_.pairs));
    for (int p = 0; p < puzzle_.pairs; ++p)
        w.put(puzzle_.length[at(p)]);
    for (int i = 0; i < puzzle_.cells(); ++i)
        w.put(puzzle_.route[at(i)]);
    for (int p = 0; p < puzzle_.pairs; ++p)
    {
        const auto &path = paths_[at(p)];
        w.put(static_cast<std::uint8_t>(path.size()));
        for (std::uint8_t cell : path)
            w.put(cell);
    }
    return w.data();
}

bool LinkUpScene::deserialize(std::string_view data)
{
    bytes::Reader r(data);
    if (r.get<std::uint8_t>() != kFormat)
        return false;
    Puzzle p;
    p.side = r.get<std::uint8_t>();
    p.pairs = r.get<std::uint8_t>();
    if (p.side < kMinSide || p.side > kMaxSide || p.pairs < 2 || p.pairs > kMaxPairs)
        return false;
    for (int i = 0; i < p.pairs; ++i)
        p.length[at(i)] = r.get<std::uint8_t>();
    for (int i = 0; i < p.cells(); ++i)
        p.route[at(i)] = r.get<std::uint8_t>();
    if (!r.ok() || !valid_puzzle(p))
        return false;
    Paths paths{};
    for (int i = 0; i < p.pairs; ++i)
    {
        const int length = r.get<std::uint8_t>();
        if (length > p.cells())
            return false;
        for (int k = 0; k < length && r.ok(); ++k)
            paths[at(i)].push_back(r.get<std::uint8_t>());
    }
    if (!r.finished() || !valid_paths(p, paths))
        return false;
    puzzle_ = p;
    paths_ = std::move(paths);
    drawing_ = kNone;
    return true;
}

bool LinkUpScene::solved() const
{
    return linkup::solved(puzzle_, paths_);
}

void LinkUpScene::touch()
{
    if (touched_)
        return;
    remember();
    touched_ = true;
}

void LinkUpScene::let_go(std::vector<audio::Cue> &cues)
{
    drawing_ = kNone;
    cues.push_back(audio::Cue::drop);
}

void LinkUpScene::draw_step(Direction direction, std::vector<audio::Cue> &cues)
{
    const int n = puzzle_.side;
    const int head = paths_[at(drawing_)].back();
    int col = head % n;
    int row = head / n;
    switch (direction)
    {
    case Direction::up:
        --row;
        break;
    case Direction::down:
        ++row;
        break;
    case Direction::left:
        --col;
        break;
    case Direction::right:
        ++col;
        break;
    default:
        return;
    }
    if (col < 0 || row < 0 || col >= n || row >= n)
        return; // the board's edge: nothing to draw into
    const int cell = row * n + col;
    Paths next = paths_;
    const Step step = extend(puzzle_, next, drawing_, cell);
    if (step == Step::rejected)
    {
        reject(cues);
        return;
    }
    touch();
    paths_ = std::move(next);
    set_cursor(col, row);
    pop_[at(cell)] = 1.0f;
    switch (step)
    {
    case Step::connected:
        glow_[at(drawing_)] = 1.0f;
        cues.push_back(audio::Cue::connect);
        drawing_ = kNone;
        break;
    case Step::retracted:
        cues.push_back(audio::Cue::erase);
        break;
    default:
        cues.push_back(audio::Cue::slide);
        break;
    }
}

void LinkUpScene::play(const InputFrame &input, std::vector<audio::Cue> &cues)
{
    const int cell = cursor_row() * puzzle_.side + cursor_col();
    if (drawing_ != kNone)
    {
        if (input.is_pressed(Action::confirm) || input.is_pressed(Action::back))
        {
            let_go(cues);
        }
        else if (input.is_pressed(Action::west))
        {
            touch();
            paths_[at(drawing_)].clear();
            drawing_ = kNone;
            cues.push_back(audio::Cue::erase);
        }
        else if (input.nav != Direction::none)
        {
            draw_step(input.nav, cues);
        }
        return;
    }

    const int dot = puzzle_.dot_at(cell);
    const int pair = dot != kNone ? dot : owner(paths_, cell);
    if (input.is_pressed(Action::confirm))
    {
        if (pair == kNone)
        {
            reject(cues);
            return;
        }
        touched_ = false;
        Paths next = paths_;
        pick_up(puzzle_, next, cell);
        if (next != paths_)
        {
            touch();
            paths_ = std::move(next);
        }
        drawing_ = pair;
        pop_[at(cell)] = 1.0f;
        cues.push_back(audio::Cue::pickup);
    }
    else if (input.is_pressed(Action::west))
    {
        if (pair == kNone || paths_[at(pair)].empty())
        {
            reject(cues);
            return;
        }
        remember();
        paths_[at(pair)].clear();
        cues.push_back(audio::Cue::erase);
    }
}

void LinkUpScene::animate(float dt)
{
    for (float &pop : pop_)
        pop = std::max(0.0f, pop - dt * 4.0f);
    for (float &glow : glow_)
        glow = std::max(0.0f, glow - dt * 1.4f);
}

kit::Grid LinkUpScene::grid() const
{
    return kit::fit_grid(puzzle_.side, puzzle_.side, 0.08f);
}

void LinkUpScene::draw_board(gfx::DrawList &list) const
{
    const kit::Grid g = grid();
    const int n = puzzle_.side;
    const float solved_t = solved_progress();
    const float radius = g.cell * 0.2f;
    const auto centre_x = [&](int cell)
    { return g.cell_rect(cell % n, cell / n).x + g.cell * 0.5f; };
    const auto centre_y = [&](int cell)
    { return g.cell_rect(cell % n, cell / n).y + g.cell * 0.5f; };

    std::array<int, kMaxCells> owners{};
    owners.fill(kNone);
    std::array<bool, kMaxPairs> linked{};
    for (int p = 0; p < puzzle_.pairs; ++p)
    {
        for (std::uint8_t cell : paths_[at(p)])
            owners[cell] = p;
        linked[at(p)] = connected(puzzle_, paths_, p);
    }
    const auto wash = [&](int pair)
    { return kit::look::wash(hue_index(pair), linked[at(pair)] ? 0.34f : 0.22f); };

    // Cells: empty wells, or a light wash of the path's hue, joined along the path.
    for (int i = 0; i < puzzle_.cells(); ++i)
    {
        const gfx::Rect r = g.cell_rect(i % n, i / n);
        const int pair = owners[at(i)];
        list.rounded_rect(r, radius, pair == kNone ? kit::look::kWell : wash(pair));
    }
    for (int p = 0; p < puzzle_.pairs; ++p)
    {
        const auto &path = paths_[at(p)];
        for (std::size_t k = 1; k < path.size(); ++k)
        {
            const int a = std::min(path[k - 1], path[k]);
            const int b = std::max(path[k - 1], path[k]);
            const gfx::Rect r = g.cell_rect(a % n, a / n);
            if (b == a + 1)
                list.rounded_rect({r.x + r.w - radius, r.y, g.gap + 2 * radius, r.h}, 0, wash(p));
            else
                list.rounded_rect({r.x, r.y + r.h - radius, r.w, g.gap + 2 * radius}, 0, wash(p));
        }
    }

    // Solved: a swell runs along every path from its first dot.
    const auto swell = [&](std::size_t k, std::size_t length)
    {
        if (solved_t <= 0.0f || solved_t >= 1.0f || length == 0)
            return 1.0f;
        const float along = static_cast<float>(k) / static_cast<float>(length);
        const float wave = std::clamp(solved_t * 2.0f - along, 0.0f, 1.0f);
        return 1.0f + 0.4f * std::sin(wave * kPi);
    };

    // Paths: thick rounded strokes with round joins.
    const float thick = g.cell * 0.36f;
    for (int p = 0; p < puzzle_.pairs; ++p)
    {
        const auto &path = paths_[at(p)];
        const gfx::Color hue = pair_hue(p);
        const float glow = glow_[at(p)];
        if (glow > 0.0f)
            for (std::size_t k = 1; k < path.size(); ++k)
                list.line(centre_x(path[k - 1]), centre_y(path[k - 1]), centre_x(path[k]),
                          centre_y(path[k]), thick * (1.0f + 0.9f * glow),
                          hue.with_alpha(0.3f * glow));
        for (std::size_t k = 1; k < path.size(); ++k)
        {
            const float s = std::min(swell(k - 1, path.size()), swell(k, path.size()));
            list.line(centre_x(path[k - 1]), centre_y(path[k - 1]), centre_x(path[k]),
                      centre_y(path[k]), thick * s, hue);
        }
        for (std::size_t k = 0; k < path.size(); ++k)
        {
            const float s = swell(k, path.size());
            list.circle(centre_x(path[k]), centre_y(path[k]), thick * 0.5f * s, hue);
        }
        // The head being drawn: a rounded nub that pops as it moves.
        if (p == drawing_ && path.size() > 1)
        {
            const int head = path.back();
            const float pop = pop_[at(head)];
            const float r = thick * (0.72f + 0.2f * std::sin(pop * kPi));
            list.circle(centre_x(head), centre_y(head) + r * 0.1f, r, kit::look::kTileShadow);
            list.circle(centre_x(head), centre_y(head), r, hue);
            list.circle(centre_x(head), centre_y(head), r * 0.36f,
                        gfx::Color::rgb(0xffffff, 0.75f));
        }
    }

    // Dots: large filled circles with a soft highlight.
    for (int p = 0; p < puzzle_.pairs; ++p)
    {
        const gfx::Color hue = pair_hue(p);
        const float glow = glow_[at(p)];
        for (int which = 0; which < 2; ++which)
        {
            const int cell = puzzle_.dot(p, which);
            const float cx = centre_x(cell);
            const float cy = centre_y(cell);
            float scale =
                1.0f + 0.18f * std::sin(pop_[at(cell)] * kPi) + 0.14f * std::sin(glow * kPi);
            if (solved_t > 0.0f && solved_t < 1.0f)
            {
                const float along = which == 0 ? 0.0f : 1.0f;
                const float wave = std::clamp(solved_t * 2.0f - along, 0.0f, 1.0f);
                scale += 0.25f * std::sin(wave * kPi);
            }
            const float r = g.cell * 0.33f * scale;
            if (glow > 0.0f)
                list.ring(cx, cy, r * (1.0f + 0.7f * (1.0f - glow)),
                          std::max(2.0f, r * 0.16f * glow), hue.with_alpha(0.7f * glow));
            list.circle(cx, cy + r * 0.12f, r, kit::look::kTileShadow);
            list.circle(cx, cy, r, hue);
            list.circle(cx - r * 0.12f, cy - r * 0.14f, r * 0.62f,
                        gfx::Color::rgb(0xffffff, 0.14f));
            list.circle(cx - r * 0.3f, cy - r * 0.32f, r * 0.24f, gfx::Color::rgb(0xffffff, 0.4f));
            if (linked[at(p)])
                list.circle(cx, cy, r * 0.2f, gfx::Color::rgb(0xffffff, 0.85f));
        }
    }
}

std::vector<ui::Hint> LinkUpScene::hints() const
{
    if (drawing_ != kNone)
        return {{ui::Button::dpad, "Draw"},
                {ui::Button::cross, "Let go"},
                {ui::Button::square, "Clear"}};
    return {{ui::Button::cross, "Pick up"}, {ui::Button::square, "Clear"}};
}

bool LinkUpScene::extra_stat(std::string *label, std::string *value) const
{
    *label = "PAIRS";
    *value =
        std::to_string(connected_pairs(puzzle_, paths_)) + " / " + std::to_string(puzzle_.pairs);
    return true;
}

} // namespace ppz::linkup
