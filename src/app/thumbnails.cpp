// ProsperoPuzzles - Library card previews rendered from real game boards.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/thumbnails.hpp"

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
    if (sgt::find_game(id) == nullptr)
        return; // only Tatham puzzles have board previews for now
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

void Thumbnails::render(const std::string &id, const std::string &save)
{
    const sgt::GameEntry *entry = sgt::find_game(id);
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
    preview.renderer->configure(width, height, session.colours());
    session.force_redraw();
    preview.width = static_cast<float>(width) / scale_;
    preview.height = static_cast<float>(height) / scale_;
}

bool Thumbnails::thumbnail(const std::string &id, std::uint32_t *texture, float *width,
                           float *height) const
{
    const auto it = previews_.find(id);
    if (it == previews_.end() || !it->second.renderer || it->second.width <= 0.0f)
        return false;
    *texture = it->second.renderer->canvas().texture();
    *width = it->second.width;
    *height = it->second.height;
    return true;
}

} // namespace ppz::app
