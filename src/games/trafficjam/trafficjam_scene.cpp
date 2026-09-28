// ProsperoPuzzles - Traffic Jam play screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/trafficjam/trafficjam_scene.hpp"

#include "core/bytes.hpp"

#include <algorithm>
#include <cmath>

namespace ppz::trafficjam
{

namespace
{

constexpr std::uint8_t kFormat = 1;
constexpr float kPi = 3.14159265f;

// Palette entries for the other vehicles: every Tenfold hue that is not red or pink.
constexpr int kVehicleHues[] = {0, 2, 3, 4, 5, 7, 8, 10, 11};

constexpr int kHueCount = static_cast<int>(sizeof(kVehicleHues) / sizeof(kVehicleHues[0]));

gfx::Color mix(gfx::Color a, gfx::Color b, float t)
{
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t,
            a.a + (b.a - a.a) * t};
}

gfx::Rect scaled(const gfx::Rect &r, float s)
{
    return {r.x - r.w * (s - 1) * 0.5f, r.y - r.h * (s - 1) * 0.5f, r.w * s, r.h * s};
}

// A part of a vehicle's body, from u0 to u1 along it (0 = back, 1 = front)
// and t0 to t1 across it.
struct Frame
{
    gfx::Rect body;
    bool vertical = false;
    bool front_first = false; // the front is at the left or top

    gfx::Rect part(float u0, float u1, float t0, float t1) const
    {
        if (front_first)
        {
            const float a = 1.0f - u1;
            u1 = 1.0f - u0;
            u0 = a;
        }
        if (vertical)
            return {body.x + body.w * t0, body.y + body.h * u0, body.w * (t1 - t0),
                    body.h * (u1 - u0)};
        return {body.x + body.w * u0, body.y + body.h * t0, body.w * (u1 - u0), body.h * (t1 - t0)};
    }
    void point(float u, float t, float *x, float *y) const
    {
        const gfx::Rect r = part(u, u, t, t);
        *x = r.x;
        *y = r.y;
    }
};

// A chevron pointing right, centred on (cx, cy).
void chevron(gfx::DrawList &list, float cx, float cy, float size, float thick, gfx::Color color)
{
    list.line(cx - size * 0.3f, cy - size * 0.5f, cx + size * 0.3f, cy, thick, color);
    list.line(cx + size * 0.3f, cy, cx - size * 0.3f, cy + size * 0.5f, thick, color);
}

} // namespace

TrafficJamScene::TrafficJamScene(const ui::Fonts &fonts)
    : kit::PuzzleScene(fonts, {"trafficjam",
                               "Traffic Jam",
                               "Clear the lot and drive the red car out.",
                               {"Easy", "Medium", "Hard"},
                               0})
{
}

gfx::Color TrafficJamScene::vehicle_color(int v) const
{
    if (v <= 0 || v >= kMaxVehicles)
        return kit::look::kError; // the red car
    return kit::look::hue(kVehicleHues[hue_[static_cast<std::size_t>(v)] % kHueCount]);
}

void TrafficJamScene::pick_colors()
{
    // Greedy: each vehicle takes the hue that clashes least with the vehicles
    // already coloured: touching at the start is worst, then parallel
    // neighbouring lanes (they can end up side by side), then the same shape.
    const std::array<std::int8_t, kCells> cells = occupancy(puzzle_, puzzle_.start);
    std::array<bool, kMaxVehicles * kMaxVehicles> touching{};
    for (int cell = 0; cell < kCells; ++cell)
    {
        const int a = cells[static_cast<std::size_t>(cell)];
        const int col = cell % kSide;
        const int row = cell / kSide;
        const int right = col + 1 < kSide ? cells[static_cast<std::size_t>(cell + 1)] : -1;
        const int below = row + 1 < kSide ? cells[static_cast<std::size_t>(cell + kSide)] : -1;
        for (int b : {right, below})
        {
            if (a < 0 || b < 0 || a == b)
                continue;
            touching[static_cast<std::size_t>(a * kMaxVehicles + b)] = true;
            touching[static_cast<std::size_t>(b * kMaxVehicles + a)] = true;
        }
    }
    std::array<int, kHueCount> used{};
    hue_.fill(0);
    for (int v = 1; v < puzzle_.count; ++v)
    {
        const Vehicle &mine = puzzle_.vehicles[static_cast<std::size_t>(v)];
        std::array<int, kHueCount> clash{};
        for (int h = 0; h < kHueCount; ++h)
            clash[static_cast<std::size_t>(h)] = used[static_cast<std::size_t>(h)] * 4;
        for (int o = 1; o < v; ++o)
        {
            const Vehicle &other = puzzle_.vehicles[static_cast<std::size_t>(o)];
            // Twins of the same shape are easiest to confuse.
            int cost = 2 + (other.length == mine.length ? 5 : 0) +
                       (other.vertical == mine.vertical ? 3 : 0);
            if (touching[static_cast<std::size_t>(v * kMaxVehicles + o)])
                cost += 100;
            else if (other.vertical == mine.vertical && std::abs(other.lane - mine.lane) <= 1)
                cost += 12;
            clash[hue_[static_cast<std::size_t>(o)]] += cost;
        }
        const auto best = std::min_element(clash.begin(), clash.end()) - clash.begin();
        hue_[static_cast<std::size_t>(v)] = static_cast<std::uint8_t>(best);
        ++used[static_cast<std::size_t>(best)];
    }
}

void TrafficJamScene::snap_springs()
{
    for (int v = 0; v < kMaxVehicles; ++v)
    {
        const auto i = static_cast<std::size_t>(v);
        along_[i].snap(static_cast<float>(pos_[i]));
        lift_[i].snap(0.0f);
    }
}

void TrafficJamScene::generate(std::uint64_t seed, int size)
{
    puzzle_ = trafficjam::generate(seed, std::clamp(size, 0, 2));
    pos_ = puzzle_.start;
    grabbed_ = -1;
    slid_ = false;
    pick_colors();
    snap_springs();
}

void TrafficJamScene::restart()
{
    pos_ = puzzle_.start;
    grabbed_ = -1;
    slid_ = false;
}

std::string TrafficJamScene::serialize() const
{
    bytes::Writer w;
    w.put(kFormat);
    w.put(static_cast<std::uint8_t>(puzzle_.count));
    w.put(static_cast<std::uint8_t>(std::clamp(puzzle_.par, 0, 255)));
    for (int v = 0; v < puzzle_.count; ++v)
    {
        const auto i = static_cast<std::size_t>(v);
        const Vehicle &vehicle = puzzle_.vehicles[i];
        w.put(vehicle.length);
        w.put_bool(vehicle.vertical);
        w.put(vehicle.lane);
        w.put(puzzle_.start[i]);
        w.put(pos_[i]);
    }
    return w.data();
}

bool TrafficJamScene::deserialize(std::string_view data)
{
    bytes::Reader r(data);
    if (r.get<std::uint8_t>() != kFormat)
        return false;
    Puzzle p;
    p.count = r.get<std::uint8_t>();
    p.par = r.get<std::uint8_t>();
    if (p.count < 1 || p.count > kMaxVehicles)
        return false;
    Positions pos{};
    for (int v = 0; v < p.count; ++v)
    {
        const auto i = static_cast<std::size_t>(v);
        p.vehicles[i].length = r.get<std::uint8_t>();
        p.vehicles[i].vertical = r.get_bool();
        p.vehicles[i].lane = r.get<std::uint8_t>();
        p.start[i] = r.get<std::uint8_t>();
        pos[i] = r.get<std::uint8_t>();
    }
    if (!r.finished() || !valid(p, p.start) || !valid(p, pos))
        return false;
    // A different puzzle (a restored save) appears in place; undo steps slide.
    bool same = p.count == puzzle_.count;
    for (int v = 0; same && v < p.count; ++v)
    {
        const auto i = static_cast<std::size_t>(v);
        same = p.vehicles[i].length == puzzle_.vehicles[i].length &&
               p.vehicles[i].vertical == puzzle_.vehicles[i].vertical &&
               p.vehicles[i].lane == puzzle_.vehicles[i].lane;
    }
    puzzle_ = p;
    pos_ = pos;
    grabbed_ = -1;
    slid_ = false;
    if (!same)
    {
        pick_colors();
        snap_springs();
    }
    return true;
}

bool TrafficJamScene::solved() const
{
    return trafficjam::solved(puzzle_, pos_);
}

void TrafficJamScene::slide(int dir, std::vector<audio::Cue> &cues)
{
    const auto v = static_cast<std::size_t>(grabbed_);
    if (slide_room(puzzle_, pos_, grabbed_, dir) == 0)
    {
        reject(cues);
        return;
    }
    // One grab is one move and one undo step, however far the vehicle slides.
    if (!slid_)
    {
        remember();
        slid_ = true;
    }
    pos_[v] = static_cast<std::uint8_t>(pos_[v] + dir);
    if (puzzle_.vehicles[v].vertical)
        set_cursor(cursor_col(), cursor_row() + dir);
    else
        set_cursor(cursor_col() + dir, cursor_row());
    cues.push_back(audio::Cue::slide);
}

void TrafficJamScene::play(const InputFrame &input, std::vector<audio::Cue> &cues)
{
    const std::array<std::int8_t, kCells> cells = occupancy(puzzle_, pos_);
    const auto at = [&cells](int col, int row)
    { return static_cast<int>(cells[static_cast<std::size_t>(row * kSide + col)]); };

    if (grabbed_ < 0)
    {
        if (input.is_pressed(Action::confirm))
        {
            const int v = at(cursor_col(), cursor_row());
            if (v < 0)
            {
                reject(cues);
            }
            else
            {
                grabbed_ = v;
                slid_ = false;
                cues.push_back(audio::Cue::pickup);
            }
        }
    }
    else if (input.is_pressed(Action::confirm) || input.is_pressed(Action::back))
    {
        grabbed_ = -1;
        cues.push_back(audio::Cue::drop);
    }
    else if (input.nav != Direction::none)
    {
        const bool vertical = puzzle_.vehicles[static_cast<std::size_t>(grabbed_)].vertical;
        const bool along = vertical
                               ? (input.nav == Direction::up || input.nav == Direction::down)
                               : (input.nav == Direction::left || input.nav == Direction::right);
        if (!along)
            cues.push_back(audio::Cue::invalid); // across the lane: a soft no
        else
            slide(input.nav == Direction::up || input.nav == Direction::left ? -1 : 1, cues);
    }
}

void TrafficJamScene::animate(float dt)
{
    if (is_solved())
        grabbed_ = -1;
    // The vehicle under the cursor rises a little; the held one rises fully.
    const int hover =
        is_solved()
            ? -1
            : occupancy(puzzle_,
                        pos_)[static_cast<std::size_t>(cursor_row() * kSide + cursor_col())];
    for (int v = 0; v < puzzle_.count; ++v)
    {
        const auto i = static_cast<std::size_t>(v);
        along_[i].target = static_cast<float>(pos_[i]);
        along_[i].update(dt, 20.0f);
        lift_[i].target = v == grabbed_ ? 1.0f : v == hover ? 0.3f : 0.0f;
        lift_[i].update(dt, 22.0f);
    }
}

kit::Grid TrafficJamScene::grid() const
{
    return kit::fit_grid(kSide, kSide, 0.08f);
}

gfx::Rect TrafficJamScene::cursor_rect(float col, float row) const
{
    const kit::Grid g = grid();
    const int c = std::clamp(static_cast<int>(std::lround(col)), 0, kSide - 1);
    const int r = std::clamp(static_cast<int>(std::lround(row)), 0, kSide - 1);
    const int v = occupancy(puzzle_, pos_)[static_cast<std::size_t>(r * kSide + c)];
    if (v < 0 || v != grabbed_)
        return g.cell_rect(c, r);
    // A held vehicle's ring hugs all of it; inset so the kit's padding (6% of
    // the width) leaves the same margin as around a single cell.
    const auto i = static_cast<std::size_t>(v);
    const Vehicle &vehicle = puzzle_.vehicles[i];
    const int end = pos_[i] + vehicle.length - 1;
    const gfx::Rect a =
        vehicle.vertical ? g.cell_rect(vehicle.lane, pos_[i]) : g.cell_rect(pos_[i], vehicle.lane);
    const gfx::Rect b =
        vehicle.vertical ? g.cell_rect(vehicle.lane, end) : g.cell_rect(end, vehicle.lane);
    const gfx::Rect body{a.x, a.y, b.x + b.w - a.x, b.y + b.h - a.y};
    const float want = std::max(5.0f, g.cell * 0.06f);
    const float inset = std::max(0.0f, (body.w * 0.06f - want) / 1.12f);
    return {body.x + inset, body.y + inset, body.w - 2 * inset, body.h - 2 * inset};
}

void TrafficJamScene::draw_vehicle(gfx::DrawList &list, const kit::Grid &g, int v, float along,
                                   float lift, float pop) const
{
    const auto i = static_cast<std::size_t>(v);
    const Vehicle &vehicle = puzzle_.vehicles[i];
    const float step = g.cell + g.gap;
    const float span = static_cast<float>(vehicle.length) * g.cell +
                       static_cast<float>(vehicle.length - 1) * g.gap;
    const float lane = static_cast<float>(vehicle.lane) * step;
    const float inset = g.cell * 0.05f;
    gfx::Rect body = vehicle.vertical ? gfx::Rect{g.x + lane, g.y + along * step, g.cell, span}
                                      : gfx::Rect{g.x + along * step, g.y + lane, span, g.cell};
    body = {body.x + inset, body.y + inset, body.w - 2 * inset, body.h - 2 * inset};
    body = scaled(body, 1.0f + 0.04f * lift + pop);
    const float thick = std::min(body.w, body.h);
    const float radius = thick * 0.24f;

    // A held vehicle floats: a wider, softer shadow further below it.
    if (lift > 0.01f)
        list.shadow({body.x + 2, body.y + thick * 0.12f * lift, body.w - 4, body.h}, radius,
                    thick * 0.3f, kit::look::kTileShadow.with_alpha(0.35f * lift));
    const gfx::Color color = vehicle_color(v);
    kit::draw_tile(list, body, color, 0.24f);

    const Frame f{body, vehicle.vertical, v != 0 && v % 2 == 0};
    const gfx::Color glass = mix(color, gfx::Color::rgb(0xfffefa), 0.62f);
    const gfx::Color light = mix(color, gfx::Color::rgb(0xfffefa), 0.22f);
    const gfx::Color dark = mix(color, kit::look::kInk, 0.18f);
    const auto rounded = [&](const gfx::Rect &r, gfx::Color c, float k)
    { list.rounded_rect(r, std::min(r.w, r.h) * k, c); };
    const float len = static_cast<float>(vehicle.length);
    if (vehicle.length == 2)
    {
        // Car: rear window, roof, windscreen (and headlights below).
        rounded(f.part(0.12f, 0.24f, 0.2f, 0.8f), glass, 0.4f);
        rounded(f.part(0.27f, 0.54f, 0.14f, 0.86f), light, 0.3f);
        rounded(f.part(0.57f, 0.75f, 0.16f, 0.84f), glass, 0.4f);
    }
    else
    {
        // Truck: a cargo box with ribs, then the cab with its windscreen.
        const float cab = 1.0f - 0.9f / len;
        rounded(f.part(0.05f, cab - 0.03f, 0.12f, 0.88f), light, 0.18f);
        for (int rib = 1; rib <= 2; ++rib)
        {
            const float t = 0.12f + 0.76f * static_cast<float>(rib) / 3.0f;
            const gfx::Rect r = f.part(0.10f, cab - 0.08f, t - 0.018f, t + 0.018f);
            rounded(r, dark.with_alpha(0.35f), 0.5f);
        }
        rounded(f.part(cab + 0.06f, cab + 0.06f + 0.16f / len * 2.0f, 0.17f, 0.83f), glass, 0.35f);
    }
    // Headlights at the front corners.
    const float lamp = thick * 0.055f;
    for (float t : {0.2f, 0.8f})
    {
        float x = 0.0f;
        float y = 0.0f;
        f.point(1.0f - 0.05f / len * 2.0f, t, &x, &y);
        list.circle(x, y, lamp, gfx::Color::rgb(0xfff4c8));
    }
}

void TrafficJamScene::draw_board(gfx::DrawList &list) const
{
    const kit::Grid g = grid();
    const float solved_t = solved_progress();

    // Parking bays.
    const float bay_radius = g.cell * 0.16f;
    for (int row = 0; row < kSide; ++row)
        for (int col = 0; col < kSide; ++col)
            list.rounded_rect(g.cell_rect(col, row), bay_radius, kit::look::kWell);

    // The exit: a gap in the card's right edge on the red car's row (painted
    // in the backdrop's colour there), with chevrons pulsing outwards.
    const gfx::Rect last = g.cell_rect(kSide - 1, kExitRow);
    const float card_right = g.card.x + g.card.w;
    const float cy = last.y + last.h * 0.5f;
    const float gap_h = g.cell * 0.84f;
    const gfx::Color backdrop =
        mix(mix(ui::theme::kBackgroundTop, ui::theme::kBackgroundBottom, cy / 1080.0f),
            gfx::Color::rgb(0x7775c5), 0.06f);
    const float gap_x = last.x + last.w + g.gap * 0.5f;
    list.rounded_rect({gap_x, cy - gap_h * 0.5f, card_right + 2.0f - gap_x, gap_h},
                      std::min(g.gap, gap_h * 0.2f), backdrop);
    list.rounded_rect({gap_x + g.gap, cy - gap_h * 0.5f, card_right + 2.0f - gap_x - g.gap, gap_h},
                      0.0f, backdrop);
    const float fade = solved_t > 0.0f ? 1.0f - solved_t : 1.0f;
    for (int k = 0; k < 2; ++k)
    {
        const float phase = std::fmod(time() * 1.2f - static_cast<float>(k) * 0.3f, 1.0f);
        const float glow = 0.3f + 0.6f * std::max(0.0f, std::sin(phase * kPi));
        chevron(list, card_right + g.cell * (0.16f + 0.17f * static_cast<float>(k)), cy,
                g.cell * 0.24f, std::max(3.0f, g.cell * 0.045f),
                vehicle_color(0).with_alpha(glow * fade));
    }

    // Vehicles; the held one last so it floats above its neighbours.
    const float exit_x = static_cast<float>(kSide - 1);
    const auto draw = [&](int v)
    {
        const auto i = static_cast<std::size_t>(v);
        float along = along_[i].value;
        float pop = 0.0f;
        float alpha = 1.0f;
        if (solved_t > 0.0f)
        {
            if (v == 0)
            {
                // The red car accelerates out through the exit and fades away.
                const float drive = tween::clamp01(solved_t / 0.7f);
                along += drive * drive * drive * 3.6f;
                alpha =
                    1.0f - tween::clamp01((along - static_cast<float>(kSide - 2) - 1.2f) / 1.6f);
            }
            else
            {
                // Then a ripple runs back across the lot from the exit.
                const Vehicle &vehicle = puzzle_.vehicles[i];
                const float mid = along + static_cast<float>(vehicle.length - 1) * 0.5f;
                const float col = vehicle.vertical ? static_cast<float>(vehicle.lane) : mid;
                const float row = vehicle.vertical ? mid : static_cast<float>(vehicle.lane);
                const float distance = std::fabs(exit_x - col) + std::fabs(kExitRow - row);
                const float wave = tween::clamp01((solved_t - 0.35f) * 2.6f - distance * 0.12f);
                pop = 0.07f * std::sin(wave * kPi);
            }
        }
        if (alpha <= 0.0f)
            return;
        list.push_opacity(alpha);
        draw_vehicle(list, g, v, along, lift_[i].value, pop);
        list.pop_opacity();
    };
    for (int v = 0; v < puzzle_.count; ++v)
        if (v != grabbed_)
            draw(v);
    if (grabbed_ >= 0 && grabbed_ < puzzle_.count)
        draw(grabbed_);
}

std::vector<ui::Hint> TrafficJamScene::hints() const
{
    if (grabbed_ >= 0)
        return {{ui::Button::cross, "Release"}, {ui::Button::dpad, "Slide"}};
    return {{ui::Button::cross, "Grab"}};
}

bool TrafficJamScene::extra_stat(std::string *label, std::string *value) const
{
    *label = "PAR";
    *value = std::to_string(puzzle_.par);
    return true;
}

} // namespace ppz::trafficjam
