// ProsperoPuzzles - Kakuro play screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/kakuro/kakuro_scene.hpp"

#include "core/bytes.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace ppz::kakuro
{

namespace
{

constexpr int kSides[3] = {6, 8, 10};
constexpr std::uint8_t kFormat = 1;
constexpr int kHighlightHue = 0; // the blue of the palette
constexpr float kPi = 3.14159265f;

template <typename T> auto &at(T &items, int i)
{
    return items[static_cast<std::size_t>(i)];
}

gfx::Color mix(gfx::Color a, gfx::Color b, float t)
{
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t,
            a.a + (b.a - a.a) * t};
}

gfx::Rect scaled(const gfx::Rect &r, float s)
{
    return {r.x - r.w * (s - 1) * 0.5f, r.y - r.h * (s - 1) * 0.5f, r.w * s, r.h * s};
}

// How a run stands: complete and right, broken, or still open.
enum class RunState
{
    open,
    done,
    wrong,
};

RunState run_state(const Run &run, const Digits &digits)
{
    int sum = 0;
    unsigned seen = 0;
    bool full = true;
    bool repeat = false;
    for (int k = 0; k < run.length; ++k)
    {
        const int digit = at(digits, at(run.cells, k));
        if (digit == 0)
        {
            full = false;
            continue;
        }
        repeat = repeat || (seen & (1u << digit)) != 0;
        seen |= 1u << digit;
        sum += digit;
    }
    if (repeat || (full && sum != run.sum))
        return RunState::wrong;
    return full ? RunState::done : RunState::open;
}

} // namespace

KakuroScene::KakuroScene(const ui::Fonts &fonts)
    : kit::PuzzleScene(fonts, {"kakuro",
                               "Kakuro",
                               "Every run adds up to its clue.",
                               {"6 \xC3\x97 6", "8 \xC3\x97 8", "10 \xC3\x97 10"},
                               0})
{
}

void KakuroScene::generate(std::uint64_t seed, int size)
{
    puzzle_ = kakuro::generate(seed, at(kSides, std::clamp(size, 0, 2)));
    digits_.fill(0);
    pop_.fill(0.0f);
    picker_open_ = false;
}

void KakuroScene::restart()
{
    digits_.fill(0);
    picker_open_ = false;
}

std::string KakuroScene::serialize() const
{
    bytes::Writer w;
    w.put(kFormat);
    w.put(static_cast<std::uint8_t>(puzzle_.side));
    const int cells = puzzle_.side * puzzle_.side;
    for (int i = 0; i < cells; ++i)
        w.put(at(puzzle_.solution, i));
    for (int i = 0; i < cells; ++i)
        w.put(at(digits_, i));
    return w.data();
}

bool KakuroScene::deserialize(std::string_view data)
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
        at(p.solution, i) = r.get<std::uint8_t>();
    Digits digits{};
    for (int i = 0; i < cells; ++i)
    {
        at(digits, i) = r.get<std::uint8_t>();
        if (at(digits, i) > 9 || (at(digits, i) != 0 && !p.white(i)))
            return false;
    }
    if (!r.finished())
        return false;
    for (int i = 0; i < cells; ++i)
        if (at(p.solution, i) > 9)
            return false;
    compute_clues(&p);
    if (!well_formed(p))
        return false;
    puzzle_ = p;
    digits_ = digits;
    picker_open_ = false; // undo, redo and loading start with the board
    return true;
}

bool KakuroScene::solved() const
{
    return kakuro::solved(puzzle_, digits_);
}

int KakuroScene::nearest_white(int col, int row) const
{
    const int n = puzzle_.side;
    int best = -1;
    int best_distance = 1 << 20;
    for (int i = 0; i < n * n; ++i)
    {
        if (!puzzle_.white(i))
            continue;
        const int distance = std::abs(i % n - col) + std::abs(i / n - row);
        if (distance < best_distance)
        {
            best = i;
            best_distance = distance;
        }
    }
    return best >= 0 ? best
                     : std::clamp(row, 0, std::max(0, n - 1)) * n + std::clamp(col, 0, n - 1);
}

void KakuroScene::move(Direction direction, std::vector<audio::Cue> &cues)
{
    int dc = 0;
    int dr = 0;
    switch (direction)
    {
    case Direction::up:
        dr = -1;
        break;
    case Direction::down:
        dr = 1;
        break;
    case Direction::left:
        dc = -1;
        break;
    case Direction::right:
        dc = 1;
        break;
    default:
        return;
    }
    // The nearest white cell ahead on the same line (blocks are skipped), else
    // the nearest one ahead on a neighbouring line.
    const int n = puzzle_.side;
    int best = -1;
    int best_score = 1 << 20;
    for (int i = 0; i < n * n; ++i)
    {
        if (!puzzle_.white(i))
            continue;
        const int ddc = i % n - cursor_col();
        const int ddr = i / n - cursor_row();
        const int ahead = ddc * dc + ddr * dr;
        if (ahead <= 0)
            continue;
        const int side = std::abs(ddc * dr) + std::abs(ddr * dc);
        const int score = (side == 0 ? 0 : 4096) + (ahead + 2 * side) * 16 + side;
        if (score < best_score)
        {
            best = i;
            best_score = score;
        }
    }
    if (best < 0)
        return;
    set_cursor(best % n, best / n);
    cues.push_back(audio::Cue::cursor);
}

void KakuroScene::place(int cell, int digit, std::vector<audio::Cue> &cues)
{
    if (at(digits_, cell) == digit)
        return;
    remember();
    at(digits_, cell) = static_cast<std::uint8_t>(digit);
    if (digit == 0)
    {
        cues.push_back(audio::Cue::erase);
        return;
    }
    at(pop_, cell) = 1.0f;
    const std::vector<bool> bad = conflicts(puzzle_, digits_);
    cues.push_back(bad[static_cast<std::size_t>(cell)] ? audio::Cue::invalid : audio::Cue::digit);
}

void KakuroScene::play(const InputFrame &input, std::vector<audio::Cue> &cues)
{
    const int cell = cursor_row() * puzzle_.side + cursor_col();
    if (picker_open_)
    {
        if (input.nav != Direction::none)
        {
            int col = picker_focus_ % 3;
            int row = picker_focus_ / 3;
            col += input.nav == Direction::left ? -1 : input.nav == Direction::right ? 1 : 0;
            row += input.nav == Direction::up ? -1 : input.nav == Direction::down ? 1 : 0;
            if (col >= 0 && col < 3 && row >= 0 && row < 3)
            {
                picker_focus_ = row * 3 + col;
                cues.push_back(audio::Cue::cursor);
            }
        }
        if (input.is_pressed(Action::confirm))
        {
            picker_open_ = false;
            place(cell, picker_focus_ + 1, cues);
        }
        else if (input.is_pressed(Action::back))
        {
            picker_open_ = false;
            cues.push_back(audio::Cue::ui_back);
        }
        else if (input.is_pressed(Action::west))
        {
            picker_open_ = false;
            if (at(digits_, cell) != 0)
                place(cell, 0, cues);
            else
                cues.push_back(audio::Cue::ui_back);
        }
        return;
    }

    if (input.nav != Direction::none)
        move(input.nav, cues);
    const int here = cursor_row() * puzzle_.side + cursor_col();
    if (input.is_pressed(Action::confirm))
    {
        if (!puzzle_.white(here))
        {
            reject(cues);
            return;
        }
        picker_open_ = true;
        const int digit = at(digits_, here);
        picker_focus_ = digit > 0 ? digit - 1 : 4;
        focus_x_.snap(static_cast<float>(picker_focus_ % 3));
        focus_y_.snap(static_cast<float>(picker_focus_ / 3));
        picker_intro_.start(0.22f);
        cues.push_back(audio::Cue::ui_select);
    }
    else if (input.is_pressed(Action::west))
    {
        if (!puzzle_.white(here) || at(digits_, here) == 0)
            reject(cues);
        else
            place(here, 0, cues);
    }
}

void KakuroScene::animate(float dt)
{
    for (float &pop : pop_)
        pop = std::max(0.0f, pop - dt * 3.5f);
    picker_intro_.update(dt);
    focus_x_.target = static_cast<float>(picker_focus_ % 3);
    focus_y_.target = static_cast<float>(picker_focus_ / 3);
    focus_x_.update(dt, 30.0f);
    focus_y_.update(dt, 30.0f);
    // The kit starts the cursor mid-board, which may be a block.
    const int n = puzzle_.side;
    if (n > 0 && !puzzle_.white(cursor_row() * n + cursor_col()))
    {
        const int cell = nearest_white(cursor_col(), cursor_row());
        set_cursor(cell % n, cell / n);
    }
}

kit::Grid KakuroScene::grid() const
{
    return kit::fit_grid(puzzle_.side, puzzle_.side, 0.07f);
}

gfx::Rect KakuroScene::cursor_rect(float col, float row) const
{
    const kit::Grid g = grid();
    const int n = puzzle_.side;
    if (n <= 0)
        return g.cell_rect(col, row);
    const int cell =
        nearest_white(static_cast<int>(std::lround(col)), static_cast<int>(std::lround(row)));
    return g.cell_rect(cell % n, cell / n);
}

void KakuroScene::draw_board(gfx::DrawList &list) const
{
    using gfx::Align;
    using gfx::Color;
    const kit::Grid g = grid();
    const int n = puzzle_.side;
    if (n <= 0)
        return;
    const std::vector<bool> bad = conflicts(puzzle_, digits_);
    const std::vector<Run> all = runs(puzzle_);
    const float solved_t = solved_progress();
    const int cursor = cursor_row() * n + cursor_col();
    const Color ink = kit::look::kInk;
    const Color plain_block = mix(ink, kit::look::kBoard, 0.12f);
    const Color on_ink = kit::look::kTile;

    // The runs through the cursor: a soft wash behind their cells.
    std::array<bool, kMaxCells> active{};
    if (solved_t <= 0.0f)
    {
        for (const Run &run : all)
        {
            bool through = false;
            for (int k = 0; k < run.length; ++k)
                through = through || at(run.cells, k) == cursor;
            if (!through)
                continue;
            const int first = at(run.cells, 0);
            const int last = at(run.cells, run.length - 1);
            const gfx::Rect a = g.cell_rect(first % n, first / n);
            const gfx::Rect b = g.cell_rect(last % n, last / n);
            const float pad = g.gap * 0.5f + g.cell * 0.05f;
            list.rounded_rect(
                {a.x - pad, a.y - pad, b.x + b.w - a.x + 2 * pad, b.y + b.h - a.y + 2 * pad},
                g.cell * 0.22f, kit::look::wash(kHighlightHue, 0.42f));
            for (int k = 0; k < run.length; ++k)
                at(active, at(run.cells, k)) = true;
        }
    }

    // Clue text and how each clue's run stands.
    std::array<int, kMaxCells> across_run{};
    std::array<int, kMaxCells> down_run{};
    across_run.fill(-1);
    down_run.fill(-1);
    for (int r = 0; r < static_cast<int>(all.size()); ++r)
        at(at(all, r).across ? across_run : down_run, at(all, r).clue) = r;
    const auto clue_color = [&](int r)
    {
        switch (run_state(at(all, r), digits_))
        {
        case RunState::done:
            return on_ink.with_alpha(0.38f);
        case RunState::wrong:
            return mix(kit::look::kError, on_ink, 0.15f);
        default:
            return on_ink;
        }
    };

    for (int row = 0; row < n; ++row)
    {
        for (int col = 0; col < n; ++col)
        {
            const int i = row * n + col;
            gfx::Rect r = g.cell_rect(col, row);
            if (!puzzle_.white(i))
            {
                const int a = at(across_run, i);
                const int d = at(down_run, i);
                if (a < 0 && d < 0)
                {
                    kit::draw_tile(list, r, plain_block);
                    continue;
                }
                kit::draw_tile(list, r, ink);
                const float inset = r.w * 0.14f;
                list.line(r.x + inset, r.y + inset, r.x + r.w - inset, r.y + r.h - inset,
                          std::max(1.5f, r.w * 0.022f), on_ink.with_alpha(0.22f));
                const float size = r.w * 0.3f;
                if (d >= 0)
                    list.text(*fonts().semibold, fonts().semibold_texture,
                              std::to_string(at(all, d).sum), r.x + r.w * 0.31f,
                              r.y + r.h * 0.69f + size * 0.36f, size, clue_color(d), Align::center);
                if (a >= 0)
                    list.text(*fonts().semibold, fonts().semibold_texture,
                              std::to_string(at(all, a).sum), r.x + r.w * 0.69f,
                              r.y + r.h * 0.31f + size * 0.36f, size, clue_color(a), Align::center);
                continue;
            }
            // Solved: a wave of pops runs across the white cells from the top left.
            if (solved_t > 0.0f && solved_t < 1.0f)
            {
                const float wave =
                    std::clamp(solved_t * 2.2f - static_cast<float>(row + col) * 0.07f, 0.0f, 1.0f);
                r = scaled(r, 1.0f + 0.22f * std::sin(wave * kPi));
            }
            const float pop = at(pop_, i);
            if (pop > 0.0f)
                r = scaled(r, 1.0f + 0.08f * std::sin(pop * kPi));
            const Color fill = at(active, i) ? mix(kit::look::kTile, kit::look::hue(kHighlightHue),
                                                   i == cursor ? 0.16f : 0.08f)
                                             : kit::look::kTile;
            kit::draw_tile(list, r, fill);
            const int digit = at(digits_, i);
            if (digit == 0)
                continue;
            const float size = r.w * 0.56f * (1.0f + 0.3f * std::sin(pop * kPi));
            const bool wrong = bad[static_cast<std::size_t>(i)];
            list.text(*fonts().semibold, fonts().semibold_texture, std::to_string(digit),
                      r.x + r.w * 0.5f, r.y + r.h * 0.5f + size * 0.36f, size,
                      wrong ? kit::look::kError : ink, Align::center);
        }
    }
}

gfx::Rect KakuroScene::picker_card() const
{
    const kit::Grid g = grid();
    const int n = std::max(1, puzzle_.side);
    const int cell = cursor_row() * n + cursor_col();
    const gfx::Rect c = g.cell_rect(cell % n, cell / n);
    const float button = std::clamp(g.cell * 0.9f, 60.0f, 84.0f);
    const float size = button * 3.0f + button * 0.1f * 2.0f + button * 0.36f;
    float x = c.x + c.w + g.gap * 2.0f;
    if (x + size > kit::kBoardArea.x + kit::kBoardArea.w + 30.0f)
        x = c.x - g.gap * 2.0f - size;
    float y = c.y + c.h * 0.5f - size * 0.5f;
    y = std::clamp(y, g.card.y, std::max(g.card.y, g.card.y + g.card.h - size));
    return {x, y, size, size};
}

void KakuroScene::draw_overlay(gfx::DrawList &list) const
{
    using gfx::Align;
    using gfx::Color;
    if (!picker_open_)
        return;
    const int n = puzzle_.side;
    const int cell = cursor_row() * n + cursor_col();
    const gfx::Rect card = picker_card();
    const float button = card.w / (3.0f + 0.2f + 0.36f);
    const float gap = button * 0.1f;
    const float pad = button * 0.18f;
    const float t = picker_intro_.running ? tween::back_out(picker_intro_.progress()) : 1.0f;
    list.push_transform(0.86f + 0.14f * t, card.x + card.w * 0.5f, card.y + card.h * 0.5f, 0, 0);
    list.push_opacity(std::clamp(0.3f + t, 0.0f, 1.0f));
    list.shadow({card.x + 6, card.y + 16, card.w - 12, card.h}, button * 0.34f, 30,
                Color::rgb(0x1c2233, 0.35f));
    list.rounded_rect(card, button * 0.34f, kit::look::kTile);

    // Digits already used elsewhere in this cell's runs are shown faded.
    unsigned used = 0;
    for (const Run &run : runs(puzzle_))
    {
        bool through = false;
        for (int k = 0; k < run.length; ++k)
            through = through || at(run.cells, k) == cell;
        for (int k = 0; k < run.length && through; ++k)
            if (at(run.cells, k) != cell)
                used |= 1u << at(digits_, at(run.cells, k));
    }
    const auto button_rect = [&](float col, float row)
    {
        return gfx::Rect{card.x + pad + col * (button + gap), card.y + pad + row * (button + gap),
                         button, button};
    };
    for (int k = 0; k < 9; ++k)
        list.rounded_rect(button_rect(static_cast<float>(k % 3), static_cast<float>(k / 3)),
                          button * 0.24f, kit::look::kBoard);
    const gfx::Rect focus = button_rect(focus_x_.value, focus_y_.value);
    list.rounded_rect({focus.x, focus.y + 3, focus.w, focus.h}, button * 0.24f,
                      kit::look::kTileShadow);
    list.rounded_rect(focus, button * 0.24f, kit::look::hue(kHighlightHue));
    const float size = button * 0.5f;
    for (int k = 0; k < 9; ++k)
    {
        const gfx::Rect r = button_rect(static_cast<float>(k % 3), static_cast<float>(k / 3));
        const int digit = k + 1;
        Color color =
            (used & (1u << digit)) != 0 ? kit::look::kInkSoft.with_alpha(0.6f) : kit::look::kInk;
        if (k == picker_focus_)
            color = kit::look::kTile;
        list.text(*fonts().semibold, fonts().semibold_texture, std::to_string(digit),
                  r.x + r.w * 0.5f, r.y + r.h * 0.5f + size * 0.36f, size, color, Align::center);
    }
    list.pop_opacity();
    list.pop_transform();
}

std::vector<ui::Hint> KakuroScene::hints() const
{
    if (picker_open_)
        return {{ui::Button::dpad, "Choose"},
                {ui::Button::cross, "Place"},
                {ui::Button::circle, "Close"}};
    return {{ui::Button::cross, "Enter digit"}, {ui::Button::square, "Clear"}};
}

bool KakuroScene::extra_stat(std::string *label, std::string *value) const
{
    *label = "FILLED";
    *value = std::to_string(filled_cells(puzzle_, digits_)) + " / " +
             std::to_string(white_cells(puzzle_));
    return true;
}

} // namespace ppz::kakuro
