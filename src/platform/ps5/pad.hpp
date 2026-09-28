// ProsperoPuzzles - DualSense input through scePad.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/input.hpp"

#include <cstddef>
#include <span>

namespace ppz::ps5
{

// Reads every buffered sample of the initial user's controller each frame
// (scePadRead batches up to 64), so short taps survive long frames. Follows
// ProsperoLight's radio_input.cpp.
class Pad
{
  public:
    Pad() = default;
    Pad(const Pad &) = delete;
    Pad &operator=(const Pad &) = delete;
    ~Pad();

    bool open();
    void close();
    // Fills out with the samples read since the last call; returns the count.
    // Returns one disconnected sample if the pad is not open.
    std::size_t read(std::span<PadSample> out);
    bool is_open() const
    {
        return handle_ >= 0;
    }

  private:
    int handle_ = -1;
    int user_ = -1;
    bool owns_user_service_ = false;
};

} // namespace ppz::ps5
