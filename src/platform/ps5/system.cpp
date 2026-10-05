// ProsperoPuzzles - PS5 system services: logging, clock, splash screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "platform/ps5/system.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>

extern "C"
{
    int sceKernelDebugOutText(int channel, const char *text);
    int sceKernelUsleep(unsigned int microseconds);
    int sceSystemServiceHideSplashScreen(void);
    int sceSystemServiceLoadExec(const char *path, const char **arguments);
    void ppz_open_log(const char *directory);
}

namespace ppz::sys
{

namespace
{

// Lines logged before the log file is open (filesystem access decides where
// it is) wait here; klog has them at once.
bool log_open = false;
char early[4096];
std::size_t early_used = 0;

} // namespace

std::int64_t monotonic_us()
{
    timespec now{};
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return 0;
    return static_cast<std::int64_t>(now.tv_sec) * 1000000 + now.tv_nsec / 1000;
}

void log(const char *format, ...)
{
    // klog lines are short; longer messages are truncated rather than split.
    char line[384];
    va_list arguments;
    va_start(arguments, format);
    int length = std::vsnprintf(line, sizeof(line) - 1, format, arguments);
    va_end(arguments);
    if (length < 0)
        return;
    std::size_t used = std::strlen(line);
    line[used] = '\n';
    line[used + 1] = '\0';
    sceKernelDebugOutText(0, line);
    if (log_open)
    {
        std::fputs(line, stdout);
        return;
    }
    used = std::strlen(line);
    if (used < sizeof(early) - early_used)
    {
        std::memcpy(early + early_used, line, used);
        early_used += used;
    }
}

void open_log(const char *directory)
{
    ppz_open_log(directory);
    log_open = true;
    std::fwrite(early, 1, early_used, stdout);
    early_used = 0;
}

bool hide_splash_screen()
{
    return sceSystemServiceHideSplashScreen() == 0;
}

void sleep_us(std::uint32_t microseconds)
{
    sceKernelUsleep(microseconds);
}

void park()
{
    std::fflush(nullptr);
    for (;;)
        sceKernelUsleep(100000);
}

void exit_app()
{
    std::fflush(nullptr);
    sceSystemServiceLoadExec("exit", nullptr);
    park();
}

} // namespace ppz::sys
