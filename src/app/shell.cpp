// ProsperoPuzzles - App shell: library, game screens, transitions and saves.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/shell.hpp"

#include "core/save_file.hpp"
#include "games/registry.hpp"
#include "games/g2048/g2048_scene.hpp"
#include "games/sgt/sgt_scene.hpp"
#include "games/tenfold/tenfold_scene.hpp"
#include "platform/ps5/system.hpp"

#include <unistd.h>

namespace ppz::app
{

namespace
{

constexpr float kTransitionSeconds = 0.38f;
constexpr std::uint16_t kLibraryVersion = 1;
constexpr std::uint16_t kGameVersion = 1;

} // namespace

Shell::Shell(gfx::GlBatch &batch, const ui::Fonts &fonts, float surface_scale,
             std::string data_root)
    : batch_(batch), fonts_(fonts), surface_scale_(surface_scale), root_(std::move(data_root)),
      library_(games::library_entries()), library_scene_(library_)
{
    save::ensure_directory(root_);
    save::ensure_directory(root_ + "/games");
    std::string data;
    if (save::read_file(root_ + "/library.bin", &data))
    {
        const auto decoded = save::decode(save::Kind::library, data);
        std::string focused;
        if (decoded.ok && library_.decode(decoded.payload, &focused))
        {
            library_scene_.refresh();
            library_scene_.focus_game(focused);
        }
        else
        {
            sys::log("[PPZ] library.bin ignored: %s", decoded.error.c_str());
        }
    }
    int resumable = 0;
    for (const games::GameInfo &game : games::all())
    {
        std::string ignored;
        if (save::read_file(game_path(game.id), &ignored))
        {
            library_.set_in_progress(game.id, true);
            ++resumable;
        }
    }
    sys::log("[PPZ] library games=%zu favorites_loaded resumable=%d", library_.entries().size(),
             resumable);
}

std::string Shell::game_path(const std::string &id) const
{
    return root_ + "/games/" + id + ".sav";
}

std::string Shell::stats_path(const std::string &id) const
{
    return root_ + "/games/" + id + ".stats";
}

void Shell::save_library()
{
    const std::string error = save::write_atomic(
        root_ + "/library.bin", save::encode(save::Kind::library, kLibraryVersion,
                                             library_.encode(library_scene_.focused_id())));
    if (!error.empty())
        sys::log("[PPZ] library save failed: %s", error.c_str());
}

void Shell::save_game()
{
    if (!game_)
        return;
    const std::string id = game_->id();
    const std::string stats = game_->stats();
    if (!stats.empty())
    {
        const std::string error = save::write_atomic(
            stats_path(id), save::encode(save::Kind::stats, kGameVersion, stats));
        if (!error.empty())
            sys::log("[PPZ] stats save failed %s: %s", id.c_str(), error.c_str());
    }
    if (game_->in_progress())
    {
        const std::string error = save::write_atomic(
            game_path(id), save::encode(save::Kind::game, kGameVersion, game_->save()));
        if (!error.empty())
            sys::log("[PPZ] game save failed %s: %s", id.c_str(), error.c_str());
        library_.set_in_progress(id, true);
    }
    else
    {
        // Solved, lost or untouched: nothing to resume.
        ::unlink(game_path(id).c_str());
        library_.set_in_progress(id, false);
    }
}

void Shell::toast(const std::string &text)
{
    toast_text_ = text;
    toast_timer_.start(2.2f);
}

void Shell::launch(const std::string &id)
{
    const games::GameInfo *game = games::find(id);
    if (game == nullptr)
    {
        cues_.push_back(audio::Cue::ui_error);
        return;
    }
    std::string save_data;
    std::string payload;
    if (save::read_file(game_path(id), &save_data))
    {
        const auto decoded = save::decode(save::Kind::game, save_data);
        if (decoded.ok)
            payload = decoded.payload;
    }
    switch (game->kind)
    {
    case games::Kind::sgt:
        game_ = std::make_unique<sgt::SgtScene>(*game->sgt, batch_, fonts_, surface_scale_);
        break;
    case games::Kind::g2048:
        game_ = std::make_unique<g2048::G2048Scene>(fonts_);
        break;
    case games::Kind::tenfold:
        game_ = std::make_unique<tenfold::TenfoldScene>(fonts_);
        break;
    }
    std::string stats_data;
    std::string stats_payload;
    if (save::read_file(stats_path(id), &stats_data))
    {
        const auto decoded = save::decode(save::Kind::stats, stats_data);
        if (decoded.ok)
            stats_payload = decoded.payload;
    }
    game_->start(payload, stats_payload);
    sys::log("[PPZ] launch game=%s resumed=%d", id.c_str(), payload.empty() ? 0 : 1);
    stage_ = Stage::entering;
    transition_.start(kTransitionSeconds);
    save_library();
}

void Shell::update(const InputFrame &input, float dt)
{
    transition_.update(dt);
    toast_timer_.update(dt);
    switch (stage_)
    {
    case Stage::library:
    {
        const ui::LibraryRequest request = library_scene_.update(input, dt, cues_);
        if (request.kind == ui::LibraryRequest::Kind::launch)
            launch(request.game_id);
        else if (request.kind == ui::LibraryRequest::Kind::favorites_changed)
            save_library();
        else if (request.kind == ui::LibraryRequest::Kind::details ||
                 request.kind == ui::LibraryRequest::Kind::settings)
            toast("Coming soon");
        break;
    }
    case Stage::entering:
        library_scene_.update(InputFrame{}, dt, cues_);
        if (!transition_.running)
            stage_ = Stage::game;
        break;
    case Stage::game:
    {
        const games::SceneExit exit = game_->update(input, dt, cues_);
        autosave_ += dt;
        const bool moved = !cues_.empty();
        if (exit == games::SceneExit::library)
        {
            save_game();
            save_library();
            library_scene_.focus_game(game_->id());
            stage_ = Stage::leaving;
            transition_.start(kTransitionSeconds);
        }
        else if (moved && autosave_ > 1.0f)
        {
            // Throttled autosave after activity.
            save_game();
            autosave_ = 0.0f;
        }
        break;
    }
    case Stage::leaving:
        library_scene_.update(InputFrame{}, dt, cues_);
        if (!transition_.running)
        {
            game_.reset();
            stage_ = Stage::library;
        }
        break;
    }
}

void Shell::draw(gfx::DrawList &list) const
{
    const float p = tween::cubic_in_out(transition_.progress());
    switch (stage_)
    {
    case Stage::library:
        library_scene_.draw(list, fonts_);
        break;
    case Stage::entering:
        // The library zooms toward the viewer and fades while the game arrives.
        list.push_transform(1.0f + 0.12f * p, 960, 540, 0, 0);
        list.push_opacity(1.0f - p);
        library_scene_.draw(list, fonts_);
        list.pop_opacity();
        list.pop_transform();
        list.push_transform(0.94f + 0.06f * p, 960, 540, 0, 0);
        list.push_opacity(p);
        game_->draw(list);
        list.pop_opacity();
        list.pop_transform();
        break;
    case Stage::game:
        game_->draw(list);
        break;
    case Stage::leaving:
        list.push_transform(1.0f + 0.12f * (1.0f - p), 960, 540, 0, 0);
        list.push_opacity(p);
        library_scene_.draw(list, fonts_);
        list.pop_opacity();
        list.pop_transform();
        list.push_opacity(1.0f - p);
        game_->draw(list);
        list.pop_opacity();
        break;
    }
    if (toast_timer_.running)
    {
        const float t = toast_timer_.progress();
        const float alpha = t < 0.1f ? t / 0.1f : (t > 0.85f ? (1.0f - t) / 0.15f : 1.0f);
        const float width = fonts_.semibold->measure(toast_text_, 28) + 64.0f;
        list.push_opacity(alpha);
        list.shadow({960 - width * 0.5f, 874, width, 64}, 32, 20, ui::theme::kShadow);
        list.rounded_rect({960 - width * 0.5f, 866, width, 64}, 32, ui::theme::kPaper);
        list.text(*fonts_.semibold, fonts_.semibold_texture, toast_text_, 960, 908, 28,
                  ui::theme::kInk, gfx::Align::center);
        list.pop_opacity();
    }
}

std::vector<audio::Cue> Shell::take_cues()
{
    std::vector<audio::Cue> out;
    out.swap(cues_);
    return out;
}

std::string Shell::active_game() const
{
    return game_ && stage_ == Stage::game ? game_->id() : std::string();
}

} // namespace ppz::app
