// ProsperoPuzzles - Player settings and their save format.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace ppz
{

struct Settings
{
    int music_volume = 6; // 0..10
    int sfx_volume = 8;   // 0..10
    int ui_volume = 7;    // 0..10
    bool reduced_motion = false;
    bool swap_confirm = false; // Circle confirms, Cross goes back
    bool show_fps = true;
    // Display resolution, an index into kResolutions (1080p, 1440p, 4K).
    int resolution = 0;

    struct Resolution
    {
        int width;
        int height;
        const char *label;
    };
    static constexpr int kResolutionCount = 3;
    static constexpr Resolution kResolutions[kResolutionCount] = {
        {1920, 1080, "1080p"}, {2560, 1440, "1440p"}, {3840, 2160, "4K"}};

    static float gain(int volume)
    {
        // Perceptual curve: 0..10 steps map to roughly -40..0 dB.
        const float t = static_cast<float>(volume) / 10.0f;
        return t * t;
    }
};

std::string encode_settings(const Settings &settings);
// Accepts any known version; out-of-range values are clamped. Returns false
// (leaving settings untouched) for malformed data.
bool decode_settings(std::string_view data, Settings *settings);

} // namespace ppz
