// ProsperoPuzzles - Color Sort play screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/colorsort/colorsort_scene.hpp"

#include "core/bytes.hpp"

#include <algorithm>
#include <cmath>

namespace ppz::colorsort
{

namespace
{

constexpr int kColours[3] = {4, 6, 8};
constexpr std::uint8_t kFormat = 1;
constexpr float kPi = 3.14159265f;
constexpr float kFlightTime = 0.42f; // seconds a ball spends in the air
constexpr float kStagger = 0.07f;    // between balls of one pour
constexpr float kSettled = 10.0f;    // badge_ value for tubes that were already complete

// Tube proportions, in tube widths.
constexpr float kBall = 0.8f;    // ball diameter
constexpr float kStep = 0.82f;   // between stacked ball centres
constexpr float kBottom = 0.1f;  // below the lowest ball
constexpr float kTop = 0.36f;    // above the highest ball
constexpr float kHead = 1.15f;   // room above the tube for lifted balls
constexpr float kHover = 0.14f;  // gap between the tube's mouth and a lifted ball
constexpr float kGapY = 0.32f;   // between rows
constexpr float kMinGap = 0.5f;  // between tubes in a row
constexpr float kMaxGap = 0.95f; // tubes stay grouped on wide boards

std::size_t at(int index)
{
    return static_cast<std::size_t>(index);
}

// Tenfold hues chosen to stay apart from one another.
gfx::Color ball_color(int colour)
{
    constexpr int kHueIndex[kMaxColours] = {0, 1, 2, 3, 4, 5, 6, 10};
    return kit::look::hue(kHueIndex[at(std::clamp(colour, 0, kMaxColours - 1))]);
}

gfx::Color mix(gfx::Color a, gfx::Color b, float t)
{
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t,
            a.a + (b.a - a.a) * t};
}

// A glossy ball: a darker rim, the body, a soft sheen and a specular dot.
void draw_ball(gfx::DrawList &list, float x, float y, float radius, int colour)
{
    const gfx::Color base = ball_color(colour);
    const gfx::Color white = gfx::Color::rgb(0xffffff);
    list.circle(x, y + radius * 0.14f, radius * 0.96f, kit::look::kTileShadow);
    list.circle(x, y, radius, mix(base, kit::look::kInk, 0.22f));
    list.circle(x - radius * 0.06f, y - radius * 0.07f, radius * 0.87f, base);
    list.circle(x - radius * 0.2f, y - radius * 0.24f, radius * 0.5f,
                mix(base, white, 0.3f).with_alpha(0.7f));
    list.circle(x - radius * 0.33f, y - radius * 0.38f, radius * 0.16f, white.with_alpha(0.92f));
}

} // namespace

// ---- layout ----

gfx::Rect ColorSortScene::Layout::tube(float col, float row) const
{
    return {x + col * (tube_w + gap_x), y + row * (head + tube_h + gap_y) + head, tube_w, tube_h};
}

gfx::Rect ColorSortScene::Layout::tube(int index) const
{
    const int c = std::max(1, cols);
    return tube(static_cast<float>(index % c), static_cast<float>(index / c));
}

float ColorSortScene::Layout::slot_x(int index) const
{
    const gfx::Rect r = tube(index);
    return r.x + r.w * 0.5f;
}

float ColorSortScene::Layout::slot_y(int index, int slot) const
{
    const gfx::Rect r = tube(index);
    return r.y + r.h - tube_w * kBottom - ball * 0.5f - static_cast<float>(slot) * step;
}

float ColorSortScene::Layout::hover_y(int index) const
{
    return tube(index).y - tube_w * kHover - ball * 0.5f;
}

ColorSortScene::Layout ColorSortScene::layout() const
{
    Layout l;
    l.cols = grid_cols();
    const int rows = grid_rows();
    const gfx::Rect area = kit::kBoardArea;
    const float pad_x = 34.0f;
    const float pad_top = 14.0f;
    const float pad_bottom = 30.0f;
    const float tube_units = kBottom + static_cast<float>(kCapacity - 1) * kStep + kBall + kTop;
    const float units_h =
        static_cast<float>(rows) * (kHead + tube_units) + static_cast<float>(rows - 1) * kGapY;
    const float units_w = static_cast<float>(l.cols) + static_cast<float>(l.cols - 1) * kMinGap;
    const float avail_w = area.w - 2 * pad_x;
    const float avail_h = area.h - pad_top - pad_bottom;
    const float w = std::min({avail_w / units_w, avail_h / units_h, 104.0f});
    l.tube_w = w;
    l.tube_h = w * tube_units;
    l.head = w * kHead;
    l.ball = w * kBall;
    l.step = w * kStep;
    l.gap_y = w * kGapY;
    l.gap_x = 0.0f;
    if (l.cols > 1)
        l.gap_x =
            std::clamp((avail_w - static_cast<float>(l.cols) * w) / static_cast<float>(l.cols - 1),
                       w * kMinGap, w * kMaxGap);
    const float total_w = static_cast<float>(l.cols) * w + static_cast<float>(l.cols - 1) * l.gap_x;
    const float total_h = w * units_h;
    l.x = area.x + (area.w - total_w) * 0.5f;
    l.y = area.y + (area.h - total_h - pad_top - pad_bottom) * 0.5f + pad_top;
    l.card = {l.x - pad_x, l.y - pad_top, total_w + 2 * pad_x, total_h + pad_top + pad_bottom};
    return l;
}

int ColorSortScene::grid_cols() const
{
    const int tubes = std::max(1, board_.tubes);
    return tubes <= 6 ? tubes : (tubes + 1) / 2;
}

int ColorSortScene::grid_rows() const
{
    return board_.tubes <= 6 ? 1 : 2;
}

kit::Grid ColorSortScene::grid() const
{
    const Layout l = layout();
    kit::Grid g;
    g.cols = l.cols;
    g.rows = grid_rows();
    g.x = l.x;
    g.y = l.y;
    g.cell = l.tube_w;
    g.gap = l.gap_x;
    g.card = l.card;
    return g;
}

gfx::Rect ColorSortScene::cursor_rect(float col, float row) const
{
    return layout().tube(col, row);
}

int ColorSortScene::tube_at(int col, int row) const
{
    const int tube = row * grid_cols() + col;
    return tube >= 0 && tube < board_.tubes ? tube : -1;
}

// ---- rules and state ----

ColorSortScene::ColorSortScene(const ui::Fonts &fonts)
    : kit::PuzzleScene(fonts, {"colorsort",
                               "Color Sort",
                               "Pour the balls until every tube holds one colour.",
                               {"4 colours", "6 colours", "8 colours"},
                               0})
{
}

void ColorSortScene::clear_motion()
{
    lifted_ = -1;
    lifted_count_.fill(0);
    lift_px_.fill(0.0f);
    for (tween::Spring &spring : lift_)
    {
        spring.snap(0.0f);
        spring.target = 0.0f;
    }
    land_.fill(0.0f);
    badge_.fill(kSettled);
    flights_.clear();
}

void ColorSortScene::generate(std::uint64_t seed, int size)
{
    start_ = colorsort::generate(seed, kColours[std::clamp(size, 0, 2)]);
    board_ = start_;
    clear_motion();
}

void ColorSortScene::restart()
{
    board_ = start_;
    clear_motion();
}

std::string ColorSortScene::serialize() const
{
    bytes::Writer w;
    w.put(kFormat);
    w.put(static_cast<std::uint8_t>(board_.colours));
    for (const Board *b : {&start_, &board_})
        for (int t = 0; t < b->tubes; ++t)
        {
            w.put(b->count[at(t)]);
            for (int i = 0; i < b->count[at(t)]; ++i)
                w.put(b->ball[at(t)][at(i)]);
        }
    return w.data();
}

bool ColorSortScene::deserialize(std::string_view data)
{
    bytes::Reader r(data);
    if (r.get<std::uint8_t>() != kFormat)
        return false;
    const int colours = r.get<std::uint8_t>();
    if (colours < 2 || colours > kMaxColours)
        return false;
    Board boards[2];
    for (Board &b : boards)
    {
        b.colours = colours;
        b.tubes = colours + kEmptyTubes;
        for (int t = 0; t < b.tubes; ++t)
        {
            const std::uint8_t count = r.get<std::uint8_t>();
            if (count > kCapacity)
                return false;
            b.count[at(t)] = count;
            for (int i = 0; i < count; ++i)
                b.ball[at(t)][at(i)] = r.get<std::uint8_t>();
        }
        if (!valid(b))
            return false;
    }
    if (!r.finished())
        return false;
    start_ = boards[0];
    board_ = boards[1];
    clear_motion();
    return true;
}

bool ColorSortScene::solved() const
{
    return colorsort::solved(board_);
}

float ColorSortScene::lift_offset(int tube) const
{
    if (tube < 0 || tube >= kMaxTubes || lifted_count_[at(tube)] == 0)
        return 0.0f;
    const float raised = std::max(0.0f, lift_[at(tube)].value);
    float bob = 0.0f;
    if (tube == lifted_)
        bob = std::sin(time() * 4.2f) * layout().ball * 0.07f * raised;
    return lift_px_[at(tube)] * raised + bob;
}

void ColorSortScene::lift(int tube, std::vector<audio::Cue> &cues)
{
    const int count = board_.count[at(tube)];
    if (count == 0)
    {
        reject(cues);
        return;
    }
    // Balls still in the air towards this tube land at once.
    std::erase_if(flights_, [tube](const Flight &f) { return f.tube == tube; });
    const Layout l = layout();
    lifted_ = tube;
    lifted_count_[at(tube)] = top_run(board_, tube);
    lift_px_[at(tube)] = l.slot_y(tube, count - 1) - l.hover_y(tube);
    lift_[at(tube)].target = 1.0f;
    cues.push_back(audio::Cue::pickup);
}

void ColorSortScene::put_back(std::vector<audio::Cue> &cues)
{
    if (lifted_ < 0)
        return;
    lift_[at(lifted_)].target = 0.0f;
    lifted_ = -1;
    cues.push_back(audio::Cue::slide);
}

void ColorSortScene::pour_into(int tube, std::vector<audio::Cue> &cues)
{
    const int from = lifted_;
    const int amount = pour_amount(board_, from, tube);
    if (amount == 0)
    {
        reject(cues); // the selection stays lifted
        return;
    }
    remember();
    const Layout l = layout();
    const float offset = lift_offset(from);
    const int from_count = board_.count[at(from)];
    const int to_count = board_.count[at(tube)];
    for (int i = 0; i < amount; ++i)
    {
        const int slot = from_count - 1 - i; // the top ball leaves first
        Flight f;
        f.colour = board_.ball[at(from)][at(slot)];
        f.tube = tube;
        f.slot = to_count + i;
        f.from_x = l.slot_x(from);
        f.from_y = l.slot_y(from, slot) - offset;
        f.t = -static_cast<float>(i) * kStagger;
        flights_.push_back(f);
    }
    pour(board_, from, tube);
    lifted_count_[at(from)] = std::max(0, lifted_count_[at(from)] - amount);
    lift_[at(from)].target = 0.0f;
    lifted_ = -1;
    if (tube_complete(board_, tube))
        badge_[at(tube)] = -(static_cast<float>(amount - 1) * kStagger + kFlightTime);
    cues.push_back(audio::Cue::drop);
}

void ColorSortScene::play(const InputFrame &input, std::vector<audio::Cue> &cues)
{
    const int tube = tube_at(cursor_col(), cursor_row());
    if (tube < 0)
        return;
    if (input.is_pressed(Action::confirm))
    {
        if (lifted_ < 0)
            lift(tube, cues);
        else if (tube == lifted_)
            put_back(cues);
        else
            pour_into(tube, cues);
    }
    else if (input.is_pressed(Action::back) && lifted_ >= 0)
    {
        put_back(cues);
    }
}

void ColorSortScene::animate(float dt)
{
    for (tween::Spring &spring : lift_)
        spring.update(dt, 20.0f);
    for (float &land : land_)
        land = std::max(0.0f, land - dt * 4.0f);
    for (float &badge : badge_)
        badge = std::min(kSettled, badge + dt);
    for (Flight &f : flights_)
    {
        f.t += dt;
        if (f.t >= kFlightTime && f.tube >= 0 && f.tube < kMaxTubes)
            land_[at(f.tube)] = 1.0f;
    }
    std::erase_if(flights_, [](const Flight &f) { return f.t >= kFlightTime; });
}

// ---- drawing ----

void ColorSortScene::draw_board(gfx::DrawList &list) const
{
    const Layout l = layout();
    const float w = l.tube_w;
    const float radius = l.ball * 0.5f;
    const float solved_t = solved_progress();
    const gfx::Color glass = mix(kit::look::kBoard, kit::look::kTile, 0.7f);
    const gfx::Color cavity = mix(glass, kit::look::kWell, 0.42f);
    const gfx::Color white = gfx::Color::rgb(0xffffff);

    const auto in_flight = [this](int tube, int slot)
    {
        return std::any_of(flights_.begin(), flights_.end(),
                           [&](const Flight &f) { return f.tube == tube && f.slot == slot; });
    };

    for (int t = 0; t < board_.tubes; ++t)
    {
        const gfx::Rect r = l.tube(t);
        const float cx = r.x + r.w * 0.5f;
        const bool complete = tube_complete(board_, t);
        const int count = board_.count[at(t)];

        // Solved: the tubes pop one after another; landing balls give a small bounce.
        float scale = 1.0f + 0.035f * std::sin(land_[at(t)] * kPi);
        if (solved_t > 0.0f && solved_t < 1.0f)
        {
            const float wave =
                std::clamp(solved_t * 2.4f - static_cast<float>(t) * 0.13f, 0.0f, 1.0f);
            scale += 0.16f * std::sin(wave * kPi);
        }
        list.push_transform(scale, cx, r.y + r.h, 0, 0);

        // A completed tube glows in its colour once its last ball has landed.
        const float badge_t = badge_[at(t)];
        if (complete && badge_t >= 0.0f)
        {
            const float glow = std::clamp(badge_t / 0.3f, 0.0f, 1.0f);
            list.shadow({r.x - w * 0.08f, r.y + w * 0.1f, r.w + w * 0.16f, r.h}, w * 0.5f,
                        w * 0.45f, ball_color(board_.top(t)).with_alpha(0.55f * glow));
        }

        // Glass: a flat-topped capsule with a rounded bottom, a darker cavity and a lip.
        list.rounded_rect({r.x, r.y + std::max(2.0f, w * 0.07f), r.w, r.h}, w * 0.5f,
                          kit::look::kTileShadow);
        list.rounded_rect({r.x, r.y, r.w, r.h - w * 0.5f}, w * 0.14f, glass);
        list.rounded_rect({r.x, r.y + r.h - w * 1.2f, r.w, w * 1.2f}, w * 0.5f, glass);
        const float inset = w * 0.09f;
        const gfx::Rect inner{r.x + inset, r.y + w * 0.16f, r.w - 2 * inset,
                              r.h - w * 0.16f - inset};
        list.gradient_rect({inner.x, inner.y, inner.w, inner.h - inner.w * 0.5f}, w * 0.06f, glass,
                           cavity);
        list.rounded_rect({inner.x, inner.y + inner.h - inner.w * 1.1f, inner.w, inner.w * 1.1f},
                          inner.w * 0.5f, cavity);
        list.rounded_rect({r.x - w * 0.08f, r.y - w * 0.02f, r.w + w * 0.16f, w * 0.18f}, w * 0.09f,
                          kit::look::kTileShadow);
        list.rounded_rect({r.x - w * 0.08f, r.y - w * 0.06f, r.w + w * 0.16f, w * 0.18f}, w * 0.09f,
                          kit::look::kTile);

        // Balls, the lifted run raised above the mouth.
        const float offset = lift_offset(t);
        const int raised_from = count - std::min(count, lifted_count_[at(t)]);
        for (int i = 0; i < count; ++i)
        {
            if (in_flight(t, i))
                continue;
            const float y = l.slot_y(t, i) - (i >= raised_from ? offset : 0.0f);
            draw_ball(list, l.slot_x(t), y, radius, board_.ball[at(t)][at(i)]);
        }

        // The glass's reflection runs over the balls.
        list.rounded_rect({r.x + w * 0.17f, r.y + w * 0.3f, w * 0.1f, r.h - w * 0.85f}, w * 0.05f,
                          white.with_alpha(0.42f));
        list.rounded_rect({r.x + w * 0.17f, r.y + r.h - w * 0.45f, w * 0.1f, w * 0.1f}, w * 0.05f,
                          white.with_alpha(0.3f));

        // A star badge above a completed tube.
        if (complete && badge_t >= 0.0f && offset < w * 0.05f)
        {
            const float pop = tween::back_out(std::clamp(badge_t / 0.35f, 0.0f, 1.0f));
            const float by = r.y - l.head * 0.5f;
            const float br = w * 0.25f * pop;
            if (br > 0.5f)
            {
                list.circle(cx, by + br * 0.12f, br, kit::look::kTileShadow);
                list.circle(cx, by, br, kit::look::kTile);
                list.star(cx, by + br * 0.04f, br * 0.66f, ball_color(board_.top(t)));
            }
        }
        list.pop_transform();
    }

    // Balls in the air: an eased arc to above the target, then a drop into place.
    for (const Flight &f : flights_)
    {
        if (f.t < 0.0f)
        {
            // Waiting its turn: still hovering where it was lifted.
            draw_ball(list, f.from_x, f.from_y, radius, f.colour);
            continue;
        }
        const float e = tween::cubic_in_out(std::clamp(f.t / kFlightTime, 0.0f, 1.0f));
        const float hx = l.slot_x(f.tube);
        const float hy = l.hover_y(f.tube);
        float x = hx;
        float y = hy;
        constexpr float kSplit = 0.72f;
        if (e < kSplit)
        {
            const float q = e / kSplit;
            const float arc = l.ball * 0.9f + std::fabs(hx - f.from_x) * 0.14f;
            const float px = (f.from_x + hx) * 0.5f;
            const float py = std::min(f.from_y, hy) - arc;
            const float a = (1 - q) * (1 - q);
            const float b = 2 * (1 - q) * q;
            const float c = q * q;
            x = a * f.from_x + b * px + c * hx;
            y = a * f.from_y + b * py + c * hy;
        }
        else
        {
            const float q = (e - kSplit) / (1.0f - kSplit);
            y = hy + (l.slot_y(f.tube, f.slot) - hy) * q * q;
        }
        draw_ball(list, x, y, radius, f.colour);
    }
}

std::vector<ui::Hint> ColorSortScene::hints() const
{
    if (lifted_ >= 0)
        return {{ui::Button::cross, "Pour"}, {ui::Button::circle, "Put back"}};
    return {{ui::Button::cross, "Lift"}};
}

bool ColorSortScene::extra_stat(std::string *label, std::string *value) const
{
    *label = "SORTED";
    *value = std::to_string(sorted_count(board_)) + " / " + std::to_string(board_.colours);
    return true;
}

} // namespace ppz::colorsort
