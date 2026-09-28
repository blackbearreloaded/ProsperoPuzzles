// ProsperoPuzzles - Tatham drawing API rendered into a persistent GL canvas.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/sgt/sgt_canvas.hpp"

#include "games/sgt/sgt_c.hpp"

namespace ppz::sgt
{

namespace
{

// Tatham's integer coordinates name pixels; strokes are centred on them.
constexpr float kPixelCentre = 0.5f;

} // namespace

CanvasRenderer::CanvasRenderer(gfx::GlBatch &batch, const ui::Fonts &fonts)
    : batch_(batch), fonts_(fonts)
{
}

CanvasRenderer::~CanvasRenderer()
{
    for (auto &[id, blitter] : blitters_)
    {
        if (blitter.texture != 0)
            glDeleteTextures(1, &blitter.texture);
    }
}

bool CanvasRenderer::configure(int width, int height, const std::vector<float> &palette)
{
    palette_.clear();
    for (std::size_t i = 0; i + 2 < palette.size(); i += 3)
        palette_.push_back(gfx::Color{palette[i], palette[i + 1], palette[i + 2], 1.0f});
    pending_.clear();
    clipped_ = false;
    dirty_ = true;
    if (canvas_.width() == width && canvas_.height() == height)
        return true;
    return canvas_.create(width, height, 4);
}

gfx::Color CanvasRenderer::colour(int index) const
{
    if (index < 0 || index >= static_cast<int>(palette_.size()))
        return gfx::Color{1.0f, 0.0f, 1.0f, 1.0f}; // loud magenta: a bad index is a bug
    return palette_[static_cast<std::size_t>(index)];
}

void CanvasRenderer::flush()
{
    if (pending_.instances().empty() && pending_.mesh_vertices().empty())
        return;
    GLint previous = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
    canvas_.bind();
    batch_.draw(pending_, gfx::Viewport{}, canvas_.width(), canvas_.height());
    canvas_.unbind(static_cast<GLuint>(previous));
    // Keep the clip state across the flush.
    pending_.clear();
    if (clipped_)
        pending_.push_clip(clip_rect_);
    dirty_ = true;
}

void CanvasRenderer::begin_draw()
{
}

void CanvasRenderer::end_draw()
{
    flush();
    if (dirty_)
        canvas_.resolve();
}

void CanvasRenderer::rect(int x, int y, int w, int h, int colour_index)
{
    pending_.rounded_rect({static_cast<float>(x), static_cast<float>(y), static_cast<float>(w),
                           static_cast<float>(h)},
                          0.0f, colour(colour_index));
}

void CanvasRenderer::line(float x1, float y1, float x2, float y2, float thickness, int colour_index)
{
    pending_.line(x1 + kPixelCentre, y1 + kPixelCentre, x2 + kPixelCentre, y2 + kPixelCentre,
                  thickness < 1.0f ? 1.0f : thickness, colour(colour_index));
}

void CanvasRenderer::polygon(const int *coords, int npoints, int fill, int outline)
{
    if (npoints < 2)
        return;
    scratch_.clear();
    for (int i = 0; i < npoints; ++i)
    {
        scratch_.push_back(static_cast<float>(coords[2 * i]) + kPixelCentre);
        scratch_.push_back(static_cast<float>(coords[2 * i + 1]) + kPixelCentre);
    }
    if (fill >= 0 && npoints >= 3)
        pending_.polygon(scratch_.data(), npoints, colour(fill));
    const gfx::Color stroke = colour(outline);
    for (int i = 0; i < npoints; ++i)
    {
        const int j = (i + 1) % npoints;
        pending_.line(scratch_[2 * i], scratch_[2 * i + 1], scratch_[2 * j], scratch_[2 * j + 1],
                      1.0f, stroke);
    }
}

void CanvasRenderer::circle(int cx, int cy, int radius, int fill, int outline)
{
    const float x = static_cast<float>(cx) + kPixelCentre;
    const float y = static_cast<float>(cy) + kPixelCentre;
    const float r = static_cast<float>(radius) + kPixelCentre;
    if (fill >= 0)
        pending_.bordered_rect({x - r, y - r, 2 * r, 2 * r}, r, colour(fill), 1.0f,
                               colour(outline));
    else
        pending_.ring(x, y, r - 0.5f, 1.0f, colour(outline));
}

void CanvasRenderer::text(int x, int y, int font_type, int font_size, int align, int colour_index,
                          const char *text)
{
    (void)font_type; // Inter serves both FONT_FIXED and FONT_VARIABLE
    const gfx::Font &font = *fonts_.semibold;
    const float size = static_cast<float>(font_size);
    float baseline = static_cast<float>(y);
    if ((align & ALIGN_VCENTRE) != 0)
        baseline += size * 0.36f; // centre of Inter's cap height
    gfx::Align horizontal = gfx::Align::left;
    if ((align & ALIGN_HCENTRE) != 0)
        horizontal = gfx::Align::center;
    else if ((align & ALIGN_HRIGHT) != 0)
        horizontal = gfx::Align::right;
    pending_.text(font, fonts_.semibold_texture, text, static_cast<float>(x), baseline, size,
                  colour(colour_index), horizontal);
}

void CanvasRenderer::clip(int x, int y, int w, int h)
{
    if (clipped_)
        pending_.pop_clip();
    clip_rect_ = {static_cast<float>(x), static_cast<float>(y), static_cast<float>(w),
                  static_cast<float>(h)};
    pending_.push_clip(clip_rect_);
    clipped_ = true;
}

void CanvasRenderer::unclip()
{
    if (clipped_)
        pending_.pop_clip();
    clipped_ = false;
}

int CanvasRenderer::blitter_new(int w, int h)
{
    const int id = next_blitter_++;
    blitters_[id] = Blitter{w, h, 0};
    return id;
}

void CanvasRenderer::blitter_free(int id)
{
    auto it = blitters_.find(id);
    if (it == blitters_.end())
        return;
    flush(); // a pending load may still sample this texture
    if (it->second.texture != 0)
        glDeleteTextures(1, &it->second.texture);
    blitters_.erase(it);
}

void CanvasRenderer::blitter_save(int id, int x, int y)
{
    auto it = blitters_.find(id);
    if (it == blitters_.end())
        return;
    flush();
    if (it->second.texture != 0)
        glDeleteTextures(1, &it->second.texture);
    it->second.texture = canvas_.save_region(x, y, it->second.w, it->second.h);
}

void CanvasRenderer::blitter_load(int id, int x, int y)
{
    auto it = blitters_.find(id);
    if (it == blitters_.end() || it->second.texture == 0)
        return;
    // Saved regions are stored bottom-up like the canvas: flip v.
    pending_.image(it->second.texture,
                   {static_cast<float>(x), static_cast<float>(y), static_cast<float>(it->second.w),
                    static_cast<float>(it->second.h)},
                   {0.0f, 1.0f, 1.0f, -1.0f}, gfx::Color{1, 1, 1, 1});
}

} // namespace ppz::sgt
