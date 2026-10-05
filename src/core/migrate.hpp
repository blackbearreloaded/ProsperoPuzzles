// ProsperoPuzzles - One-time copy of the saves from an earlier data folder.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>

namespace ppz::save
{

struct Migration
{
    bool ran = false; // false: this folder was migrated (or looked at) before
    int copied = 0;   // files copied
    int failed = 0;   // files that could not be copied
};

// Copies settings.bin, library.bin and games/* from `from` into `to` when
// `to` has never been looked at before. Files already in `to` are kept, the
// ones in `from` stay where they are, and a marker in `to` records that it
// was done so a save removed later does not come back. A copy that failed is
// tried again on the next call.
Migration migrate(const std::string &from, const std::string &to);

} // namespace ppz::save
