// ProsperoPuzzles - Sokoban play screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/sokoban/sokoban_scene.hpp"

#include "core/bytes.hpp"

#include <algorithm>
#include <cmath>

namespace ppz::sokoban
{

namespace
{

constexpr std::uint8_t kFormat = 1;
constexpr float kPi = 3.14159265f;
constexpr int kCrateHue = 4;  // warm amber
constexpr int kTargetHue = 5; // green-teal
constexpr int kKeeperHue = 0; // blue

std::size_t at(int i)
{
    return static_cast<std::size_t>(i);
}

gfx::Color mix(gfx::Color a, gfx::Color b, float k)
{
    return {a.r + (b.r - a.r) * k, a.g + (b.g - a.g) * k, a.b + (b.b - a.b) * k, a.a};
}

const gfx::Color kWallSide = gfx::Color::rgb(0x363f58);
const gfx::Color kWallFace = gfx::Color::rgb(0x505a75);
const gfx::Color kWallLight = gfx::Color::rgb(0x6c7792);
const gfx::Color kWhite = gfx::Color::rgb(0xfffefa);

int direction_index(Direction d)
{
    switch (d)
    {
    case Direction::up:
        return 0;
    case Direction::down:
        return 1;
    case Direction::left:
        return 2;
    case Direction::right:
        return 3;
    default:
        return -1;
    }
}

} // namespace

SokobanScene::SokobanScene(const ui::Fonts &fonts)
    : kit::PuzzleScene(fonts, {"sokoban",
                               "Sokoban",
                               "Push every crate onto a target.",
                               {"Small", "Medium", "Large"},
                               0})
{
}

void SokobanScene::snap_pieces()
{
    const int cols = std::max(1, puzzle_.cols);
    keeper_x_.snap(static_cast<float>(state_.keeper % cols));
    keeper_y_.snap(static_cast<float>(state_.keeper / cols));
    for (int i = 0; i < kMaxCrates; ++i)
    {
        crate_x_[at(i)].snap(static_cast<float>(state_.crates[at(i)] % cols));
        crate_y_[at(i)].snap(static_cast<float>(state_.crates[at(i)] / cols));
    }
    pop_.fill(0.0f);
    step_ = 0.0f;
    placed_ = true;
}

void SokobanScene::generate(std::uint64_t seed, int size)
{
    puzzle_ = sokoban::generate(seed, size);
    state_ = puzzle_.start;
    facing_ = 1;
    snap_pieces();
}

void SokobanScene::restart()
{
    state_ = puzzle_.start;
    facing_ = 1;
}

std::string SokobanScene::serialize() const
{
    bytes::Writer w;
    w.put(kFormat);
    w.put(static_cast<std::uint8_t>(puzzle_.cols));
    w.put(static_cast<std::uint8_t>(puzzle_.rows));
    for (int i = 0; i < puzzle_.cols * puzzle_.rows; ++i)
        w.put(puzzle_.cell[at(i)]);
    w.put(static_cast<std::uint8_t>(puzzle_.crates));
    w.put(static_cast<std::uint16_t>(puzzle_.min_pushes < 0 ? 0xffff : puzzle_.min_pushes));
    for (const State *s : {&puzzle_.start, &state_})
    {
        w.put(s->keeper);
        for (int i = 0; i < puzzle_.crates; ++i)
            w.put(s->crates[at(i)]);
    }
    return w.data();
}

bool SokobanScene::deserialize(std::string_view data)
{
    bytes::Reader r(data);
    if (r.get<std::uint8_t>() != kFormat)
        return false;
    Puzzle p;
    p.cols = r.get<std::uint8_t>();
    p.rows = r.get<std::uint8_t>();
    if (p.cols < 3 || p.rows < 3 || p.cols > kMaxSide || p.rows > kMaxSide)
        return false;
    const int cells = p.cols * p.rows;
    int targets = 0;
    for (int i = 0; i < cells; ++i)
    {
        p.cell[at(i)] = r.get<std::uint8_t>();
        if (p.cell[at(i)] > kTarget)
            return false;
        targets += p.cell[at(i)] == kTarget ? 1 : 0;
    }
    p.crates = r.get<std::uint8_t>();
    if (p.crates < 1 || p.crates > kMaxCrates || p.crates != targets)
        return false;
    const std::uint16_t pushes = r.get<std::uint16_t>();
    p.min_pushes = pushes == 0xffff ? -1 : pushes;
    State current;
    for (State *s : {&p.start, &current})
    {
        s->keeper = r.get<std::uint8_t>();
        if (!is_floor(p, s->keeper))
            return false;
        for (int i = 0; i < p.crates; ++i)
        {
            const std::uint8_t cell = r.get<std::uint8_t>();
            if (!is_floor(p, cell) || cell == s->keeper || crate_at(p, *s, cell) >= 0)
                return false;
            s->crates[at(i)] = cell;
        }
    }
    if (!r.finished())
        return false;
    const bool same_room =
        placed_ && p.cols == puzzle_.cols && p.rows == puzzle_.rows && p.cell == puzzle_.cell;
    puzzle_ = p;
    state_ = current;
    if (!same_room)
        snap_pieces(); // a loaded game appears in place; undo slides back
    return true;
}

bool SokobanScene::solved() const
{
    return sokoban::solved(puzzle_, state_);
}

void SokobanScene::play(const InputFrame &input, std::vector<audio::Cue> &cues)
{
    const int dir = direction_index(input.nav);
    if (dir < 0)
        return;
    facing_ = dir;
    State next = state_;
    int pushed = -1;
    const Step step = move(puzzle_, &next, dir, &pushed);
    if (step == Step::none)
        return; // walking into a wall: the keeper just turns
    if (step == Step::blocked)
    {
        reject(cues);
        return;
    }
    remember();
    state_ = next;
    step_ = 1.0f;
    set_cursor(state_.keeper % puzzle_.cols, state_.keeper / puzzle_.cols);
    if (step == Step::walk)
    {
        cues.push_back(audio::Cue::cursor);
        return;
    }
    if (puzzle_.cell[at(state_.crates[at(pushed)])] == kTarget)
    {
        pop_[at(pushed)] = 1.0f;
        cues.push_back(audio::Cue::place);
    }
    else
    {
        cues.push_back(audio::Cue::slide);
    }
}

void SokobanScene::animate(float dt)
{
    const int cols = std::max(1, puzzle_.cols);
    keeper_x_.target = static_cast<float>(state_.keeper % cols);
    keeper_y_.target = static_cast<float>(state_.keeper / cols);
    keeper_x_.update(dt, 26.0f);
    keeper_y_.update(dt, 26.0f);
    for (int i = 0; i < puzzle_.crates; ++i)
    {
        crate_x_[at(i)].target = static_cast<float>(state_.crates[at(i)] % cols);
        crate_y_[at(i)].target = static_cast<float>(state_.crates[at(i)] / cols);
        crate_x_[at(i)].update(dt, 26.0f);
        crate_y_[at(i)].update(dt, 26.0f);
    }
    for (float &pop : pop_)
        pop = std::max(0.0f, pop - dt * 3.0f);
    step_ = std::max(0.0f, step_ - dt * 6.0f);
}

kit::Grid SokobanScene::grid() const
{
    return kit::fit_grid(puzzle_.cols, puzzle_.rows, 0.06f);
}

void SokobanScene::draw_walls(gfx::DrawList &list, const kit::Grid &g) const
{
    const auto wall = [&](int col, int row)
    {
        return col >= 0 && row >= 0 && col < puzzle_.cols && row < puzzle_.rows &&
               puzzle_.cell[at(row * puzzle_.cols + col)] == kWall;
    };
    const float radius = g.cell * 0.2f;
    const float depth = g.cell * 0.07f;
    // One continuous shape: every wall block is joined to its wall neighbours
    // across the gap, drawn as a darker side layer and a lighter top face.
    const auto shape = [&](float dy, gfx::Color fill)
    {
        for (int row = 0; row < puzzle_.rows; ++row)
        {
            for (int col = 0; col < puzzle_.cols; ++col)
            {
                if (!wall(col, row))
                    continue;
                const gfx::Rect r = g.cell_rect(col, row);
                list.rounded_rect({r.x, r.y + dy, r.w, r.h}, radius, fill);
                const bool right = wall(col + 1, row);
                const bool down = wall(col, row + 1);
                if (right)
                    list.rounded_rect({r.x + r.w - radius, r.y + dy, g.gap + 2 * radius, r.h}, 0,
                                      fill);
                if (down)
                    list.rounded_rect({r.x, r.y + r.h - radius + dy, r.w, g.gap + 2 * radius}, 0,
                                      fill);
                if (right && down && wall(col + 1, row + 1))
                    list.rounded_rect({r.x + r.w - radius, r.y + r.h - radius + dy,
                                       g.gap + 2 * radius, g.gap + 2 * radius},
                                      0, fill);
            }
        }
    };
    shape(depth, kWallSide);
    shape(0.0f, kWallFace);
    // A soft highlight along the top edge of each exposed run of wall.
    const float inset = g.cell * 0.16f;
    const float band = std::max(3.0f, g.cell * 0.06f);
    const auto exposed = [&](int col, int row) { return wall(col, row) && !wall(col, row - 1); };
    for (int row = 0; row < puzzle_.rows; ++row)
    {
        for (int col = 0; col < puzzle_.cols; ++col)
        {
            if (!exposed(col, row) || exposed(col - 1, row))
                continue;
            int last = col;
            while (exposed(last + 1, row))
                ++last;
            const gfx::Rect a = g.cell_rect(col, row);
            const gfx::Rect b = g.cell_rect(last, row);
            list.rounded_rect({a.x + inset, a.y + g.cell * 0.1f, b.x + b.w - a.x - 2 * inset, band},
                              band * 0.5f, kWallLight);
        }
    }
}

void SokobanScene::draw_crate(gfx::DrawList &list, const kit::Grid &g, int index) const
{
    const float x = crate_x_[at(index)].value;
    const float y = crate_y_[at(index)].value;
    const gfx::Rect cell = g.cell_rect(x, y);
    const int home = state_.crates[at(index)];
    const bool arrived = std::fabs(x - static_cast<float>(home % puzzle_.cols)) +
                             std::fabs(y - static_cast<float>(home / puzzle_.cols)) <
                         0.35f;
    const bool on = arrived && puzzle_.cell[at(home)] == kTarget;

    float scale = 1.0f + 0.22f * std::sin(pop_[at(index)] * kPi);
    const float solved_t = solved_progress();
    if (solved_t > 0.0f && solved_t < 1.0f)
    {
        // Solved: the crates pop one after another.
        const float wave =
            std::clamp(solved_t * 2.2f - static_cast<float>(index) * 0.3f, 0.0f, 1.0f);
        scale *= 1.0f + 0.3f * std::sin(wave * kPi);
    }
    const float s = g.cell * 0.82f * scale;
    const float cx = cell.x + cell.w * 0.5f;
    const float cy = cell.y + cell.h * 0.5f;
    const gfx::Rect box{cx - s * 0.5f, cy - s * 0.5f, s, s};
    const gfx::Color fill = kit::look::hue(on ? kTargetHue : kCrateHue);
    kit::draw_tile(list, box, fill, 0.16f);
    const float inset = s * 0.13f;
    const gfx::Rect inner{box.x + inset, box.y + inset, s - 2 * inset, s - 2 * inset};
    list.rounded_rect(inner, s * 0.08f, mix(fill, kWhite, 0.3f));
    if (on)
    {
        const float t = std::max(2.5f, s * 0.085f);
        list.line(cx - s * 0.19f, cy + s * 0.01f, cx - s * 0.05f, cy + s * 0.15f, t, kWhite);
        list.line(cx - s * 0.05f, cy + s * 0.15f, cx + s * 0.21f, cy - s * 0.13f, t, kWhite);
    }
    else
    {
        // Plank lines across the inset, and a brace.
        const gfx::Color plank = mix(fill, kit::look::kInk, 0.22f).with_alpha(0.7f);
        const float t = std::max(2.0f, s * 0.035f);
        for (int k = 1; k <= 2; ++k)
        {
            const float py = inner.y + inner.h * static_cast<float>(k) / 3.0f;
            list.line(inner.x + t, py, inner.x + inner.w - t, py, t, plank);
        }
        list.line(inner.x + inner.w * 0.18f, inner.y + inner.h * 0.82f, inner.x + inner.w * 0.82f,
                  inner.y + inner.h * 0.18f, t * 1.4f, plank.with_alpha(0.8f));
    }
}

void SokobanScene::draw_keeper(gfx::DrawList &list, const kit::Grid &g) const
{
    const gfx::Rect cell = g.cell_rect(keeper_x_.value, keeper_y_.value);
    const float cx = cell.x + cell.w * 0.5f;
    const float ground = cell.y + cell.h * 0.5f;
    const float radius = g.cell * 0.36f;

    // Solved: two happy hops.
    const float u = std::clamp(solved_progress() * 1.25f, 0.0f, 1.0f);
    const float hop = g.cell * 0.26f * std::fabs(std::sin(u * 2.0f * kPi)) * (1.0f - 0.4f * u);
    const float bob = g.cell * 0.04f * std::sin(step_ * kPi);
    const float cy = ground - hop - bob;

    const float shadow_w = radius * (1.8f - 0.6f * hop / g.cell);
    list.rounded_rect({cx - shadow_w * 0.5f, ground + radius * 0.7f, shadow_w, radius * 0.38f},
                      radius * 0.19f, kit::look::kTileShadow);
    const gfx::Color body = kit::look::hue(kKeeperHue);
    list.circle(cx, cy + radius * 0.05f, radius, mix(body, kit::look::kInk, 0.3f));
    list.circle(cx, cy - radius * 0.02f, radius * 0.95f, body);
    list.circle(cx - radius * 0.38f, cy - radius * 0.42f, radius * 0.17f, kWhite.with_alpha(0.35f));

    // Eyes look where the keeper last moved.
    const float fx = static_cast<float>(kDirCol[facing_]);
    const float fy = static_cast<float>(kDirRow[facing_]);
    const float spread = radius * (fx != 0.0f ? 0.3f : 0.36f);
    for (int side = -1; side <= 1; side += 2)
    {
        const float ex = cx + static_cast<float>(side) * spread + fx * radius * 0.3f;
        const float ey = cy - radius * 0.1f + fy * radius * 0.22f;
        list.circle(ex, ey, radius * 0.27f, kWhite);
        list.circle(ex + fx * radius * 0.09f, ey + fy * radius * 0.09f, radius * 0.15f,
                    kit::look::kInk);
    }
}

void SokobanScene::draw_board(gfx::DrawList &list) const
{
    const kit::Grid g = grid();
    const float well_radius = g.cell * 0.16f;
    for (int row = 0; row < puzzle_.rows; ++row)
    {
        for (int col = 0; col < puzzle_.cols; ++col)
        {
            const std::uint8_t type = puzzle_.cell[at(row * puzzle_.cols + col)];
            if (type != kFloor && type != kTarget)
                continue;
            const gfx::Rect r = g.cell_rect(col, row);
            list.rounded_rect(r, well_radius, kit::look::kWell);
            if (type == kTarget)
            {
                const float cx = r.x + r.w * 0.5f;
                const float cy = r.y + r.h * 0.5f;
                const gfx::Color hue = kit::look::hue(kTargetHue);
                list.circle(cx, cy, r.w * 0.27f, kit::look::wash(kTargetHue, 0.28f));
                list.ring(cx, cy, r.w * 0.27f, std::max(3.0f, r.w * 0.055f), hue.with_alpha(0.85f));
                list.circle(cx, cy, r.w * 0.07f, hue.with_alpha(0.7f));
            }
        }
    }
    draw_walls(list, g);

    // Crates behind the keeper when they share a row, top rows first.
    std::array<int, kMaxCrates> order{};
    for (int i = 0; i < puzzle_.crates; ++i)
        order[at(i)] = i;
    std::sort(order.begin(), order.begin() + puzzle_.crates,
              [&](int a, int b) { return crate_y_[at(a)].value < crate_y_[at(b)].value; });
    for (int i = 0; i < puzzle_.crates; ++i)
        draw_crate(list, g, order[at(i)]);
    draw_keeper(list, g);
}

std::vector<ui::Hint> SokobanScene::hints() const
{
    return {{ui::Button::dpad, "Walk / push"}};
}

bool SokobanScene::extra_stat(std::string *label, std::string *value) const
{
    *label = "CRATES";
    *value =
        std::to_string(crates_on_targets(puzzle_, state_)) + " / " + std::to_string(puzzle_.crates);
    return true;
}

} // namespace ppz::sokoban
