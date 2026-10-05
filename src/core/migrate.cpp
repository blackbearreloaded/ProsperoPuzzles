// ProsperoPuzzles - One-time copy of the saves from an earlier data folder.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/migrate.hpp"

#include "core/save_file.hpp"

#include <sys/stat.h>

namespace ppz::save
{

namespace
{

constexpr const char *kMarker = "/migrated";

bool exists(const std::string &path)
{
    struct stat info
    {
    };
    return ::stat(path.c_str(), &info) == 0;
}

// 1 copied, 0 nothing to do, -1 failed.
int copy_file(const std::string &from, const std::string &to)
{
    if (!exists(from) || exists(to))
        return 0;
    std::string data;
    if (!read_file(from, &data))
        return -1;
    return write_atomic(to, data).empty() ? 1 : -1;
}

} // namespace

Migration migrate(const std::string &from, const std::string &to)
{
    Migration result;
    if (exists(to + kMarker) || !ensure_directory(to) || !ensure_directory(to + "/games"))
        return result;
    result.ran = true;
    const auto take = [&](const std::string &name)
    {
        const int copied = copy_file(from + "/" + name, to + "/" + name);
        result.copied += copied > 0 ? 1 : 0;
        result.failed += copied < 0 ? 1 : 0;
    };
    take("settings.bin");
    take("library.bin");
    for (const std::string &name : list_files(from + "/games"))
    {
        // Leftovers of an interrupted write are not saves.
        if (name.size() > 4 && name.compare(name.size() - 4, 4, ".tmp") == 0)
            continue;
        take("games/" + name);
    }
    if (result.failed == 0)
        (void)write_atomic(to + kMarker, "1\n");
    return result;
}

} // namespace ppz::save
