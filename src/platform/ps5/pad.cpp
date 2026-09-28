// ProsperoPuzzles - DualSense input through scePad.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "platform/ps5/pad.hpp"

#include "platform/ps5/system.hpp"

#include <cstddef>
#include <cstdint>

namespace
{

// scePadRead sample layout (120 bytes), as typed in ProsperoLight.
struct RawPadSample
{
    std::uint32_t buttons;
    std::uint8_t left_x;
    std::uint8_t left_y;
    std::uint8_t right_x;
    std::uint8_t right_y;
    std::uint8_t l2;
    std::uint8_t r2;
    std::uint8_t reserved0[66];
    std::int32_t connected;
    std::uint64_t timestamp_us;
    std::uint8_t extension[16];
    std::uint8_t connected_count;
    std::uint8_t reserved1[15];
};
static_assert(sizeof(RawPadSample) == 120);
static_assert(offsetof(RawPadSample, connected) == 0x4c);
static_assert(offsetof(RawPadSample, timestamp_us) == 0x50);
static_assert(offsetof(RawPadSample, connected_count) == 0x68);

constexpr int kAlreadyInitialized = static_cast<int>(0x80960003);

} // namespace

extern "C"
{
    int sceUserServiceInitialize(const void *params);
    int sceUserServiceTerminate(void);
    int sceUserServiceGetInitialUser(int *user);
    int scePadInit(void);
    int scePadOpen(int user, int type, int index, const void *parameters);
    int scePadRead(int handle, RawPadSample *samples, int count);
    int scePadClose(int handle);
}

namespace ppz::ps5
{

Pad::~Pad()
{
    close();
}

bool Pad::open()
{
    const int init = sceUserServiceInitialize(nullptr);
    owns_user_service_ = init == 0;
    if (init != 0 && init != kAlreadyInitialized)
        sys::log("[PPZ] pad sceUserServiceInitialize=0x%08x", static_cast<unsigned>(init));
    if (sceUserServiceGetInitialUser(&user_) != 0)
    {
        sys::log("[PPZ] pad no initial user");
        return false;
    }
    const int pad_init = scePadInit();
    if (pad_init != 0)
        sys::log("[PPZ] pad scePadInit=0x%08x", static_cast<unsigned>(pad_init));
    // The pad service can lag behind title start; retry briefly.
    for (int attempt = 0; attempt < 20; ++attempt)
    {
        handle_ = scePadOpen(user_, 0, 0, nullptr);
        if (handle_ >= 0)
        {
            sys::log("[PPZ] pad open user=0x%08x handle=%d attempts=%d",
                     static_cast<unsigned>(user_), handle_, attempt + 1);
            return true;
        }
        sys::sleep_us(50000);
    }
    sys::log("[PPZ] pad scePadOpen failed=0x%08x", static_cast<unsigned>(handle_));
    handle_ = -1;
    return false;
}

void Pad::close()
{
    if (handle_ >= 0)
        scePadClose(handle_);
    handle_ = -1;
    if (owns_user_service_)
        sceUserServiceTerminate();
    owns_user_service_ = false;
}

std::size_t Pad::read(std::span<PadSample> out)
{
    if (out.empty())
        return 0;
    if (handle_ < 0)
    {
        out[0] = PadSample{};
        return 1;
    }
    RawPadSample raw[64];
    const int limit = static_cast<int>(out.size() < 64 ? out.size() : 64);
    const int count = scePadRead(handle_, raw, limit);
    if (count <= 0)
        return 0;
    for (int index = 0; index < count; ++index)
    {
        const RawPadSample &r = raw[index];
        PadSample &s = out[static_cast<std::size_t>(index)];
        s.buttons = r.buttons;
        s.left_x = r.left_x;
        s.left_y = r.left_y;
        s.right_x = r.right_x;
        s.right_y = r.right_y;
        s.l2 = r.l2;
        s.r2 = r.r2;
        s.connected = r.connected != 0;
        s.timestamp_us = r.timestamp_us;
    }
    return static_cast<std::size_t>(count);
}

} // namespace ppz::ps5
