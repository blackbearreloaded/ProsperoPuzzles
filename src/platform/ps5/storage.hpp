// ProsperoPuzzles - Filesystem access and where the app keeps its files.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>

namespace ppz::ps5
{

// With filesystem access (platform/ps5/elevation.hpp) the app uses real
// console paths: settings, saves and the log live in /data/prosperopuzzles,
// where they outlast the app and can be reached over FTP, and the app's own
// folder is wherever the console mounted it from. Without it (no resident
// Lapy service and no payload loader, or the request was refused) the sandbox
// paths stay: /app0 and /download0/prosperopuzzles.
struct Storage
{
    int access = -1;            // 0 granted, otherwise the elevation::Status that refused it
    const char *route = "none"; // existing, resident, helper or none
    std::string title_id;       // from the packaged param.json
    std::string version;        // its contentVersion
    std::string app_dir;        // the app's own files (eboot.bin, assets/, sce_sys/)
    std::string data_root;      // settings.bin, library.bin, games/, app.log
    std::string previous_root;  // the sandbox data folder, when data_root is not it
    bool granted() const
    {
        return access == 0;
    }
};

// Requests filesystem access and settles every path. Call it first thing in
// main, before any thread exists: upstream Lapy accepts only a single-threaded
// process.
const Storage &prepare_storage();

// The paths prepare_storage() settled.
const Storage &storage();

} // namespace ppz::ps5
