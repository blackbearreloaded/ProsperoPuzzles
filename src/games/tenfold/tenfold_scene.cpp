// ProsperoPuzzles - Tenfold play screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/tenfold/tenfold_scene.hpp"

#include "core/bytes.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace ppz::tenfold
{

namespace
{

constexpr float kCell = 104.0f;
constexpr float kGap = 10.0f;
constexpr float kBoard = kCell * 6 + kGap * 7;
constexpr float kBoardX = 960.0f - kBoard * 0.5f;
constexpr float kBoardY = 240.0f;
constexpr float kFallSeconds = 0.42f;
constexpr std::uint8_t kSaveVersion = 1;

// Tenfold's palette, one colour per value 1..10 (from the WIP renderer).
constexpr std::uint32_t kPalette[] = {0,        0x64b8a6, 0x469fca, 0x7775c5, 0xae75bb, 0xd97799,
                                      0xed836d, 0xdf9a43, 0x7eac59, 0x3c9290, 0x28334f};

gfx::Color value_color(std::uint8_t value)
{
    return gfx::Color::rgb(value < sizeof(kPalette) / sizeof(kPalette[0]) ? kPalette[value]
                                                                          : 0x353354);
}

float cell_x(int index)
{
    return kBoardX + kGap + static_cast<float>(index % 6) * (kCell + kGap);
}

float row_y(float row)
{
    return kBoardY + kGap + row * (kCell + kGap);
}

std::uint64_t fresh_seed()
{
    static std::uint64_t counter = 0x10f01d;
    counter += 0x9e3779b97f4a7c15ULL;
    return counter ^
           static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
}

std::string format_number(std::uint64_t value)
{
    char digits[32];
    std::snprintf(digits, sizeof(digits), "%llu", static_cast<unsigned long long>(value));
    std::string raw = digits;
    std::string out;
    for (std::size_t i = 0; i < raw.size(); ++i)
    {
        if (i != 0 && (raw.size() - i) % 3 == 0)
            out += ',';
        out += raw[i];
    }
    return out;
}

} // namespace

std::string encode_game(const Snapshot &state)
{
    bytes::Writer w;
    w.put(kSaveVersion);
    for (std::uint8_t cell : state.cells)
        w.put(cell);
    w.put(state.score);
    w.put(state.best);
    w.put(state.random_state);
    w.put(state.moves);
    w.put(state.highest);
    w.put_bool(state.won);
    w.put_bool(state.game_over);
    return w.data();
}

bool decode_game(std::string_view data, Snapshot *state)
{
    bytes::Reader r(data);
    if (r.get<std::uint8_t>() != kSaveVersion)
        return false;
    Snapshot s;
    for (auto &cell : s.cells)
        cell = r.get<std::uint8_t>();
    s.score = r.get<std::uint64_t>();
    s.best = r.get<std::uint64_t>();
    s.random_state = r.get<std::uint64_t>();
    s.moves = r.get<std::uint32_t>();
    s.highest = r.get<std::uint8_t>();
    s.won = r.get_bool();
    s.game_over = r.get_bool();
    Game check;
    if (!r.finished() || !check.restore(s))
        return false;
    *state = s;
    return true;
}

std::string encode_stats(const Stats &stats)
{
    bytes::Writer w;
    w.put(kSaveVersion);
    w.put(stats.best);
    w.put(stats.played);
    w.put(stats.reached_ten);
    w.put(stats.highest);
    return w.data();
}

bool decode_stats(std::string_view data, Stats *stats)
{
    bytes::Reader r(data);
    if (r.get<std::uint8_t>() != kSaveVersion)
        return false;
    Stats s;
    s.best = r.get<std::uint64_t>();
    s.played = r.get<std::uint32_t>();
    s.reached_ten = r.get<std::uint32_t>();
    s.highest = r.get<std::uint8_t>();
    if (!r.finished())
        return false;
    *stats = s;
    return true;
}

TenfoldScene::TenfoldScene(const ui::Fonts &fonts) : fonts_(fonts), game_(fresh_seed())
{
    cursor_x_.snap(static_cast<float>(cursor_ % 6));
    cursor_y_.snap(static_cast<float>(cursor_ / 6));
}

void TenfoldScene::start(const std::string &save, const std::string &stats)
{
    decode_stats(stats, &stats_);
    Snapshot state;
    if (!save.empty() && decode_game(save, &state))
    {
        game_.restore(state);
        finish_recorded_ = state.game_over;
        if (state.game_over)
            open_game_over();
    }
    else
    {
        new_game();
    }
    game_.set_best(stats_.best);
}

void TenfoldScene::new_game()
{
    game_.new_game(fresh_seed());
    game_.set_best(stats_.best);
    cursor_ = 14;
    selected_ = -1;
    finish_recorded_ = false;
    last_ = MoveResult{};
    for (auto &motion : last_.motion)
        motion = {-3, true};
    for (std::size_t i = 0; i < kCells; ++i)
        last_.motion[i].from_row = static_cast<std::int8_t>(static_cast<int>(i / kSide) - 6);
    fall_.start(kFallSeconds * 1.3f);
}

Group TenfoldScene::selection() const
{
    return selected_ >= 0 ? game_.group_at(static_cast<std::uint8_t>(selected_)) : Group{};
}

void TenfoldScene::record_finish()
{
    if (finish_recorded_)
        return;
    finish_recorded_ = true;
    ++stats_.played;
}

void TenfoldScene::open_game_over()
{
    menu_.open(
        "No more moves",
        {{"New game", kNewGame}, {"Undo", kUndo, game_.can_undo()}, {"Back to library", kLibrary}},
        "Score " + format_number(game_.snapshot().score));
}

void TenfoldScene::undo(std::vector<audio::Cue> &cues)
{
    if (!game_.undo())
    {
        cues.push_back(audio::Cue::invalid);
        return;
    }
    selected_ = -1;
    finish_recorded_ = false;
    last_ = MoveResult{};
    cues.push_back(audio::Cue::undo);
}

void TenfoldScene::press_cross(std::vector<audio::Cue> &cues)
{
    const auto cell = static_cast<std::uint8_t>(cursor_);
    const Group current = selection();
    if (selected_ >= 0 && current.contains(cell))
    {
        const MoveResult result = game_.merge(static_cast<std::uint8_t>(selected_), cell);
        selected_ = -1;
        if (!result.changed)
        {
            cues.push_back(audio::Cue::invalid);
            return;
        }
        last_ = result;
        fall_.start(kFallSeconds);
        cues.push_back(audio::Cue::merge);
        const Snapshot &state = game_.snapshot();
        stats_.best = std::max(stats_.best, state.best);
        stats_.highest = std::max(stats_.highest, state.highest);
        if (result.first_ten)
        {
            ++stats_.reached_ten;
            cues.push_back(audio::Cue::complete);
            menu_.open("You made ten!",
                       {{"Keep playing", kKeepPlaying},
                        {"New game", kNewGame},
                        {"Back to library", kLibrary}},
                       "Score " + format_number(state.score));
        }
        else if (result.game_over)
        {
            record_finish();
            cues.push_back(audio::Cue::game_over);
            open_game_over();
        }
        return;
    }
    const Group group = game_.group_at(cell);
    if (group.count < 2)
    {
        cues.push_back(audio::Cue::invalid);
        shake_.start(0.14f);
        return;
    }
    selected_ = cursor_;
    cues.push_back(audio::Cue::place);
}

void TenfoldScene::hint(std::vector<audio::Cue> &cues)
{
    // The largest group, preferring ones near the cursor.
    int best_cell = -1;
    int best_score = -1;
    for (std::uint8_t cell = 0; cell < kCells; ++cell)
    {
        const Group group = game_.group_at(cell);
        if (group.count < 2)
            continue;
        const int distance = std::abs(cell % 6 - cursor_ % 6) + std::abs(cell / 6 - cursor_ / 6);
        const int score = group.count * 20 + game_.snapshot().cells[cell] * 8 - distance;
        if (score > best_score)
        {
            best_score = score;
            best_cell = cell;
        }
    }
    if (best_cell < 0)
    {
        cues.push_back(audio::Cue::invalid);
        return;
    }
    cursor_ = best_cell;
    selected_ = -1;
    hint_.start(1.2f);
    cues.push_back(audio::Cue::ui_notify);
}

games::SceneExit TenfoldScene::update(const InputFrame &input, float dt,
                                      std::vector<audio::Cue> &cues)
{
    time_ += dt;
    fall_.update(dt);
    shake_.update(dt);
    hint_.update(dt);
    cursor_x_.target = static_cast<float>(cursor_ % 6);
    cursor_y_.target = static_cast<float>(cursor_ / 6);
    cursor_x_.update(dt, 22.0f);
    cursor_y_.update(dt, 22.0f);

    const bool finished = game_.snapshot().game_over;
    if (menu_.is_open())
    {
        switch (menu_.update(input, dt, cues, !finished))
        {
        case kNewGame:
            new_game();
            cues.push_back(audio::Cue::new_game);
            break;
        case kUndo:
            undo(cues);
            break;
        case kKeepPlaying:
            if (game_.snapshot().game_over)
                open_game_over();
            break;
        case kHowTo:
            return games::SceneExit::howto;
        case kLibrary:
            return games::SceneExit::library;
        default:
            break;
        }
        return games::SceneExit::none;
    }
    menu_.update(InputFrame{}, dt, cues);

    if (input.is_pressed(Action::menu) || input.focus_lost ||
        (input.is_pressed(Action::back) && selected_ < 0))
    {
        menu_.open("Paused",
                   {{"Resume", kResume},
                    {"New game", kNewGame},
                    {"Undo", kUndo, game_.can_undo()},
                    {"How to play", kHowTo},
                    {"Back to library", kLibrary}},
                   "Tenfold");
        cues.push_back(audio::Cue::ui_pause_open);
        return games::SceneExit::none;
    }
    if (input.is_pressed(Action::back))
    {
        selected_ = -1;
        cues.push_back(audio::Cue::ui_back);
    }
    switch (input.nav)
    {
    case ppz::Direction::up:
        cursor_ = cursor_ >= 6 ? cursor_ - 6 : cursor_;
        break;
    case ppz::Direction::down:
        cursor_ = cursor_ + 6 < static_cast<int>(kCells) ? cursor_ + 6 : cursor_;
        break;
    case ppz::Direction::left:
        cursor_ = cursor_ % 6 > 0 ? cursor_ - 1 : cursor_;
        break;
    case ppz::Direction::right:
        cursor_ = cursor_ % 6 < 5 ? cursor_ + 1 : cursor_;
        break;
    default:
        break;
    }
    if (input.nav != ppz::Direction::none)
        cues.push_back(audio::Cue::cursor);
    if (input.is_pressed(Action::confirm))
        press_cross(cues);
    if (input.is_pressed(Action::west))
        hint(cues);
    if (input.is_pressed(Action::page_prev))
        undo(cues);
    return games::SceneExit::none;
}

std::string TenfoldScene::save()
{
    return encode_game(game_.snapshot());
}

std::string TenfoldScene::stats()
{
    return encode_stats(stats_);
}

bool TenfoldScene::in_progress()
{
    return game_.snapshot().moves > 0 && !game_.snapshot().game_over;
}

void TenfoldScene::draw(gfx::DrawList &list) const
{
    using gfx::Align;
    using gfx::Color;
    const Snapshot &state = game_.snapshot();
    list.gradient_rect({0, 0, 1920, 1080}, 0, ui::theme::kBackgroundTop,
                       ui::theme::kBackgroundBottom);
    list.shadow({560, 240, 800, 640}, 220, 260, Color::rgb(0x7775c5, 0.12f));

    list.text(*fonts_.semibold, fonts_.semibold_texture, "Tenfold", ui::theme::kSafeMargin, 150, 88,
              ui::theme::kTextOnDark);
    list.text(*fonts_.regular, fonts_.regular_texture,
              "Merge groups of equal numbers. Make it to ten.", ui::theme::kSafeMargin, 200, 26,
              ui::theme::kTextOnDarkMuted);

    const std::pair<const char *, std::string> boxes[] = {
        {"SCORE", format_number(state.score)},
        {"BEST", format_number(std::max(state.best, stats_.best))},
        {"MOVES", format_number(state.moves)}};
    for (int i = 0; i < 3; ++i)
    {
        const float y = 252.0f + static_cast<float>(i) * 130.0f;
        list.rounded_rect({1380, y, 300, 110}, 20, Color::rgb(0xffffff, 0.08f));
        list.text(*fonts_.semibold, fonts_.semibold_texture, boxes[i].first, 1410, y + 40, 20,
                  ui::theme::kTextOnDarkMuted);
        list.text(*fonts_.semibold, fonts_.semibold_texture, boxes[i].second, 1410, y + 88, 40,
                  ui::theme::kTextOnDark);
    }
    // Climb meter toward ten.
    const float climb = std::min(10.0f, static_cast<float>(state.highest)) / 10.0f;
    list.text(*fonts_.semibold, fonts_.semibold_texture, "YOUR CLIMB", 1410, 684, 20,
              ui::theme::kTextOnDarkMuted);
    list.rounded_rect({1410, 700, 240, 18}, 9, Color::rgb(0xffffff, 0.1f));
    list.rounded_rect({1410, 700, 240 * climb, 18}, 9,
                      value_color(std::min<std::uint8_t>(state.highest, 10)));

    float shake = 0.0f;
    if (shake_.running)
        shake = 7.0f * std::sin(shake_.progress() * 6.2831853f * 3.0f) * (1.0f - shake_.progress());
    list.push_transform(1.0f, 0, 0, shake, 0);
    list.shadow({kBoardX + 8, kBoardY + 22, kBoard - 16, kBoard}, 22, 34, ui::theme::kShadow);
    list.rounded_rect({kBoardX, kBoardY, kBoard, kBoard}, 22, Color::rgb(0xe6e3dc));
    list.push_clip({kBoardX, kBoardY, kBoard, kBoard});

    const Group selected = selection();
    const float fall = tween::cubic_out(fall_.progress());
    for (int i = 0; i < static_cast<int>(kCells); ++i)
    {
        const std::uint8_t value = state.cells[static_cast<std::size_t>(i)];
        const int row = i / 6;
        const TileMotion &motion = last_.motion[static_cast<std::size_t>(i)];
        float y_row = static_cast<float>(row);
        float alpha = 1.0f;
        if (fall_.running)
        {
            y_row = tween::lerp(static_cast<float>(motion.from_row), static_cast<float>(row), fall);
            if (motion.spawned)
                alpha = std::min(1.0f, fall * 1.6f);
        }
        const float x = cell_x(i);
        const float y = row_y(y_row);
        float scale = 1.0f;
        if (i == last_.destination && fall_.running && fall > 0.7f)
            scale = 1.0f + 0.12f * std::sin((fall - 0.7f) / 0.3f * 3.14159265f);
        if (selected.contains(static_cast<std::uint8_t>(i)))
            scale *= 1.0f + 0.04f * std::sin(time_ * 7.0f);
        const float size = kCell * scale;
        const float offset = (size - kCell) * 0.5f;
        list.push_opacity(alpha);
        list.rounded_rect({x - offset, y - offset + 4, size, size}, 16,
                          Color::rgb(0x394259, 0.18f));
        list.rounded_rect({x - offset, y - offset, size, size}, 16, value_color(value));
        char label[8];
        std::snprintf(label, sizeof(label), "%u", static_cast<unsigned>(value));
        list.text(*fonts_.semibold, fonts_.semibold_texture, label, x + kCell * 0.5f,
                  y + kCell * 0.5f + 18, 50, Color::rgb(0xfffefa), Align::center);
        if (selected.contains(static_cast<std::uint8_t>(i)))
            list.bordered_rect({x - offset - 4, y - offset - 4, size + 8, size + 8}, 19,
                               Color{1, 1, 1, 0}, 4, Color::rgb(0xffffff, 0.85f));
        list.pop_opacity();
    }
    list.pop_clip();

    // Cursor ring glides between cells; a hint makes it pulse.
    const float cx = kBoardX + kGap + cursor_x_.value * (kCell + kGap);
    const float cy = row_y(cursor_y_.value);
    const float pulse = hint_.running ? 0.5f + 0.5f * std::sin(time_ * 12.0f) : 1.0f;
    list.bordered_rect({cx - 7, cy - 7, kCell + 14, kCell + 14}, 22, Color{0, 0, 0, 0}, 5,
                       ui::theme::kFocus.with_alpha(pulse));
    list.pop_transform();

    using ui::Button;
    const ui::Hint primary[] = {{Button::cross, selected_ >= 0 ? "Merge here" : "Select group"},
                                {Button::square, "Hint"}};
    ui::draw_hints(list, fonts_, primary, 2, ui::theme::kSafeMargin, false);
    const ui::Hint secondary[] = {
        {Button::circle, "Deselect"}, {Button::l1, "Undo"}, {Button::options, "Menu"}};
    ui::draw_hints(list, fonts_, secondary, 3, 1920.0f - ui::theme::kSafeMargin, true,
                   ui::theme::kTextOnDarkMuted);
    menu_.draw(list, fonts_);
}

} // namespace ppz::tenfold
