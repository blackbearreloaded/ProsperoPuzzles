// ProsperoPuzzles - Shared play screen for the native logic puzzles.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/kit/puzzle_scene.hpp"

#include "core/bytes.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace ppz::kit
{

namespace
{

constexpr std::uint8_t kSaveVersion = 1;
constexpr std::size_t kUndoLimit = 200;
constexpr std::size_t kSavedUndo = 40; // undo steps kept in a save
constexpr float kPanelX = 1380.0f;

} // namespace

gfx::Color look::wash(int index, float amount)
{
    const gfx::Color h = hue(index);
    const gfx::Color base = kBoard;
    return {base.r + (h.r - base.r) * amount, base.g + (h.g - base.g) * amount,
            base.b + (h.b - base.b) * amount, 1.0f};
}

Grid fit_grid(int cols, int rows, float gap_fraction, gfx::Rect area)
{
    Grid g;
    g.cols = std::max(1, cols);
    g.rows = std::max(1, rows);
    const float padding = 22.0f;
    const float units_w =
        static_cast<float>(g.cols) + gap_fraction * static_cast<float>(g.cols - 1);
    const float units_h =
        static_cast<float>(g.rows) + gap_fraction * static_cast<float>(g.rows - 1);
    g.cell = std::min((area.w - 2 * padding) / units_w, (area.h - 2 * padding) / units_h);
    g.cell = std::min(g.cell, 132.0f); // small boards stay tile-sized, not huge
    g.gap = g.cell * gap_fraction;
    const float w = g.cell * units_w;
    const float h = g.cell * units_h;
    g.x = area.x + (area.w - w) * 0.5f;
    g.y = area.y + (area.h - h) * 0.5f;
    g.card = {g.x - padding, g.y - padding, w + 2 * padding, h + 2 * padding};
    return g;
}

void draw_tile(gfx::DrawList &list, const gfx::Rect &r, gfx::Color fill, float radius_fraction)
{
    const float radius = std::min(r.w, r.h) * radius_fraction;
    list.rounded_rect({r.x, r.y + std::max(2.0f, r.h * 0.04f), r.w, r.h}, radius,
                      look::kTileShadow);
    list.rounded_rect(r, radius, fill);
}

std::string format_time(std::uint32_t seconds)
{
    char text[24];
    std::snprintf(text, sizeof(text), "%u:%02u", seconds / 60, seconds % 60);
    return text;
}

std::uint64_t fresh_seed()
{
    static std::uint64_t counter = 0x5eed5eedULL;
    counter += 0x9e3779b97f4a7c15ULL;
    return counter ^
           static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
}

PuzzleScene::PuzzleScene(const ui::Fonts &fonts, Info info)
    : size_(info.default_size), info_(std::move(info)), fonts_(fonts)
{
}

Grid PuzzleScene::grid() const
{
    return fit_grid(grid_cols(), grid_rows());
}

gfx::Rect PuzzleScene::cursor_rect(float col, float row) const
{
    return grid().cell_rect(col, row);
}

void PuzzleScene::snap_ring()
{
    const gfx::Rect r =
        cursor_rect(static_cast<float>(cursor_col_), static_cast<float>(cursor_row_));
    ring_x_.snap(r.x);
    ring_y_.snap(r.y);
    ring_w_.snap(r.w);
    ring_h_.snap(r.h);
}

void PuzzleScene::new_puzzle(std::uint64_t seed, int size)
{
    size_ = std::clamp(size, 0, 2);
    generate(seed, size_);
    undo_.clear();
    redo_.clear();
    moves_ = 0;
    seconds_ = 0.0f;
    solved_ = false;
    counted_ = false;
    solved_timer_ = {};
    intro_.start(0.5f);
    set_cursor(grid_cols() / 2, grid_rows() / 2);
    snap_ring();
}

void PuzzleScene::start(const std::string &save, const std::string &stats)
{
    bytes::Reader s(stats);
    if (s.get<std::uint8_t>() == kSaveVersion)
    {
        PuzzleStats loaded;
        loaded.played = s.get<std::uint32_t>();
        loaded.solved = s.get<std::uint32_t>();
        for (auto &best : loaded.best_seconds)
            best = s.get<std::uint32_t>();
        const int last_size = s.get<std::uint8_t>();
        if (s.finished())
        {
            stats_ = loaded;
            size_ = std::clamp(last_size, 0, 2);
        }
    }

    bytes::Reader r(save);
    bool restored = false;
    if (!save.empty() && r.get<std::uint8_t>() == kSaveVersion)
    {
        const int size = r.get<std::uint8_t>();
        const std::uint32_t seconds = r.get<std::uint32_t>();
        const std::uint32_t moves = r.get<std::uint32_t>();
        auto read_blob = [&r]()
        {
            const std::uint32_t length = r.get<std::uint32_t>();
            std::string blob;
            for (std::uint32_t i = 0; i < length && r.ok(); ++i)
                blob.push_back(static_cast<char>(r.get<std::uint8_t>()));
            return blob;
        };
        const std::string state = read_blob();
        const std::uint16_t steps = r.get<std::uint16_t>();
        std::vector<std::string> history;
        for (std::uint16_t i = 0; i < steps && r.ok(); ++i)
            history.push_back(read_blob());
        const int previous_size = size_;
        size_ = std::clamp(size, 0, 2);
        if (r.finished() && deserialize(state))
        {
            seconds_ = static_cast<float>(seconds);
            moves_ = moves;
            undo_ = std::move(history);
            counted_ = true;
            solved_ = solved();
            restored = true;
            set_cursor(grid_cols() / 2, grid_rows() / 2);
            snap_ring();
            intro_.start(0.5f);
        }
        else
        {
            size_ = previous_size;
        }
    }
    if (!restored)
        new_puzzle(fresh_seed(), size_);
}

std::string PuzzleScene::save()
{
    bytes::Writer w;
    w.put(kSaveVersion);
    w.put(static_cast<std::uint8_t>(size_));
    w.put(static_cast<std::uint32_t>(seconds_));
    w.put(moves_);
    auto put_blob = [&w](const std::string &blob)
    {
        w.put(static_cast<std::uint32_t>(blob.size()));
        for (char c : blob)
            w.put(static_cast<std::uint8_t>(c));
    };
    put_blob(serialize());
    const std::size_t first = undo_.size() > kSavedUndo ? undo_.size() - kSavedUndo : 0;
    w.put(static_cast<std::uint16_t>(undo_.size() - first));
    for (std::size_t i = first; i < undo_.size(); ++i)
        put_blob(undo_[i]);
    return w.data();
}

std::string PuzzleScene::stats()
{
    bytes::Writer w;
    w.put(kSaveVersion);
    w.put(stats_.played);
    w.put(stats_.solved);
    for (std::uint32_t best : stats_.best_seconds)
        w.put(best);
    w.put(static_cast<std::uint8_t>(size_));
    return w.data();
}

bool PuzzleScene::in_progress()
{
    return moves_ > 0 && !solved_;
}

void PuzzleScene::remember()
{
    undo_.push_back(serialize());
    if (undo_.size() > kUndoLimit)
        undo_.erase(undo_.begin());
    redo_.clear();
    ++moves_;
    if (!counted_)
    {
        counted_ = true;
        ++stats_.played;
    }
}

void PuzzleScene::reject(std::vector<audio::Cue> &cues)
{
    cues.push_back(audio::Cue::invalid);
    shake_.start(0.16f);
}

void PuzzleScene::set_cursor(int col, int row)
{
    cursor_col_ = std::clamp(col, 0, std::max(0, grid_cols() - 1));
    cursor_row_ = std::clamp(row, 0, std::max(0, grid_rows() - 1));
}

void PuzzleScene::undo(std::vector<audio::Cue> &cues)
{
    if (undo_.empty() || solved_)
    {
        cues.push_back(audio::Cue::invalid);
        return;
    }
    redo_.push_back(serialize());
    deserialize(undo_.back());
    undo_.pop_back();
    set_cursor(cursor_col_, cursor_row_);
    cues.push_back(audio::Cue::undo);
}

void PuzzleScene::redo(std::vector<audio::Cue> &cues)
{
    if (redo_.empty() || solved_)
    {
        cues.push_back(audio::Cue::invalid);
        return;
    }
    undo_.push_back(serialize());
    deserialize(redo_.back());
    redo_.pop_back();
    set_cursor(cursor_col_, cursor_row_);
    cues.push_back(audio::Cue::redo);
}

void PuzzleScene::move_cursor(Direction direction, std::vector<audio::Cue> &cues)
{
    int col = cursor_col_;
    int row = cursor_row_;
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
    if (col < 0 || row < 0 || col >= grid_cols() || row >= grid_rows())
        return;
    set_cursor(col, row);
    cues.push_back(audio::Cue::cursor);
}

void PuzzleScene::open_pause()
{
    std::string size = "Size: ";
    size += info_.sizes[static_cast<std::size_t>(size_)];
    menu_.open("Paused",
               {{"Resume", kResume},
                {"New puzzle", kNewPuzzle},
                {"Restart", kRestart, moves_ > 0 && !solved_},
                {size, kSize},
                {"How to play", kHowTo},
                {"Back to library", kLibrary}},
               info_.title);
}

void PuzzleScene::open_solved()
{
    const auto seconds = static_cast<std::uint32_t>(seconds_);
    const std::uint32_t best = stats_.best_seconds[static_cast<std::size_t>(size_)];
    std::string subtitle = "Time " + format_time(seconds);
    if (best == seconds)
        subtitle += "  \xC2\xB7  New best!";
    else if (best > 0)
        subtitle += "  \xC2\xB7  Best " + format_time(best);
    menu_.open("Solved!", {{"New puzzle", kNewPuzzle}, {"Back to library", kLibrary}}, subtitle);
}

void PuzzleScene::check_solved(std::vector<audio::Cue> &cues)
{
    if (solved_ || !solved())
        return;
    solved_ = true;
    solved_timer_.start(1.2f);
    ++stats_.solved;
    auto &best = stats_.best_seconds[static_cast<std::size_t>(size_)];
    const auto seconds = std::max<std::uint32_t>(1, static_cast<std::uint32_t>(seconds_));
    if (best == 0 || seconds < best)
        best = seconds;
    seconds_ = static_cast<float>(seconds);
    cues.push_back(audio::Cue::complete);
}

games::SceneExit PuzzleScene::update(const InputFrame &input, float dt,
                                     std::vector<audio::Cue> &cues)
{
    time_ += dt;
    shake_.update(dt);
    intro_.update(dt);
    const bool was_running = solved_timer_.running;
    solved_timer_.update(dt);
    animate(dt);
    const gfx::Rect r =
        cursor_rect(static_cast<float>(cursor_col_), static_cast<float>(cursor_row_));
    ring_x_.target = r.x;
    ring_y_.target = r.y;
    ring_w_.target = r.w;
    ring_h_.target = r.h;
    for (tween::Spring *spring : {&ring_x_, &ring_y_, &ring_w_, &ring_h_})
        spring->update(dt, 24.0f);
    // The solved menu appears once the flourish has played.
    if (was_running && !solved_timer_.running && solved_ && !menu_.is_open())
        open_solved();

    if (menu_.is_open())
    {
        const int choice = menu_.update(input, dt, cues, !solved_);
        if (choice >= kSizeSmall && choice < kSizeSmall + 3)
        {
            new_puzzle(fresh_seed(), choice - kSizeSmall);
            cues.push_back(audio::Cue::new_game);
            return games::SceneExit::none;
        }
        switch (choice)
        {
        case kNewPuzzle:
            new_puzzle(fresh_seed(), size_);
            cues.push_back(audio::Cue::new_game);
            break;
        case kRestart:
            remember();
            restart();
            cues.push_back(audio::Cue::restart);
            break;
        case kSize:
        {
            std::vector<ui::Menu::Item> items;
            for (int i = 0; i < 3; ++i)
                items.push_back({info_.sizes[static_cast<std::size_t>(i)], kSizeSmall + i});
            menu_.open("Board size", std::move(items), "Starts a new puzzle");
            break;
        }
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

    if (solved_)
    {
        // Between the flourish and the menu: any button skips ahead.
        if (!solved_timer_.running && input.pressed != 0)
            open_solved();
        return games::SceneExit::none;
    }
    seconds_ += dt;

    if (input.is_pressed(Action::menu) || input.focus_lost ||
        (input.is_pressed(Action::back) && !wants_back()))
    {
        open_pause();
        if (!input.focus_lost)
            cues.push_back(audio::Cue::ui_pause_open);
        return games::SceneExit::none;
    }
    if (input.is_pressed(Action::page_prev))
    {
        undo(cues);
        return games::SceneExit::none;
    }
    if (input.is_pressed(Action::page_next))
    {
        redo(cues);
        return games::SceneExit::none;
    }
    if (kit_moves_cursor() && input.nav != Direction::none)
        move_cursor(input.nav, cues);
    play(input, cues);
    check_solved(cues);
    return games::SceneExit::none;
}

void PuzzleScene::draw_preview(gfx::DrawList &list) const
{
    const Grid g = grid();
    list.rounded_rect(g.card, 22, look::kBoard);
    draw_board(list);
}

void PuzzleScene::draw(gfx::DrawList &list) const
{
    using gfx::Align;
    using gfx::Color;
    list.gradient_rect({0, 0, 1920, 1080}, 0, ui::theme::kBackgroundTop,
                       ui::theme::kBackgroundBottom);
    list.shadow({560, 240, 800, 640}, 220, 260, Color::rgb(0x7775c5, 0.12f));

    // Title column.
    list.text(*fonts_.semibold, fonts_.semibold_texture, info_.title, ui::theme::kSafeMargin, 150,
              88, ui::theme::kTextOnDark);
    list.text(*fonts_.regular, fonts_.regular_texture, info_.subtitle, ui::theme::kSafeMargin, 200,
              26, ui::theme::kTextOnDarkMuted);
    const char *size_name = info_.sizes[static_cast<std::size_t>(size_)];
    const float chip = fonts_.semibold->measure(size_name, 20) + 40.0f;
    list.rounded_rect({ui::theme::kSafeMargin, 236, chip, 40}, 20, Color::rgb(0xffffff, 0.08f));
    list.text(*fonts_.semibold, fonts_.semibold_texture, size_name,
              ui::theme::kSafeMargin + chip * 0.5f, 263, 20, ui::theme::kTextOnDarkMuted,
              Align::center);

    // Stats panel.
    const std::uint32_t best = stats_.best_seconds[static_cast<std::size_t>(size_)];
    std::vector<std::pair<std::string, std::string>> boxes = {
        {"TIME", format_time(static_cast<std::uint32_t>(seconds_))},
        {"BEST", best > 0 ? format_time(best) : "\xE2\x80\x94"},
        {"MOVES", std::to_string(moves_)}};
    std::string label;
    std::string value;
    if (extra_stat(&label, &value))
        boxes.emplace_back(label, value);
    for (std::size_t i = 0; i < boxes.size(); ++i)
    {
        const float y = 252.0f + static_cast<float>(i) * 130.0f;
        list.rounded_rect({kPanelX, y, 300, 110}, 20, Color::rgb(0xffffff, 0.08f));
        list.text(*fonts_.semibold, fonts_.semibold_texture, boxes[i].first, kPanelX + 30, y + 40,
                  20, ui::theme::kTextOnDarkMuted);
        list.text(*fonts_.semibold, fonts_.semibold_texture, boxes[i].second, kPanelX + 30, y + 88,
                  40, ui::theme::kTextOnDark);
    }

    // Board, with a shake for rejected moves and a pop-in when a puzzle starts.
    float shake = 0.0f;
    if (shake_.running)
        shake = 7.0f * std::sin(shake_.progress() * 6.2831853f * 3.0f) * (1.0f - shake_.progress());
    const float intro = intro_.running ? tween::back_out(intro_.progress()) : 1.0f;
    const Grid g = grid();
    const float cx = g.card.x + g.card.w * 0.5f;
    const float cy = g.card.y + g.card.h * 0.5f;
    list.push_transform(0.94f + 0.06f * intro, cx, cy, shake, 0);
    list.push_opacity(std::min(1.0f, 0.2f + intro));
    list.shadow({g.card.x + 8, g.card.y + 22, g.card.w - 16, g.card.h}, 22, 34, ui::theme::kShadow);
    list.rounded_rect(g.card, 22, look::kBoard);
    draw_board(list);
    if (show_cursor() && !solved_)
    {
        const float pad = std::max(5.0f, ring_w_.value * 0.06f);
        list.bordered_rect({ring_x_.value - pad, ring_y_.value - pad, ring_w_.value + 2 * pad,
                            ring_h_.value + 2 * pad},
                           std::min(ring_w_.value, ring_h_.value) * 0.2f + pad, Color{0, 0, 0, 0},
                           std::max(3.0f, g.cell * 0.05f), ui::theme::kFocus);
    }
    draw_overlay(list);
    list.pop_opacity();
    list.pop_transform();

    // Hint bar.
    const std::vector<ui::Hint> primary = hints();
    ui::draw_hints(list, fonts_, primary.data(), static_cast<int>(primary.size()),
                   ui::theme::kSafeMargin, false);
    const ui::Hint secondary[] = {
        {ui::Button::l1, "Undo"}, {ui::Button::r1, "Redo"}, {ui::Button::options, "Menu"}};
    ui::draw_hints(list, fonts_, secondary, 3, 1920.0f - ui::theme::kSafeMargin, true,
                   ui::theme::kTextOnDarkMuted);
    menu_.draw(list, fonts_);
}

} // namespace ppz::kit
