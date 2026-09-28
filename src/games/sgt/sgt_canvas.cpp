// ProsperoPuzzles - Tatham drawing API rendered into a persistent GL canvas.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/sgt/sgt_canvas.hpp"

#include "games/sgt/sgt_c.hpp"

#include <algorithm>

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

bool CanvasRenderer::configure(int width, int height, const std::vector<float> &palette,
                               std::string_view game_id)
{
    palette_ = restyle_palette(game_id, palette);
    style_ = style_for(game_id);
    short_side_ = static_cast<float>(std::max(1, std::min(width, height)));
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

bool CanvasRenderer::bevel_card(const int *coords, int npoints, int fill, int outline)
{
    if (!style_.bevel_cards || npoints != 3 || fill < 0 || fill != outline)
        return false;
    int x0 = coords[0], x1 = coords[0], y0 = coords[1], y1 = coords[1];
    for (int i = 1; i < 3; ++i)
    {
        x0 = std::min(x0, coords[2 * i]);
        x1 = std::max(x1, coords[2 * i]);
        y0 = std::min(y0, coords[2 * i + 1]);
        y1 = std::max(y1, coords[2 * i + 1]);
    }
    const int size = x1 - x0 + 1;
    if (size != y1 - y0 + 1 || static_cast<float>(size) < 0.06f * short_side_)
        return false;
    // Which corner holds the right angle: top-left is the light half, bottom-right the dark.
    bool top_left = false, bottom_right = false;
    for (int i = 0; i < 3; ++i)
    {
        const int px = coords[2 * i], py = coords[2 * i + 1];
        const int prev = (i + 2) % 3, next = (i + 1) % 3;
        const bool axis = (coords[2 * prev] == px && coords[2 * next + 1] == py) ||
                          (coords[2 * next] == px && coords[2 * prev + 1] == py);
        if (!axis)
            continue;
        top_left = top_left || (px == x0 && py == y0);
        bottom_right = bottom_right || (px == x1 && py == y1);
    }
    if (top_left == bottom_right)
        return false;
    const float s = static_cast<float>(size);
    const gfx::Rect box{static_cast<float>(x0), static_cast<float>(y0), s, s};
    const float inset = s * 0.045f;
    const gfx::Rect face{box.x + inset, box.y + inset, s - 2 * inset, s - 2 * inset};
    if (bottom_right)
    {
        // Drawn first: clear the square, then lay the card's shadow.
        pending_.rounded_rect(box, 0.0f, colour(0));
        // Clipped to the square: an empty neighbour never shows a stray shadow.
        pending_.push_clip(box);
        pending_.shadow({face.x, face.y + s * 0.04f, face.w, face.h}, s * 0.16f, s * 0.05f,
                        gfx::Color::rgb(0x28334f, 0.28f));
        pending_.pop_clip();
        return true;
    }
    pending_.rounded_rect(face, s * 0.16f, gfx::Color::rgb(0xfffefa));
    card_box_ = box;
    card_face_pending_ = true;
    return true;
}

void CanvasRenderer::rect(int x, int y, int w, int h, int colour_index)
{
    if (card_face_pending_)
    {
        card_face_pending_ = false;
        const bool inside = x >= card_box_.x && y >= card_box_.y &&
                            x + w <= card_box_.x + card_box_.w &&
                            y + h <= card_box_.y + card_box_.h;
        if (inside)
        {
            // The bevel's face: the card already covers it. Flashes tint the card.
            if (colour_index != 0)
            {
                const float inset = card_box_.w * 0.045f;
                pending_.rounded_rect({card_box_.x + inset, card_box_.y + inset,
                                       card_box_.w - 2 * inset, card_box_.h - 2 * inset},
                                      card_box_.w * 0.16f, colour(colour_index));
            }
            return;
        }
    }
    const gfx::Rect r{static_cast<float>(x), static_cast<float>(y), static_cast<float>(w),
                      static_cast<float>(h)};
    const float shorter = static_cast<float>(std::min(w, h));
    // Tile games: pieces become rounded cards; the background stays square
    // so erasing a cell still covers it completely.
    if (style_.round_min_fraction > 0.0f && colour_index != 0 &&
        shorter >= style_.round_min_fraction * short_side_)
    {
        pending_.rounded_rect(r, shorter * style_.round_radius, colour(colour_index));
        return;
    }
    pending_.rounded_rect(r, 0.0f, colour(colour_index));
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
    if (bevel_card(coords, npoints, fill, outline))
        return;
    card_face_pending_ = false;
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
    if (fill >= 0 && style_.flat_discs && fill != outline && outline != 0)
        pending_.circle(x, y, r, colour(fill));
    else if (fill >= 0)
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
