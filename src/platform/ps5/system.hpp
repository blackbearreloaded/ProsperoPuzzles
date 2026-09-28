// ProsperoPuzzles - PS5 system services: logging, clock, splash screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace ppz::sys
{

// Monotonic time in microseconds.
std::int64_t monotonic_us();

// Writes one line to the app log (stdout, redirected by runtime_shims.c) and
// mirrors it to klog so hardware runs can observe lifecycle markers live.
void log(const char *format, ...) __attribute__((format(printf, 1, 2)));

// Hides the system splash screen. Call once the first frame has been presented.
bool hide_splash_screen();

void sleep_us(std::uint32_t microseconds);

// Never returns: keeps the title alive until the shell closes it. Returning
// from main or calling exit() is not safe for a native title.
[[noreturn]] void park();

} // namespace ppz::sys
