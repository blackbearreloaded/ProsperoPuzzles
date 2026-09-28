// ProsperoPuzzles - Player settings and their save format.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/settings.hpp"

#include "core/bytes.hpp"

#include <algorithm>

namespace ppz
{

namespace
{
// Version 2 appends the display resolution; version 1 saves still load.
constexpr std::uint8_t kVersion = 2;
} // namespace

std::string encode_settings(const Settings &settings)
{
    bytes::Writer w;
    w.put(kVersion);
    w.put(static_cast<std::uint8_t>(settings.music_volume));
    w.put(static_cast<std::uint8_t>(settings.sfx_volume));
    w.put(static_cast<std::uint8_t>(settings.ui_volume));
    w.put_bool(settings.reduced_motion);
    w.put_bool(settings.swap_confirm);
    w.put_bool(settings.show_fps);
    w.put(static_cast<std::uint8_t>(settings.resolution));
    return w.data();
}

bool decode_settings(std::string_view data, Settings *settings)
{
    bytes::Reader r(data);
    const std::uint8_t version = r.get<std::uint8_t>();
    if (version != 1 && version != kVersion)
        return false;
    Settings s;
    s.music_volume = std::clamp<int>(r.get<std::uint8_t>(), 0, 10);
    s.sfx_volume = std::clamp<int>(r.get<std::uint8_t>(), 0, 10);
    s.ui_volume = std::clamp<int>(r.get<std::uint8_t>(), 0, 10);
    s.reduced_motion = r.get_bool();
    s.swap_confirm = r.get_bool();
    s.show_fps = r.get_bool();
    if (version >= 2)
        s.resolution = std::clamp<int>(r.get<std::uint8_t>(), 0, Settings::kResolutionCount - 1);
    if (!r.finished())
        return false;
    *settings = s;
    return true;
}

} // namespace ppz
