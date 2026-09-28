// ProsperoPuzzles - Library screen: A-Z grid of games with favorites.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/library_scene.hpp"

#include "games/registry.hpp"
#include "platform/ps5/system.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ppz::ui
{

namespace
{

constexpr float kLeft = theme::kSafeMargin;
constexpr float kGridTop = 300.0f;    // screen y where content y = 0 is drawn
constexpr float kViewTop = 250.0f;    // clip top
constexpr float kViewBottom = 950.0f; // clip bottom
constexpr float kCardW = 254.0f;
constexpr float kCardH = 300.0f;
constexpr float kGap = 28.0f;
constexpr float kHeaderH = 64.0f;
constexpr float kRailX = 1856.0f;

std::uint32_t hash(const std::string &text)
{
    std::uint32_t h = 2166136261u;
    for (char c : text)
        h = (h ^ static_cast<unsigned char>(c)) * 16777619u;
    return h;
}

// Shortens text with an ellipsis to fit max_width.
std::string fit(const gfx::Font &font, const std::string &text, float size, float max_width)
{
    if (font.measure(text, size) <= max_width)
        return text;
    std::string out = text;
    while (!out.empty() && font.measure(out + "\xE2\x80\xA6", size) > max_width)
        out.pop_back();
    while (!out.empty() && out.back() == ' ')
        out.pop_back();
    return out + "\xE2\x80\xA6";
}

const char *filter_name(LibraryFilter filter)
{
    switch (filter)
    {
    case LibraryFilter::favorites:
        return "Favorites";
    case LibraryFilter::in_progress:
        return "In progress";
    default:
        return "All games";
    }
}

gfx::Color accent_of(const std::string &id)
{
    const games::GameInfo *game = games::find(id);
    return game != nullptr ? game->accent : gfx::Color::rgb(0x7b9cff);
}

} // namespace

LibraryScene::LibraryScene(Library &library) : library_(library)
{
    relayout();
    const gfx::Color accent = cells_.empty() ? gfx::Color{} : accent_of(focused_id());
    accent_r_.snap(accent.r);
    accent_g_.snap(accent.g);
    accent_b_.snap(accent.b);
}

void LibraryScene::relayout()
{
    cells_.clear();
    headers_.clear();
    const auto &items = library_.items();
    float y = 0.0f;
    int row = -1;
    int column = 0;
    for (std::size_t index = 0; index < items.size(); ++index)
    {
        if (index == 0 || items[index].section != items[index - 1].section)
        {
            if (index != 0)
                y += kCardH + kGap; // close the previous section's last row
            headers_.push_back({items[index].section, y, 0});
            y += kHeaderH;
            ++row;
            column = 0;
        }
        else if (column == kColumns)
        {
            y += kCardH + kGap;
            ++row;
            column = 0;
        }
        ++headers_.back().count;
        cells_.push_back({static_cast<int>(index), row, column,
                          kLeft + static_cast<float>(column) * (kCardW + kGap), y});
        ++column;
    }
    rows_ = row + 1;
    content_height_ = cells_.empty() ? 0.0f : cells_.back().y + kCardH;
    lift_.resize(items.size());
    focus_ = std::clamp(focus_, 0, std::max(0, static_cast<int>(items.size()) - 1));
}

int LibraryScene::cell_in_row(int row, int column) const
{
    int best = -1;
    for (const Cell &cell : cells_)
    {
        if (cell.row == row && cell.column <= column)
            best = cell.item;
    }
    return best;
}

std::string LibraryScene::focused_id() const
{
    const auto &items = library_.items();
    if (items.empty())
        return {};
    return library_.entries()[items[static_cast<std::size_t>(focus_)].entry].id;
}

void LibraryScene::refresh()
{
    const std::string keep = focused_id();
    relayout();
    focus_game(keep);
}

void LibraryScene::focus_game(const std::string &id)
{
    const int index = library_.index_of(id);
    if (index >= 0)
        focus_ = index;
}

void LibraryScene::move_focus(int item, std::vector<audio::Cue> &cues, audio::Cue cue)
{
    if (item < 0 || item >= static_cast<int>(cells_.size()) || item == focus_)
        return;
    focus_ = item;
    cues.push_back(cue);
    const Cell &cell = cells_[static_cast<std::size_t>(item)];
    sys::log("[PPZ] library focus=%s index=%d row=%d/%d y=%.0f scroll=%.0f->%.0f",
             focused_id().c_str(), item, cell.row, rows_, static_cast<double>(cell.y),
             static_cast<double>(scroll_.value), static_cast<double>(scroll_.target));
}

LibraryRequest LibraryScene::update(const InputFrame &input, float dt,
                                    std::vector<audio::Cue> &cues)
{
    LibraryRequest request;
    time_ += dt;
    const auto &items = library_.items();

    if (!cells_.empty())
    {
        const Cell &focused = cells_[static_cast<std::size_t>(focus_)];
        switch (input.nav)
        {
        case Direction::left:
            move_focus(focus_ - 1, cues);
            break;
        case Direction::right:
            move_focus(focus_ + 1, cues);
            break;
        case Direction::up:
            if (focused.row > 0)
                move_focus(cell_in_row(focused.row - 1, focused.column), cues);
            break;
        case Direction::down:
            if (focused.row + 1 < rows_)
                move_focus(cell_in_row(focused.row + 1, focused.column), cues);
            break;
        default:
            break;
        }
        // Letter jumps wrap around so L2/R2 never dead-end at either end.
        const int count = static_cast<int>(library_.items().size());
        if (input.is_pressed(Action::jump_next) && count > 0)
        {
            int target = library_.next_letter(focus_);
            if (target == focus_)
                target = 0;
            move_focus(target, cues, audio::Cue::ui_tab);
        }
        if (input.is_pressed(Action::jump_prev) && count > 0)
        {
            int target = library_.previous_letter(focus_);
            if (target == focus_)
            {
                const int last = count - 1;
                const int start = library_.previous_letter(last);
                target = library_.next_letter(start) == start ? start : last;
            }
            move_focus(target, cues, audio::Cue::ui_tab);
        }
    }

    if (input.is_pressed(Action::page_next) || input.is_pressed(Action::page_prev))
    {
        const std::string keep = focused_id();
        const int step = input.is_pressed(Action::page_next) ? 1 : 2;
        library_.set_filter(
            static_cast<LibraryFilter>((static_cast<int>(library_.filter()) + step) % 3));
        focus_ = std::max(0, library_.index_of(keep));
        relayout();
        scroll_.target = 0.0f;
        cues.push_back(audio::Cue::ui_tab);
    }

    if (!items.empty())
    {
        const std::string id = focused_id();
        if (input.is_pressed(Action::west))
        {
            const bool now_favorite = library_.toggle_favorite(id);
            relayout();
            focus_game(id);
            burst_item_ = focus_;
            favorite_burst_.start(now_favorite ? 0.55f : 0.3f);
            cues.push_back(now_favorite ? audio::Cue::ui_favorite_on : audio::Cue::ui_favorite_off);
            request = {LibraryRequest::Kind::favorites_changed, id};
        }
        else if (input.is_pressed(Action::confirm))
        {
            cues.push_back(audio::Cue::ui_launch);
            request = {LibraryRequest::Kind::launch, id};
        }
        else if (input.is_pressed(Action::north))
        {
            cues.push_back(audio::Cue::ui_select);
            request = {LibraryRequest::Kind::details, id};
        }
    }
    if (input.is_pressed(Action::menu))
    {
        cues.push_back(audio::Cue::ui_pause_open);
        request = {LibraryRequest::Kind::settings, {}};
    }

    // Motion: scroll keeps the focused row in view; focus lifts spring in.
    const float omega = reduced_motion ? 60.0f : 12.0f;
    if (!cells_.empty())
    {
        const float visible = kViewBottom - kGridTop;
        const float max_scroll = std::max(0.0f, content_height_ - visible + 20.0f);
        const float row_y = cells_[static_cast<std::size_t>(focus_)].y;
        scroll_.target = std::clamp(row_y - 150.0f, 0.0f, max_scroll);
        const gfx::Color accent = accent_of(focused_id());
        accent_r_.target = accent.r;
        accent_g_.target = accent.g;
        accent_b_.target = accent.b;
    }
    scroll_.update(dt, reduced_motion ? 60.0f : 9.0f);
    accent_r_.update(dt, 5.0f);
    accent_g_.update(dt, 5.0f);
    accent_b_.update(dt, 5.0f);
    for (std::size_t i = 0; i < lift_.size(); ++i)
    {
        lift_[i].target = static_cast<int>(i) == focus_ ? 1.0f : 0.0f;
        lift_[i].update(dt, omega);
    }
    favorite_burst_.update(dt);
    return request;
}

void LibraryScene::draw_card(gfx::DrawList &list, const Fonts &fonts, const Cell &cell) const
{
    using gfx::Color;
    const auto &items = library_.items();
    const LibraryItem &item = items[static_cast<std::size_t>(cell.item)];
    const LibraryEntry &entry = library_.entries()[item.entry];
    const float lift = lift_[static_cast<std::size_t>(cell.item)].value;
    const Color accent = accent_of(entry.id);
    const float x = cell.x;
    const float y = cell.y;

    list.push_transform(1.0f + 0.06f * lift, x + kCardW * 0.5f, y + kCardH * 0.5f, 0.0f,
                        -10.0f * lift);
    list.shadow({x + 8, y + 16 + 8 * lift, kCardW - 16, kCardH - 4}, theme::kRadiusCard,
                18.0f + 18.0f * lift, theme::kShadow.with_alpha(0.55f + 0.45f * lift));
    list.gradient_rect({x, y, kCardW, kCardH}, theme::kRadiusCard, theme::kPaper,
                       theme::kPaperShade);

    // Accent panel with a procedural mini-board unique to the game.
    const gfx::Rect panel{x + 18, y + 18, kCardW - 36, 150};
    list.gradient_rect(panel, 18, accent,
                       Color{accent.r * 0.82f, accent.g * 0.82f, accent.b * 0.82f, 1.0f});
    std::uint32_t texture = 0;
    float thumb_w = 0.0f;
    float thumb_h = 0.0f;
    if (thumbnails_ != nullptr && thumbnails_->thumbnail(entry.id, &texture, &thumb_w, &thumb_h))
    {
        // A real board, centred on a soft paper mat inside the accent panel.
        const float tx = panel.x + (panel.w - thumb_w) * 0.5f;
        const float ty = panel.y + (panel.h - thumb_h) * 0.5f;
        list.rounded_rect({tx - 5, ty - 5, thumb_w + 10, thumb_h + 10}, 8,
                          Color::rgb(0xffffff, 0.55f));
        list.image(texture, {tx, ty, thumb_w, thumb_h}, {0.0f, 1.0f, 1.0f, -1.0f},
                   Color{1, 1, 1, 1});
    }
    const std::uint32_t seed = hash(entry.id);
    const int columns = texture != 0 ? 0 : 4 + static_cast<int>(seed % 2);
    const int rows = 3;
    const float gap = 7.0f;
    const float tile_w =
        (panel.w - 36.0f - gap * static_cast<float>(columns - 1)) / static_cast<float>(columns);
    const float tile_h =
        (panel.h - 30.0f - gap * static_cast<float>(rows - 1)) / static_cast<float>(rows);
    for (int r = 0; r < rows && columns > 0; ++r)
    {
        for (int c = 0; c < columns; ++c)
        {
            const std::uint32_t bits = (seed >> ((r * columns + c) % 29)) & 3u;
            const float shimmer =
                0.04f * std::sin(time_ * 2.0f + static_cast<float>(r * 3 + c)) * lift;
            const float alpha = 0.22f + 0.2f * static_cast<float>(bits) + shimmer;
            list.rounded_rect({panel.x + 18 + static_cast<float>(c) * (tile_w + gap),
                               panel.y + 15 + static_cast<float>(r) * (tile_h + gap), tile_w,
                               tile_h},
                              7, Color::rgb(0xffffff, alpha));
        }
    }

    // Beaten at least once: a gold medal with a check on the panel's corner.
    if (library_.is_completed(entry.id))
    {
        const float mx = panel.x + panel.w - 6;
        const float my = panel.y + 6;
        list.circle(mx + 1, my + 3, 20, Color::rgb(0x000000, 0.22f));
        list.circle(mx, my, 20, Color::rgb(0xfffefa));
        list.circle(mx, my, 16, Color::rgb(0xf0c555));
        list.line(mx - 7, my + 1, mx - 2, my + 6, 3.5f, Color::rgb(0x6b4a12));
        list.line(mx - 2, my + 6, mx + 8, my - 5, 3.5f, Color::rgb(0x6b4a12));
    }

    const float text_w = kCardW - 36.0f;
    list.text(*fonts.semibold, fonts.semibold_texture,
              fit(*fonts.semibold, entry.display_name, 30, text_w - 34), x + 18, y + 214, 30,
              theme::kInk);
    list.text(*fonts.regular, fonts.regular_texture, fit(*fonts.regular, entry.tagline, 19, text_w),
              x + 18, y + 246, 19, theme::kInkMuted);

    const bool favorite = library_.is_favorite(entry.id);
    float star_scale = 1.0f;
    if (cell.item == burst_item_ && favorite_burst_.running)
        star_scale = favorite ? tween::back_out(favorite_burst_.progress()) * 1.0f
                              : 1.0f - 0.3f * (1.0f - favorite_burst_.progress());
    const float sx = x + kCardW - 34;
    const float sy = y + 202;
    if (favorite)
    {
        list.star(sx, sy, 15.0f * star_scale, theme::kFavorite);
        if (cell.item == burst_item_ && favorite_burst_.running)
        {
            const float p = favorite_burst_.progress();
            list.ring(sx, sy, 14.0f + 26.0f * tween::expo_out(p), 3.0f * (1.0f - p),
                      theme::kFavorite.with_alpha(1.0f - p));
        }
    }
    else
    {
        list.star(sx, sy, 14.0f * star_scale, theme::kInkMuted.with_alpha(0.35f + 0.4f * lift),
                  2.0f);
    }
    if (library_.is_in_progress(entry.id))
    {
        list.rounded_rect({x + 18, y + 262, 112, 26}, 13, accent.with_alpha(0.25f));
        list.text(*fonts.semibold, fonts.semibold_texture, "In progress", x + 74, y + 281, 15,
                  theme::kInk, gfx::Align::center);
    }
    if (lift > 0.01f)
        list.bordered_rect({x - 7, y - 7, kCardW + 14, kCardH + 14}, theme::kRadiusCard + 7,
                           theme::kFocus.with_alpha(0.0f), 4.0f, theme::kFocus.with_alpha(lift));
    list.pop_transform();
}

void LibraryScene::draw(gfx::DrawList &list, const Fonts &fonts) const
{
    using gfx::Align;
    using gfx::Color;
    const Color tint{accent_r_.value, accent_g_.value, accent_b_.value, 1.0f};

    // Background: deep gradient with slow drifting glows tinted by the focus.
    list.gradient_rect({0, 0, 1920, 1080}, 0, theme::kBackgroundTop, theme::kBackgroundBottom);
    for (int i = 0; i < 7; ++i)
    {
        const float phase = time_ * 0.12f + static_cast<float>(i) * 1.9f;
        const float gx = 120.0f + static_cast<float>(i) * 280.0f + 80.0f * std::sin(phase);
        const float gy =
            180.0f + static_cast<float>(i % 3) * 300.0f + 60.0f * std::cos(phase * 1.2f);
        list.shadow({gx, gy, 220, 220}, 60, 90, tint.with_alpha(0.07f));
    }

    // Header.
    list.text(*fonts.semibold, fonts.semibold_texture, "ProsperoPuzzles", kLeft, 150,
              theme::kTextDisplay, theme::kTextOnDark);
    char count[64];
    std::snprintf(count, sizeof(count), "%zu games", library_.entries().size());
    list.text(*fonts.regular, fonts.regular_texture, count, kLeft, 200, theme::kTextBody,
              theme::kTextOnDarkMuted);

    // Filter chips (L1 / R1).
    float chip_x = 1920.0f - theme::kSafeMargin;
    const LibraryFilter filters[] = {LibraryFilter::in_progress, LibraryFilter::favorites,
                                     LibraryFilter::all};
    for (LibraryFilter filter : filters)
    {
        const char *name = filter_name(filter);
        const float w = fonts.semibold->measure(name, 22) + 40.0f;
        chip_x -= w;
        const bool active = filter == library_.filter();
        list.rounded_rect({chip_x, 112, w, 46}, 23,
                          active ? theme::kTextOnDark : Color::rgb(0xffffff, 0.08f));
        list.text(*fonts.semibold, fonts.semibold_texture, name, chip_x + w * 0.5f, 143, 22,
                  active ? theme::kInk : theme::kTextOnDarkMuted, Align::center);
        chip_x -= 12.0f;
    }
    draw_button(list, fonts, Button::l1, chip_x - 4 - button_width(Button::l1, 34), 135, 34);
    draw_button(list, fonts, Button::r1, 1920.0f - theme::kSafeMargin + 12, 135, 34);

    // Grid (clipped and scrolled).
    list.push_clip({0, kViewTop, 1920, kViewBottom - kViewTop});
    list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, kGridTop - scroll_.value);
    for (const Header &header : headers_)
    {
        const bool favorites = header.section == Section::favorites;
        float hx = kLeft;
        if (favorites)
        {
            list.star(hx + 12, header.y + 26, 12, theme::kFavorite);
            hx += 34;
        }
        char label[48];
        std::snprintf(label, sizeof(label), "%s  %d",
                      favorites ? "Favorites" : filter_name(library_.filter()), header.count);
        list.text(*fonts.semibold, fonts.semibold_texture, label, hx, header.y + 36, 26,
                  theme::kTextOnDark);
    }
    if (cells_.empty())
    {
        const char *empty = library_.filter() == LibraryFilter::favorites
                                ? "No favorites yet. Press Square on a game to add it."
                                : "Nothing in progress yet.";
        list.text(*fonts.regular, fonts.regular_texture, empty, 960, 200, theme::kTextHeading,
                  theme::kTextOnDarkMuted, Align::center);
    }
    // Unfocused cards first so the focused one draws on top.
    for (const Cell &cell : cells_)
    {
        const float screen_y = cell.y + kGridTop - scroll_.value;
        if (cell.item != focus_ && screen_y < kViewBottom + 40 && screen_y + kCardH > kViewTop - 40)
            draw_card(list, fonts, cell);
    }
    if (!cells_.empty())
        draw_card(list, fonts, cells_[static_cast<std::size_t>(focus_)]);
    list.pop_transform();
    list.pop_clip();
    // Soft fades where the scrolling grid meets the header and the hint bar.
    const auto band = [&](float y0, float y1)
    {
        const float t0 = y0 / 1080.0f;
        const float t1 = y1 / 1080.0f;
        const auto at = [&](float t)
        {
            return Color{tween::lerp(theme::kBackgroundTop.r, theme::kBackgroundBottom.r, t),
                         tween::lerp(theme::kBackgroundTop.g, theme::kBackgroundBottom.g, t),
                         tween::lerp(theme::kBackgroundTop.b, theme::kBackgroundBottom.b, t), 1.0f};
        };
        return std::make_pair(at(t0), at(t1));
    };
    const auto top = band(kViewTop, kViewTop + 40.0f);
    list.gradient_rect({0, kViewTop, 1920, 40}, 0, top.first, top.second.with_alpha(0.0f));
    const auto bottom = band(kViewBottom - 50.0f, kViewBottom);
    list.gradient_rect({0, kViewBottom - 50.0f, 1920, 50}, 0, bottom.first.with_alpha(0.0f),
                       bottom.second);

    // A-Z rail: letters present in the list, the focused one highlighted.
    if (!cells_.empty())
    {
        const auto &items = library_.items();
        const char current = Library::initial(
            library_.entries()[items[static_cast<std::size_t>(focus_)].entry].display_name);
        const char *letters = "#ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        for (int i = 0; letters[i] != '\0'; ++i)
        {
            bool present = false;
            for (const LibraryItem &item : items)
                present = present || Library::initial(
                                         library_.entries()[item.entry].display_name) == letters[i];
            const char text[2] = {letters[i], '\0'};
            const float ly = 290.0f + static_cast<float>(i) * 24.5f;
            if (letters[i] == current)
                list.circle(kRailX, ly - 7, 13, theme::kFocus);
            list.text(*fonts.semibold, fonts.semibold_texture, text, kRailX, ly, 16,
                      letters[i] == current ? theme::kInk
                      : present             ? theme::kTextOnDark
                                            : Color::rgb(0xffffff, 0.18f),
                      Align::center);
        }
    }

    // Controls hint bar.
    const bool favorite = !cells_.empty() && library_.is_favorite(focused_id());
    const Hint settings[] = {{Button::options, "Settings"}};
    const float settings_width = draw_hints(list, fonts, settings, 1, 1920.0f - theme::kSafeMargin,
                                            true, theme::kTextOnDarkMuted);
    const Hint hints[] = {{Button::cross, "Play"},
                          {Button::square, favorite ? "Unfavorite" : "Favorite"},
                          {Button::triangle, "Details"}};
    draw_hints(list, fonts, hints, 3, 1920.0f - theme::kSafeMargin - settings_width - 44.0f, true);
    const Hint jump[] = {{Button::l2, "Jump letter", Button::r2}};
    draw_hints(list, fonts, jump, 1, kLeft, false, theme::kTextOnDarkMuted);
}

} // namespace ppz::ui
