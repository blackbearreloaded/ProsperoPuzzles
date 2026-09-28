// ProsperoPuzzles - Play screen for one Tatham puzzle.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/sgt/sgt_scene.hpp"

#include "games/registry.hpp"

#include <algorithm>
#include <cmath>

namespace ppz::sgt
{

namespace
{

constexpr gfx::Rect kBoardArea{96.0f, 210.0f, 1728.0f, 730.0f};

gfx::Color accent_of(const GameEntry &entry)
{
    const games::GameInfo *game = games::find(entry.id);
    return game != nullptr ? game->accent : gfx::Color::rgb(0x7b9cff);
}

int cursor_button(Direction direction)
{
    switch (direction)
    {
    case Direction::up:
        return CURSOR_UP;
    case Direction::down:
        return CURSOR_DOWN;
    case Direction::left:
        return CURSOR_LEFT;
    case Direction::right:
        return CURSOR_RIGHT;
    default:
        return 0;
    }
}

} // namespace

SgtScene::SgtScene(const GameEntry &entry, gfx::GlBatch &batch, const ui::Fonts &fonts,
                   float surface_scale)
    : entry_(entry), fonts_(fonts), surface_scale_(surface_scale),
      renderer_(std::make_unique<CanvasRenderer>(batch, fonts)), id_(entry.id)
{
    // The session is created after the renderer so it is destroyed first:
    // its teardown frees blitters through the renderer.
    session_ = std::make_unique<Session>(entry);
    session_->set_renderer(renderer_.get());
}

void SgtScene::start(const std::string &save, const std::string &stats)
{
    (void)stats;
    if (save.empty() || !session_->deserialise(save).empty())
        session_->new_game();
    layout();
    last_status_ = session_->status();
    assisted_ = false;
}

void SgtScene::layout()
{
    int width = 0;
    int height = 0;
    session_->resize(static_cast<int>(kBoardArea.w * surface_scale_),
                     static_cast<int>(kBoardArea.h * surface_scale_), &width, &height);
    renderer_->configure(width, height, session_->colours());
    const float w = static_cast<float>(width) / surface_scale_;
    const float h = static_cast<float>(height) / surface_scale_;
    board_ = {kBoardArea.x + (kBoardArea.w - w) * 0.5f, kBoardArea.y + (kBoardArea.h - h) * 0.5f, w,
              h};
    pointer_x_ = static_cast<float>(width) * 0.5f;
    pointer_y_ = static_cast<float>(height) * 0.5f;
    keys_ = session_->request_keys();
    session_->force_redraw();
}

void SgtScene::send(int button, std::vector<audio::Cue> &cues, int x, int y)
{
    const KeyOutcome outcome = session_->key(x, y, button);
    const bool select = button == CURSOR_SELECT || button == CURSOR_SELECT2 ||
                        button == LEFT_RELEASE || button == RIGHT_RELEASE;
    switch (outcome.action)
    {
    case Action::move:
        cues.push_back(button == CURSOR_SELECT2 || button == RIGHT_RELEASE ? audio::Cue::mark
                                                                           : audio::Cue::place);
        break;
    case Action::undo:
        cues.push_back(audio::Cue::undo);
        break;
    case Action::redo:
        cues.push_back(audio::Cue::redo);
        break;
    case Action::ui:
        if (button >= CURSOR_UP && button <= CURSOR_RIGHT)
            cues.push_back(audio::Cue::cursor);
        break;
    case Action::no_effect:
        if (select || button == UI_UNDO || button == UI_REDO)
        {
            cues.push_back(audio::Cue::invalid);
            shake_.start(0.14f);
        }
        break;
    default:
        break;
    }
}

void SgtScene::handle_pointer(const InputFrame &input, float dt, std::vector<audio::Cue> &cues)
{
    const float magnitude =
        std::sqrt(input.stick_x * input.stick_x + input.stick_y * input.stick_y);
    const float width = static_cast<float>(renderer_->canvas().width());
    const float height = static_cast<float>(renderer_->canvas().height());
    bool moved = false;
    if (magnitude > 0.0f)
    {
        pointer_active_ = true;
        const float speed = 1100.0f * surface_scale_ * std::pow(magnitude, 1.6f) / magnitude;
        pointer_x_ = std::clamp(pointer_x_ + input.stick_x * speed * dt, 0.0f, width - 1.0f);
        pointer_y_ = std::clamp(pointer_y_ + input.stick_y * speed * dt, 0.0f, height - 1.0f);
        moved = true;
    }
    if (!pointer_active_)
        return;
    const int x = static_cast<int>(pointer_x_);
    const int y = static_cast<int>(pointer_y_);
    if (pointer_button_ == 0)
    {
        if (input.is_pressed(ppz::Action::confirm))
            pointer_button_ = LEFT_BUTTON;
        else if (input.is_pressed(ppz::Action::west))
            pointer_button_ = RIGHT_BUTTON;
        if (pointer_button_ != 0)
            send(pointer_button_, cues, x, y);
        return;
    }
    const bool left = pointer_button_ == LEFT_BUTTON;
    const bool still_held = input.is_held(left ? ppz::Action::confirm : ppz::Action::west);
    if (moved && still_held)
        send(left ? LEFT_DRAG : RIGHT_DRAG, cues, x, y);
    if (!still_held)
    {
        send(left ? LEFT_RELEASE : RIGHT_RELEASE, cues, x, y);
        pointer_button_ = 0;
    }
}

void SgtScene::run_pause_item(int item, std::vector<audio::Cue> &cues, SceneExit &exit)
{
    overlay_ = Overlay::none;
    switch (item)
    {
    case 1:
        session_->new_game();
        layout();
        assisted_ = false;
        last_status_ = session_->status();
        cues.push_back(audio::Cue::new_game);
        break;
    case 2:
        session_->restart();
        cues.push_back(audio::Cue::restart);
        break;
    case 3:
        if (session_->can_solve() && session_->solve().empty())
        {
            assisted_ = true;
            cues.push_back(audio::Cue::solve_reveal);
        }
        else
        {
            cues.push_back(audio::Cue::ui_error);
        }
        break;
    case 4:
        exit = SceneExit::library;
        cues.push_back(audio::Cue::ui_back);
        break;
    default:
        break;
    }
}

SceneExit SgtScene::update(const InputFrame &input, float dt, std::vector<audio::Cue> &cues)
{
    SceneExit exit = SceneExit::none;
    time_ += dt;
    solved_banner_.update(dt);
    shake_.update(dt);

    if (pause_.is_open())
    {
        const int choice = pause_.update(input, dt, cues);
        if (choice != ui::Menu::kNone)
            run_pause_item(choice == ui::Menu::kCancelled ? 0 : choice, cues, exit);
    }
    else if (overlay_ == Overlay::palette)
    {
        const int count = static_cast<int>(keys_.size());
        if (input.nav == Direction::left || input.nav == Direction::up)
            palette_focus_ = (palette_focus_ + count - 1) % count;
        else if (input.nav == Direction::right || input.nav == Direction::down)
            palette_focus_ = (palette_focus_ + 1) % count;
        if (input.nav != Direction::none)
            cues.push_back(audio::Cue::ui_focus);
        if (input.is_pressed(ppz::Action::confirm))
        {
            const KeyOutcome before =
                session_->key(0, 0, keys_[static_cast<std::size_t>(palette_focus_)].button);
            cues.push_back(before.action == Action::move ? audio::Cue::digit : audio::Cue::invalid);
            overlay_ = Overlay::none;
        }
        else if (input.is_pressed(ppz::Action::back) || input.is_pressed(ppz::Action::north))
        {
            overlay_ = Overlay::none;
            cues.push_back(audio::Cue::ui_back);
        }
    }
    else
    {
        if (input.is_pressed(ppz::Action::menu) || input.is_pressed(ppz::Action::back) ||
            input.focus_lost)
        {
            pause_.open("Paused",
                        {{"Resume", 0},
                         {"New game", 1},
                         {"Restart", 2},
                         {"Solve", 3, session_->can_solve()},
                         {"Back to library", 4}},
                        entry_.display_name);
            if (!input.focus_lost)
                cues.push_back(audio::Cue::ui_pause_open);
        }
        else if (input.is_pressed(ppz::Action::north) && !keys_.empty())
        {
            overlay_ = Overlay::palette;
            palette_focus_ = std::min(palette_focus_, static_cast<int>(keys_.size()) - 1);
            cues.push_back(audio::Cue::ui_select);
        }
        else
        {
            if (const int button = cursor_button(input.nav); button != 0)
            {
                pointer_active_ = false;
                send(button, cues);
            }
            handle_pointer(input, dt, cues);
            if (!pointer_active_)
            {
                if (input.is_pressed(ppz::Action::confirm))
                    send(CURSOR_SELECT, cues);
                if (input.is_pressed(ppz::Action::west))
                    send(CURSOR_SELECT2, cues);
            }
            if (input.is_pressed(ppz::Action::page_prev))
                send(UI_UNDO, cues);
            if (input.is_pressed(ppz::Action::page_next))
                send(UI_REDO, cues);
        }
    }

    if (!pause_.is_open())
        pause_.update(InputFrame{}, dt, cues);
    session_->tick(dt);
    session_->redraw();
    const int status = session_->status();
    if (status != last_status_)
    {
        if (status > 0)
        {
            cues.push_back(assisted_ ? audio::Cue::solve_reveal : audio::Cue::complete);
            if (!assisted_)
                solved_banner_.start(3.0f);
        }
        else if (status < 0)
        {
            cues.push_back(audio::Cue::game_over);
        }
        last_status_ = status;
    }
    overlay_fade_.target = overlay_ == Overlay::none ? 0.0f : 1.0f;
    overlay_fade_.update(dt, 16.0f);
    return exit;
}

std::string SgtScene::save()
{
    return session_->serialise();
}

bool SgtScene::in_progress()
{
    return session_->status() == 0 && session_->can_undo();
}

void SgtScene::draw(gfx::DrawList &list) const
{
    using gfx::Align;
    using gfx::Color;
    const Color accent = accent_of(entry_);
    list.gradient_rect({0, 0, 1920, 1080}, 0, ui::theme::kBackgroundTop,
                       ui::theme::kBackgroundBottom);
    list.shadow({560, 280, 800, 520}, 200, 260, accent.with_alpha(0.10f));

    // Header.
    list.text(*fonts_.semibold, fonts_.semibold_texture, entry_.display_name,
              ui::theme::kSafeMargin, 120, 56, ui::theme::kTextOnDark);
    list.text(*fonts_.regular, fonts_.regular_texture, entry_.objective, ui::theme::kSafeMargin,
              164, 24, ui::theme::kTextOnDarkMuted);
    if (!session_->status_text().empty())
        list.text(*fonts_.semibold, fonts_.semibold_texture, session_->status_text(),
                  1920.0f - ui::theme::kSafeMargin, 120, 28, ui::theme::kTextOnDark, Align::right);

    // Board on a paper card; a short shake answers invalid moves.
    float shake = 0.0f;
    if (shake_.running)
        shake = 6.0f * std::sin(shake_.progress() * 6.2831853f * 3.0f) * (1.0f - shake_.progress());
    const gfx::Rect card{board_.x - 22 + shake, board_.y - 22, board_.w + 44, board_.h + 44};
    list.shadow({card.x + 10, card.y + 22, card.w - 20, card.h}, 26, 34, ui::theme::kShadow);
    list.rounded_rect(card, 26, Color::rgb(0xece9e1));
    list.image(renderer_->canvas().texture(), {board_.x + shake, board_.y, board_.w, board_.h},
               {0.0f, 1.0f, 1.0f, -1.0f}, Color{1, 1, 1, 1});

    if (pointer_active_)
    {
        const float px = board_.x + shake + pointer_x_ / surface_scale_;
        const float py = board_.y + pointer_y_ / surface_scale_;
        list.circle(px + 2, py + 3, 13, Color::rgb(0x000000, 0.25f));
        list.circle(px, py, 12, ui::theme::kFocus);
        list.ring(px, py, 12, 3, ui::theme::kInk);
    }

    if (solved_banner_.running)
    {
        const float p = solved_banner_.progress();
        const float pop = tween::back_out(std::min(1.0f, p * 4.0f));
        const float fade = p > 0.8f ? (1.0f - p) / 0.2f : 1.0f;
        list.push_opacity(fade);
        list.push_transform(pop, 960, board_.y - 4, 0, 0);
        list.shadow({800, board_.y - 44, 320, 80}, 40, 20, ui::theme::kShadow);
        list.rounded_rect({800, board_.y - 44, 320, 80}, 40, ui::theme::kFocus);
        list.text(*fonts_.semibold, fonts_.semibold_texture, "Solved!", 960, board_.y + 12, 44,
                  ui::theme::kInk, Align::center);
        list.pop_transform();
        list.pop_opacity();
    }

    // Controls hint bar.
    struct Hint
    {
        ui::FaceButton button;
        const char *label;
    };
    std::vector<Hint> hints = {{ui::FaceButton::cross, "Select"}, {ui::FaceButton::square, "Mark"}};
    if (!keys_.empty())
        hints.push_back({ui::FaceButton::triangle, "Keys"});
    float hx = ui::theme::kSafeMargin;
    for (const Hint &hint : hints)
    {
        ui::draw_face_button(list, hint.button, hx + 20, 1010, 40);
        const float width = list.text(*fonts_.regular, fonts_.regular_texture, hint.label, hx + 50,
                                      1019, ui::theme::kTextBody, ui::theme::kTextOnDark);
        hx += width + 100.0f;
    }
    list.text(*fonts_.regular, fonts_.regular_texture,
              "L1 Undo   R1 Redo   Left stick Pointer   Options Menu",
              1920.0f - ui::theme::kSafeMargin, 1019, ui::theme::kTextBody,
              ui::theme::kTextOnDarkMuted, Align::right);

    if (overlay_fade_.value > 0.01f)
    {
        list.push_opacity(overlay_fade_.value);
        list.rounded_rect({0, 0, 1920, 1080}, 0, Color::rgb(0x05070f, 0.55f));
        if (overlay_ == Overlay::palette)
            draw_palette(list);
        list.pop_opacity();
    }
    pause_.draw(list, fonts_);
}

void SgtScene::draw_palette(gfx::DrawList &list) const
{
    using gfx::Align;
    const float cell = 84.0f;
    const float gap = 12.0f;
    const int count = static_cast<int>(keys_.size());
    const int columns = std::min(count, 10);
    const int rows = (count + columns - 1) / columns;
    const float w = static_cast<float>(columns) * (cell + gap) - gap + 64.0f;
    const float h = static_cast<float>(rows) * (cell + gap) - gap + 120.0f;
    const gfx::Rect panel{960 - w * 0.5f, 540 - h * 0.5f, w, h};
    list.shadow({panel.x, panel.y + 16, panel.w, panel.h}, 28, 40, ui::theme::kShadow);
    list.rounded_rect(panel, 28, ui::theme::kPaper);
    list.text(*fonts_.semibold, fonts_.semibold_texture, "Enter", panel.x + 32, panel.y + 52, 30,
              ui::theme::kInk);
    for (int i = 0; i < count; ++i)
    {
        const float x = panel.x + 32 + static_cast<float>(i % columns) * (cell + gap);
        const float y = panel.y + 76 + static_cast<float>(i / columns) * (cell + gap);
        const bool focused = i == palette_focus_;
        list.rounded_rect({x, y, cell, cell}, 18,
                          focused ? gfx::Color::rgb(0x1b1d2b) : ui::theme::kPaperShade);
        const std::string &label = keys_[static_cast<std::size_t>(i)].label;
        list.text(*fonts_.semibold, fonts_.semibold_texture, label, x + cell * 0.5f,
                  y + cell * 0.5f + 12, label.size() > 2 ? 22.0f : 36.0f,
                  focused ? ui::theme::kTextOnDark : ui::theme::kInk, Align::center);
    }
}

} // namespace ppz::sgt
