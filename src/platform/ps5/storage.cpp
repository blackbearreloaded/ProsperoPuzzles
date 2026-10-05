// ProsperoPuzzles - Filesystem access and where the app keeps its files.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "platform/ps5/storage.hpp"

#include "core/save_file.hpp"
#include "core/version.hpp"
#include "platform/ps5/elevation.hpp"
#include "update/ppz_paths.h"

#include <sys/stat.h>
#include <unistd.h>

namespace ppz::ps5
{

namespace
{

constexpr const char *kSandboxApp = "/app0";
constexpr const char *kSandboxData = "/download0/prosperopuzzles";
constexpr const char *kData = "/data/prosperopuzzles";

Storage settled;

bool is_file(const std::string &path)
{
    struct stat info
    {
    };
    return ::stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
}

bool is_directory(const std::string &path)
{
    struct stat info
    {
    };
    return ::stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

// The app's folder once the sandbox root is gone: /app0 while it is still
// there, else the console's mount of the running app (whatever it was mounted
// from: /data, an external drive, a USB drive), else the usual install folder.
std::string find_app_dir(const std::string &title)
{
    const std::string candidates[] = {kSandboxApp, "/system_ex/app/" + title,
                                      "/data/homebrew/" + title,
                                      "/mnt/sandbox/" + title + "_000/app0"};
    for (const std::string &candidate : candidates)
    {
        if (is_file(candidate + "/eboot.bin"))
            return candidate;
    }
    return kSandboxApp;
}

} // namespace

const Storage &prepare_storage()
{
    // The packaged param.json names the title; read it while /app0 is certain.
    std::string param;
    if (save::read_file(std::string(kSandboxApp) + "/sce_sys/param.json", &param))
    {
        settled.title_id = title_id(param);
        settled.version = content_version(param);
    }
    settled.access = static_cast<int>(elevation::request(elevation::Capability::filesystem));
    settled.route = elevation::path();
    // Elevation leaves the effective group apart from the real one; match
    // them, as files made from here on should belong to one group.
    if (settled.granted() && getegid() != getgid())
        (void)setegid(getgid());
    settled.app_dir = kSandboxApp;
    settled.data_root = kSandboxData;
    if (settled.granted() && !settled.title_id.empty())
    {
        settled.app_dir = find_app_dir(settled.title_id);
        // The folder is made here so that a console where it can't be falls
        // back to the sandbox instead of losing every save.
        ::mkdir(kData, 0777);
        if (is_directory(kData))
        {
            ::chmod(kData, 0777);
            settled.data_root = kData;
            const std::string sandbox =
                "/mnt/sandbox/" + settled.title_id + "_000" + std::string(kSandboxData);
            settled.previous_root = is_directory(kSandboxData) ? kSandboxData : sandbox;
        }
    }
    return settled;
}

const Storage &storage()
{
    return settled;
}

} // namespace ppz::ps5

extern "C" const char *ppz_self_update_path(int which)
{
    static const std::string helper = ppz::ps5::storage().app_dir + "/self-updater.elf";
    static const std::string param = ppz::ps5::storage().app_dir + "/sce_sys/param.json";
    static const std::string sequence = ppz::ps5::storage().data_root + "/self-update-sequence";
    return which == 0 ? helper.c_str() : which == 1 ? param.c_str() : sequence.c_str();
}
