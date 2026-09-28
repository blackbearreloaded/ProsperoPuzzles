// ProsperoPuzzles - Visual theme tokens and shared UI resources.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/theme.hpp"

namespace ppz::ui
{

void draw_face_button(gfx::DrawList &list, FaceButton button, float cx, float cy, float size)
{
    const float half = size * 0.5f;
    const float stroke = size * 0.11f;
    // Dark disc behind the symbol, like the physical button.
    list.circle(cx, cy, half, gfx::Color::rgb(0x11131f, 0.92f));
    const float inner = half * 0.52f;
    switch (button)
    {
    case FaceButton::cross:
        list.line(cx - inner, cy - inner, cx + inner, cy + inner, stroke, theme::kCross);
        list.line(cx - inner, cy + inner, cx + inner, cy - inner, stroke, theme::kCross);
        break;
    case FaceButton::circle:
        list.ring(cx, cy, inner + stroke * 0.3f, stroke, theme::kCircle);
        break;
    case FaceButton::square:
    {
        const float s = inner * 1.7f;
        list.bordered_rect({cx - s * 0.5f, cy - s * 0.5f, s, s}, stroke * 0.3f,
                           theme::kSquare.with_alpha(0.0f), stroke, theme::kSquare);
        break;
    }
    case FaceButton::triangle:
    {
        const float w = inner * 2.1f;
        const float h = inner * 1.85f;
        list.triangle({cx - w * 0.5f, cy - h * 0.58f, w, h}, theme::kTriangle, stroke);
        break;
    }
    }
}

} // namespace ppz::ui
