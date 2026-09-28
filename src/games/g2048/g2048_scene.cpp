// ProsperoPuzzles - 2048 play screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/g2048/g2048_scene.hpp"

#include "core/bytes.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace ppz::g2048
{

namespace
{

constexpr float kCell = 140.0f;
constexpr float kGap = 16.0f;
constexpr float kBoard = kCell * 4 + kGap * 5;
constexpr float kBoardX = 960.0f - kBoard * 0.5f;
constexpr float kBoardY = 262.0f;
constexpr float kSlideSeconds = 0.12f;
constexpr float kPopSeconds = 0.22f;
constexpr std::uint8_t kSaveVersion = 1;

std::uint32_t tile_color(std::uint8_t exponent)
{
    constexpr std::uint32_t palette[] = {0,        0xeee4da, 0xede0c8, 0xf2b179,
                                         0xf59563, 0xf67c5f, 0xf65e3b, 0xedcf72,
                                         0xedcc61, 0xedc850, 0xedc53f, 0xedc22e};
    return exponent < sizeof(palette) / sizeof(palette[0]) ? palette[exponent] : 0x3c3a32;
}

float cell_x(int index)
{
    return kBoardX + kGap + static_cast<float>(index % 4) * (kCell + kGap) + kCell * 0.5f;
}

float cell_y(int index)
{
    return kBoardY + kGap + static_cast<float>(index / 4) * (kCell + kGap) + kCell * 0.5f;
}

std::uint64_t fresh_seed()
{
    static std::uint64_t counter = 0x2048;
    counter += 0x9e3779b97f4a7c15ULL;
    return counter ^
           static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
}

void put_snapshot(bytes::Writer &w, const Snapshot &s)
{
    for (std::uint8_t cell : s.cells)
        w.put(cell);
    w.put(s.score);
    w.put(s.best);
    w.put(s.random_state);
    w.put<std::uint8_t>(static_cast<std::uint8_t>((s.won ? 1 : 0) | (s.keep_playing ? 2 : 0) |
                                                  (s.game_over ? 4 : 0)));
}

Snapshot get_snapshot(bytes::Reader &r)
{
    Snapshot s;
    for (auto &cell : s.cells)
        cell = r.get<std::uint8_t>();
    s.score = r.get<std::uint64_t>();
    s.best = r.get<std::uint64_t>();
    s.random_state = r.get<std::uint64_t>();
    const auto flags = r.get<std::uint8_t>();
    s.won = (flags & 1) != 0;
    s.keep_playing = (flags & 2) != 0;
    s.game_over = (flags & 4) != 0;
    return s;
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

std::string encode_game(const Snapshot &state, std::uint32_t moves,
                        const std::optional<Snapshot> &undo)
{
    bytes::Writer w;
    w.put(kSaveVersion);
    put_snapshot(w, state);
    w.put(moves);
    w.put_bool(undo.has_value());
    if (undo)
        put_snapshot(w, *undo);
    return w.data();
}

bool decode_game(std::string_view data, Snapshot *state, std::uint32_t *moves,
                 std::optional<Snapshot> *undo)
{
    bytes::Reader r(data);
    if (r.get<std::uint8_t>() != kSaveVersion)
        return false;
    const Snapshot s = get_snapshot(r);
    const auto count = r.get<std::uint32_t>();
    std::optional<Snapshot> previous;
    if (r.get_bool())
        previous = get_snapshot(r);
    if (!r.finished())
        return false;
    Game check;
    if (!check.restore(s) || (previous && !check.restore(*previous)))
        return false;
    *state = s;
    *moves = count;
    *undo = previous;
    return true;
}

std::string encode_stats(const Stats &stats)
{
    bytes::Writer w;
    w.put(kSaveVersion);
    w.put(stats.best);
    w.put(stats.played);
    w.put(stats.won);
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
    s.won = r.get<std::uint32_t>();
    s.highest = r.get<std::uint8_t>();
    if (!r.finished())
        return false;
    *stats = s;
    return true;
}

G2048Scene::G2048Scene(const ui::Fonts &fonts) : fonts_(fonts), game_(fresh_seed())
{
}

void G2048Scene::start(const std::string &save, const std::string &stats)
{
    decode_stats(stats, &stats_);
    Snapshot state;
    std::uint32_t moves = 0;
    std::optional<Snapshot> undo;
    if (!save.empty() && decode_game(save, &state, &moves, &undo))
    {
        state.best = std::max(state.best, stats_.best);
        game_.restore(state);
        moves_ = moves;
        undo_ = undo;
        finish_recorded_ = state.game_over;
        if (state.game_over)
            open_game_over();
    }
    else
    {
        new_game();
    }
}

void G2048Scene::new_game()
{
    game_.new_game(fresh_seed());
    Snapshot state = game_.snapshot();
    state.best = std::max(state.best, stats_.best);
    game_.restore(state);
    undo_.reset();
    moves_ = 0;
    finish_recorded_ = false;
    last_ = MoveResult{};
    pop_.start(kPopSeconds);
    last_.spawned_cell = 0xff;
}

void G2048Scene::record_finish(bool won)
{
    if (finish_recorded_)
        return;
    finish_recorded_ = true;
    ++stats_.played;
    if (won)
        ++stats_.won;
}

void G2048Scene::open_pause(std::vector<audio::Cue> &cues)
{
    menu_.open("Paused",
               {{"Resume", kResume},
                {"New game", kNewGame},
                {"Undo", kUndo, undo_.has_value()},
                {"How to play", kHowTo},
                {"Back to library", kLibrary}},
               "2048");
    cues.push_back(audio::Cue::ui_pause_open);
}

void G2048Scene::open_game_over()
{
    menu_.open(
        "No more moves",
        {{"New game", kNewGame}, {"Undo", kUndo, undo_.has_value()}, {"Back to library", kLibrary}},
        "Score " + format_number(game_.snapshot().score));
}

void G2048Scene::apply_move(Direction direction, std::vector<audio::Cue> &cues)
{
    const Snapshot before = game_.snapshot();
    const MoveResult result = game_.move(direction);
    if (!result.changed)
    {
        cues.push_back(audio::Cue::invalid);
        shake_.start(0.14f);
        return;
    }
    undo_ = before;
    ++moves_;
    last_ = result;
    slide_.start(kSlideSeconds);
    pop_.start(kSlideSeconds + kPopSeconds);
    cues.push_back(audio::Cue::slide);
    if (result.score_gained > 0)
    {
        gained_ = result.score_gained;
        gain_.start(0.7f);
        cues.push_back(audio::Cue::merge);
    }
    const Snapshot &now = game_.snapshot();
    stats_.best = std::max(stats_.best, now.best);
    for (std::uint8_t exponent : now.cells)
        stats_.highest = std::max(stats_.highest, exponent);
    if (result.first_win)
    {
        record_finish(true);
        cues.push_back(audio::Cue::complete);
        menu_.open(
            "You made 2048!",
            {{"Keep going", kKeepGoing}, {"New game", kNewGame}, {"Back to library", kLibrary}},
            "Score " + format_number(now.score));
    }
    else if (result.game_over)
    {
        record_finish(now.won);
        cues.push_back(audio::Cue::game_over);
        open_game_over();
    }
}

games::SceneExit G2048Scene::update(const InputFrame &input, float dt,
                                    std::vector<audio::Cue> &cues)
{
    time_ += dt;
    slide_.update(dt);
    pop_.update(dt);
    shake_.update(dt);
    gain_.update(dt);
    const bool finished = game_.snapshot().game_over;
    if (menu_.is_open())
    {
        const int choice = menu_.update(input, dt, cues, !finished);
        switch (choice)
        {
        case kNewGame:
            new_game();
            cues.push_back(audio::Cue::new_game);
            break;
        case kUndo:
            if (undo_)
            {
                game_.restore(*undo_);
                undo_.reset();
                if (moves_ > 0)
                    --moves_;
                finish_recorded_ = false;
                cues.push_back(audio::Cue::undo);
            }
            break;
        case kKeepGoing:
            game_.keep_playing();
            // A win on the last possible move goes straight to game over
            // (the WIP left a dead board here).
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

    if (input.is_pressed(Action::menu) || input.is_pressed(Action::back) || input.focus_lost)
    {
        open_pause(cues);
        return games::SceneExit::none;
    }
    if (input.is_pressed(Action::page_prev) && undo_)
    {
        game_.restore(*undo_);
        undo_.reset();
        if (moves_ > 0)
            --moves_;
        last_ = MoveResult{};
        last_.spawned_cell = 0xff;
        cues.push_back(audio::Cue::undo);
    }
    // Slides fire once per press: holding the stick or D-pad does not repeat.
    if (input.nav != ppz::Direction::none && !input.nav_repeat)
    {
        const Direction direction = input.nav == ppz::Direction::up     ? Direction::up
                                    : input.nav == ppz::Direction::down ? Direction::down
                                    : input.nav == ppz::Direction::left ? Direction::left
                                                                        : Direction::right;
        apply_move(direction, cues);
    }
    return games::SceneExit::none;
}

std::string G2048Scene::save()
{
    return encode_game(game_.snapshot(), moves_, undo_);
}

std::string G2048Scene::stats()
{
    return encode_stats(stats_);
}

bool G2048Scene::in_progress()
{
    return moves_ > 0 && !game_.snapshot().game_over;
}

void G2048Scene::draw_tile(gfx::DrawList &list, float cx, float cy, std::uint8_t exponent,
                           float scale) const
{
    const float size = kCell * scale;
    const gfx::Color color = gfx::Color::rgb(tile_color(exponent));
    list.rounded_rect({cx - size * 0.5f, cy - size * 0.5f + 3, size, size}, 12 * scale,
                      gfx::Color::rgb(0x000000, 0.10f));
    list.gradient_rect({cx - size * 0.5f, cy - size * 0.5f, size, size}, 12 * scale, color,
                       gfx::Color{color.r * 0.95f, color.g * 0.95f, color.b * 0.95f, 1.0f});
    char label[24];
    std::snprintf(label, sizeof(label), "%llu", static_cast<unsigned long long>(1ULL << exponent));
    const std::size_t digits = std::char_traits<char>::length(label);
    const float font = (digits <= 2   ? 64.0f
                        : digits == 3 ? 54.0f
                        : digits == 4 ? 44.0f
                                      : 34.0f) *
                       scale;
    list.text(*fonts_.semibold, fonts_.semibold_texture, label, cx, cy + font * 0.36f, font,
              exponent <= 2 ? gfx::Color::rgb(0x776e65) : gfx::Color::rgb(0xf9f6f2),
              gfx::Align::center);
}

void G2048Scene::draw(gfx::DrawList &list) const
{
    using gfx::Align;
    using gfx::Color;
    const Snapshot &state = game_.snapshot();
    list.gradient_rect({0, 0, 1920, 1080}, 0, ui::theme::kBackgroundTop,
                       ui::theme::kBackgroundBottom);
    list.shadow({560, 240, 800, 640}, 220, 260, Color::rgb(0xf2b179, 0.10f));

    list.text(*fonts_.semibold, fonts_.semibold_texture, "2048", ui::theme::kSafeMargin, 150, 96,
              ui::theme::kTextOnDark);
    list.text(*fonts_.regular, fonts_.regular_texture, "Slide the tiles. Merge equal numbers.",
              ui::theme::kSafeMargin, 200, 26, ui::theme::kTextOnDarkMuted);

    // Score, best and moves.
    const std::pair<const char *, std::string> boxes[] = {
        {"SCORE", format_number(state.score)},
        {"BEST", format_number(std::max(state.best, stats_.best))},
        {"MOVES", format_number(moves_)}};
    for (int i = 0; i < 3; ++i)
    {
        const float y = 272.0f + static_cast<float>(i) * 130.0f;
        list.rounded_rect({1360, y, 300, 110}, 20, Color::rgb(0xffffff, 0.08f));
        list.text(*fonts_.semibold, fonts_.semibold_texture, boxes[i].first, 1390, y + 40, 20,
                  ui::theme::kTextOnDarkMuted);
        list.text(*fonts_.semibold, fonts_.semibold_texture, boxes[i].second, 1390, y + 88, 40,
                  ui::theme::kTextOnDark);
    }
    if (gain_.running)
    {
        const float p = gain_.progress();
        char text[32];
        std::snprintf(text, sizeof(text), "+%llu", static_cast<unsigned long long>(gained_));
        list.text(*fonts_.semibold, fonts_.semibold_texture, text, 1640, 360.0f - 50.0f * p, 34,
                  ui::theme::kFocus.with_alpha(1.0f - p), Align::right);
    }

    float shake = 0.0f;
    if (shake_.running)
        shake = 7.0f * std::sin(shake_.progress() * 6.2831853f * 3.0f) * (1.0f - shake_.progress());
    list.push_transform(1.0f, 0, 0, shake, 0);
    list.shadow({kBoardX + 8, kBoardY + 22, kBoard - 16, kBoard}, 22, 34, ui::theme::kShadow);
    list.rounded_rect({kBoardX, kBoardY, kBoard, kBoard}, 22, Color::rgb(0xbbada0));
    for (int i = 0; i < 16; ++i)
        list.rounded_rect({cell_x(i) - kCell * 0.5f, cell_y(i) - kCell * 0.5f, kCell, kCell}, 12,
                          Color::rgb(0xcdc1b4));

    const bool sliding = slide_.running;
    const float slide = tween::cubic_out(slide_.progress());
    const float pop_time = pop_.elapsed - (last_.tile_count > 0 ? kSlideSeconds : 0.0f);
    const float pop = pop_.running ? std::clamp(pop_time / kPopSeconds, 0.0f, 1.0f) : 1.0f;
    std::array<bool, 16> moving{};
    if (sliding)
    {
        for (std::uint8_t t = 0; t < last_.tile_count; ++t)
            moving[last_.tiles[t].to] = true;
        for (std::uint8_t t = 0; t < last_.tile_count; ++t)
        {
            const TileTransition &tile = last_.tiles[t];
            const float x = tween::lerp(cell_x(tile.from), cell_x(tile.to), slide);
            const float y = tween::lerp(cell_y(tile.from), cell_y(tile.to), slide);
            draw_tile(list, x, y, tile.from_exponent, 1.0f);
        }
    }
    for (int i = 0; i < 16; ++i)
    {
        const std::uint8_t exponent = state.cells[static_cast<std::size_t>(i)];
        if (exponent == 0 || moving[static_cast<std::size_t>(i)])
            continue;
        float scale = 1.0f;
        if (i == last_.spawned_cell)
            scale = sliding ? 0.0f : tween::back_out(pop);
        else
        {
            for (std::uint8_t t = 0; t < last_.tile_count; ++t)
            {
                if (last_.tiles[t].to == i && last_.tiles[t].merged)
                    scale = 1.0f + 0.14f * std::sin(pop * 3.14159265f);
            }
        }
        if (scale > 0.01f)
            draw_tile(list, cell_x(i), cell_y(i), exponent, scale);
    }
    list.pop_transform();

    using ui::Button;
    const ui::Hint primary[] = {{Button::circle, "Pause"}};
    ui::draw_hints(list, fonts_, primary, 1, ui::theme::kSafeMargin, false);
    const ui::Hint secondary[] = {{Button::dpad, "Slide", Button::left_stick},
                                  {Button::l1, "Undo"},
                                  {Button::options, "Menu"}};
    ui::draw_hints(list, fonts_, secondary, 3, 1920.0f - ui::theme::kSafeMargin, true,
                   ui::theme::kTextOnDarkMuted);
    menu_.draw(list, fonts_);
}

} // namespace ppz::g2048
