// ProsperoPuzzles - Tatham drawing API rendered into a persistent GL canvas.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "games/sgt/sgt_session.hpp"
#include "gfx/canvas.hpp"
#include "gfx/draw_list.hpp"
#include "gfx/gl_batch.hpp"
#include "ui/theme.hpp"

#include <map>
#include <vector>

namespace ppz::sgt
{

// Records each Tatham redraw into a draw list and flushes it into the canvas
// at end_draw (and before blitter reads), keeping everything the game drew
// earlier. Tatham draws only what changed, so the canvas is never cleared
// except when it is recreated.
class CanvasRenderer final : public Renderer
{
  public:
    CanvasRenderer(gfx::GlBatch &batch, const ui::Fonts &fonts);
    ~CanvasRenderer() override;

    // Resizes the canvas and refreshes the palette; the session must force a
    // full redraw afterwards.
    bool configure(int width, int height, const std::vector<float> &palette);
    const gfx::Canvas &canvas() const
    {
        return canvas_;
    }
    bool dirty() const
    {
        return dirty_;
    }
    void clear_dirty()
    {
        dirty_ = false;
    }

    void begin_draw() override;
    void end_draw() override;
    void rect(int x, int y, int w, int h, int colour) override;
    void line(float x1, float y1, float x2, float y2, float thickness, int colour) override;
    void polygon(const int *coords, int npoints, int fill, int outline) override;
    void circle(int cx, int cy, int radius, int fill, int outline) override;
    void text(int x, int y, int font_type, int font_size, int align, int colour,
              const char *text) override;
    void clip(int x, int y, int w, int h) override;
    void unclip() override;
    int blitter_new(int w, int h) override;
    void blitter_free(int id) override;
    void blitter_save(int id, int x, int y) override;
    void blitter_load(int id, int x, int y) override;

  private:
    struct Blitter
    {
        int w = 0;
        int h = 0;
        GLuint texture = 0;
    };

    gfx::Color colour(int index) const;
    void flush();

    gfx::GlBatch &batch_;
    ui::Fonts fonts_;
    gfx::Canvas canvas_;
    gfx::DrawList pending_;
    std::vector<gfx::Color> palette_;
    std::map<int, Blitter> blitters_;
    int next_blitter_ = 1;
    bool clipped_ = false;
    gfx::Rect clip_rect_;
    bool dirty_ = false;
    std::vector<float> scratch_;
};

} // namespace ppz::sgt
