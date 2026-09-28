// ProsperoPuzzles - Library card previews rendered from real game boards.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/thumbnails.hpp"

#include "games/kit/puzzle_scene.hpp"
#include "games/registry.hpp"
#include "games/sgt/sgt_catalog.hpp"
#include "games/sgt/sgt_session.hpp"

namespace ppz::app
{

Thumbnails::Thumbnails(gfx::GlBatch &batch, const ui::Fonts &fonts, float surface_scale)
    : batch_(batch), fonts_(fonts), scale_(surface_scale)
{
}

void Thumbnails::request(const std::string &id, const std::string &save)
{
    const games::GameInfo *game = games::find(id);
    if (game == nullptr || (game->kind != games::Kind::sgt && game->kind != games::Kind::native))
        return; // 2048 and Tenfold keep their drawn artwork
    for (auto &entry : pending_)
    {
        if (entry.first == id)
        {
            entry.second = save;
            return;
        }
    }
    pending_.emplace_back(id, save);
}

void Thumbnails::pump(int budget)
{
    while (budget-- > 0 && !pending_.empty())
    {
        const auto [id, save] = pending_.front();
        pending_.erase(pending_.begin());
        render(id, save);
    }
}

void Thumbnails::render_native(const std::string &id, const std::string &save)
{
    const games::GameInfo *game = games::find(id);
    auto scene = game->create(fonts_);
    kit::PuzzleScene *puzzle = scene->as_puzzle();
    if (puzzle == nullptr)
        return;
    // The saved game, or a fixed medium demo board.
    if (save.empty())
        puzzle->new_puzzle(0x70726576696577ULL, 1);
    else
        puzzle->start(save, {});
    // Fit the board card into the thumbnail, keeping its shape.
    const gfx::Rect card = puzzle->preview_bounds();
    const float fit = std::min(ui::kThumbnailWidth / card.w, ui::kThumbnailHeight / card.h);
    const int width = std::max(1, static_cast<int>(card.w * fit * scale_));
    const int height = std::max(1, static_cast<int>(card.h * fit * scale_));
    Preview &preview = previews_[id];
    if (!preview.canvas)
        preview.canvas = std::make_unique<gfx::Canvas>();
    if (preview.canvas->width() != width || preview.canvas->height() != height)
        preview.canvas->create(width, height, 4);
    const float s = fit * scale_;
    gfx::DrawList list;
    list.push_transform(s, 0.0f, 0.0f, -card.x * s, -card.y * s);
    puzzle->draw_preview(list);
    list.pop_transform();
    GLint previous = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
    preview.canvas->bind();
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // rounded corners stay transparent
    glClear(GL_COLOR_BUFFER_BIT);
    batch_.draw(list, gfx::Viewport{}, width, height);
    preview.canvas->unbind(static_cast<GLuint>(previous));
    preview.canvas->resolve();
    preview.width = static_cast<float>(width) / scale_;
    preview.height = static_cast<float>(height) / scale_;
}

void Thumbnails::render(const std::string &id, const std::string &save)
{
    const sgt::GameEntry *entry = sgt::find_game(id);
    if (entry == nullptr)
    {
        render_native(id, save);
        return;
    }
    Preview &preview = previews_[id];
    if (!preview.renderer)
        preview.renderer = std::make_unique<sgt::CanvasRenderer>(batch_, fonts_);
    // The session is local: its teardown only detaches from the renderer, and
    // the canvas keeps the finished drawing.
    sgt::Session session(*entry);
    session.set_renderer(preview.renderer.get());
    if (save.empty() || !session.deserialise(save).empty())
        session.load_game_id(session.encoded_params() + "#prospero-preview");
    int width = 0;
    int height = 0;
    session.resize(static_cast<int>(ui::kThumbnailWidth * scale_),
                   static_cast<int>(ui::kThumbnailHeight * scale_), &width, &height);
    preview.renderer->configure(width, height, session.colours(), entry->id);
    session.force_redraw();
    preview.width = static_cast<float>(width) / scale_;
    preview.height = static_cast<float>(height) / scale_;
}

bool Thumbnails::thumbnail(const std::string &id, std::uint32_t *texture, float *width,
                           float *height) const
{
    const auto it = previews_.find(id);
    if (it == previews_.end() || it->second.width <= 0.0f)
        return false;
    if (it->second.canvas)
        *texture = it->second.canvas->texture();
    else if (it->second.renderer)
        *texture = it->second.renderer->canvas().texture();
    else
        return false;
    *width = it->second.width;
    *height = it->second.height;
    return true;
}

} // namespace ppz::app
