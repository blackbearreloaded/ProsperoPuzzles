// ProsperoPuzzles - Library card previews rendered from real game boards.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "games/sgt/sgt_canvas.hpp"
#include "gfx/gl_batch.hpp"
#include "ui/library_scene.hpp"
#include "ui/theme.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ppz::app
{

// Renders each Tatham puzzle (a fixed demo board, or the saved game when one
// is in progress) into a small canvas, a few per frame so startup never stalls.
class Thumbnails final : public ui::ThumbnailSource
{
  public:
    Thumbnails(gfx::GlBatch &batch, const ui::Fonts &fonts, float surface_scale);

    // Queues (re)rendering; save is a serialised game or empty for the demo board.
    void request(const std::string &id, const std::string &save);
    // Renders up to budget queued previews.
    void pump(int budget);

    bool thumbnail(const std::string &id, std::uint32_t *texture, float *width,
                   float *height) const override;

  private:
    struct Preview
    {
        std::unique_ptr<sgt::CanvasRenderer> renderer;
        float width = 0.0f; // virtual pixels
        float height = 0.0f;
    };

    void render(const std::string &id, const std::string &save);

    gfx::GlBatch &batch_;
    ui::Fonts fonts_;
    float scale_;
    std::map<std::string, Preview> previews_;
    std::vector<std::pair<std::string, std::string>> pending_;
};

} // namespace ppz::app
