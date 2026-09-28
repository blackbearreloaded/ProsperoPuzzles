// ProsperoPuzzles - Application entry point.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// M1 bring-up: opens the OpenGL 4.6 display, presents an animated test pattern
// every frame, and logs the lifecycle markers and frame pacing that the first
// hardware runs use as their oracle.

#include "core/frame_stats.hpp"
#include "gfx/gl_program.hpp"
#include "platform/ps5/display_egl.hpp"
#include "platform/ps5/system.hpp"

#include <GL/glcorearb.h>

#include <cmath>
#include <cstddef>
#include <cstdint>

extern "C" void ppz_heap_stats(std::size_t *live_bytes, std::size_t *peak_bytes,
                               std::size_t *blocks, std::size_t *failures);

namespace
{

constexpr const char *kVertexShader = R"(
layout(location = 0) uniform vec2 u_resolution;
layout(location = 1) uniform vec4 u_rect;
out vec2 v_local;
out vec2 v_half;
const vec2 kCorners[6] = vec2[6](vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
                                 vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0));
void main()
{
    vec2 corner = kCorners[gl_VertexID];
    vec2 pixel = u_rect.xy + corner * u_rect.zw;
    v_half = 0.5 * u_rect.zw;
    v_local = (corner - 0.5) * u_rect.zw;
    vec2 ndc = pixel / u_resolution * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
}
)";

constexpr const char *kFragmentShader = R"(
layout(location = 2) uniform vec4 u_color;
layout(location = 3) uniform float u_radius;
in vec2 v_local;
in vec2 v_half;
out vec4 frag_color;
void main()
{
    vec2 q = abs(v_local) - v_half + vec2(u_radius);
    float distance = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - u_radius;
    float coverage = clamp(0.5 - distance, 0.0, 1.0);
    frag_color = vec4(u_color.rgb, u_color.a * coverage);
}
)";

// Test pattern for the first hardware runs: a card, an orbiting tile and a
// one-second sweep bar, so a screenshot proves frames are advancing.
class TestPattern
{
  public:
    bool init()
    {
        program_ = ppz::gfx::build_program("test-pattern", kVertexShader, kFragmentShader);
        if (program_ == 0)
            return false;
        glGenVertexArrays(1, &vao_);
        return true;
    }

    void draw(double seconds, int width, int height)
    {
        const float scale = static_cast<float>(height) / 1080.0f;
        glViewport(0, 0, width, height);
        glClearColor(0.03f, 0.05f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(program_);
        glBindVertexArray(vao_);
        glUniform2f(0, static_cast<float>(width), static_cast<float>(height));

        rect(560, 240, 800, 600, 36, 0.93f, 0.91f, 0.86f, 1.0f, scale);
        const double angle = seconds * 1.4;
        const float x = 960.0f + 230.0f * static_cast<float>(std::cos(angle)) - 70.0f;
        const float y = 520.0f + 150.0f * static_cast<float>(std::sin(angle)) - 70.0f;
        rect(x, y, 140, 140, 28, 0.96f, 0.45f, 0.26f, 1.0f, scale);
        const float sweep = static_cast<float>(seconds - std::floor(seconds));
        rect(640, 760, 640, 24, 12, 0.78f, 0.76f, 0.72f, 1.0f, scale);
        rect(640, 760, 24.0f + 616.0f * sweep, 24, 12, 0.18f, 0.55f, 0.95f, 1.0f, scale);
    }

  private:
    void rect(float x, float y, float w, float h, float radius, float r, float g, float b, float a,
              float scale)
    {
        glUniform4f(1, x * scale, y * scale, w * scale, h * scale);
        glUniform4f(2, r, g, b, a);
        glUniform1f(3, radius * scale);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    GLuint program_ = 0;
    GLuint vao_ = 0;
};

void log_heap(std::uint64_t frames)
{
    std::size_t live = 0;
    std::size_t peak = 0;
    std::size_t blocks = 0;
    std::size_t failures = 0;
    ppz_heap_stats(&live, &peak, &blocks, &failures);
    ppz::sys::log("[PPZ] heap frames=%llu live=%zu peak=%zu blocks=%zu failures=%zu",
                  static_cast<unsigned long long>(frames), live, peak, blocks, failures);
}

} // namespace

int main()
{
    using namespace ppz;
    sys::log("[PPZ] entry");

    ps5::Display display;
    if (!display.open())
    {
        sys::log("[PPZ] fatal: display open failed");
        sys::park();
    }
    TestPattern pattern;
    if (!pattern.init())
    {
        sys::log("[PPZ] fatal: test pattern init failed");
        sys::park();
    }

    const std::int64_t start = sys::monotonic_us();
    std::int64_t previous = start;
    std::uint64_t frames = 0;
    FrameStats stats;
    for (;;)
    {
        const std::int64_t now = sys::monotonic_us();
        pattern.draw(static_cast<double>(now - start) / 1e6, display.width(), display.height());
        if (!display.swap())
        {
            sys::log("[PPZ] fatal: swap failed frame=%llu error=%s",
                     static_cast<unsigned long long>(frames),
                     ps5::egl_error_name(display.last_error()));
            sys::park();
        }
        ++frames;
        const std::int64_t presented = sys::monotonic_us();
        if (frames == 1)
        {
            sys::log("[PPZ] first-swap ok");
            const bool hidden = sys::hide_splash_screen();
            sys::log("[PPZ] ready splash_hidden=%d", hidden ? 1 : 0);
            log_heap(frames);
        }
        else
        {
            stats.add(static_cast<double>(presented - previous) / 1000.0);
        }
        previous = presented;
        if (stats.count() == 600)
        {
            char summary[160];
            stats.format(summary, sizeof(summary));
            sys::log("[PPZ] %s", summary);
            stats.reset();
            if (frames % 3600 < 600)
                log_heap(frames);
        }
    }
}
