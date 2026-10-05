// ProsperoPuzzles - Save migration tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/migrate.hpp"
#include "core/save_file.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace
{

struct Folders
{
    std::string from;
    std::string to;
};

Folders folders()
{
    char directory[] = "/tmp/ppz-migrate-XXXXXX";
    EXPECT_NE(mkdtemp(directory), nullptr);
    return {std::string(directory) + "/sandbox", std::string(directory) + "/data"};
}

void put(const std::string &path, const std::string &text)
{
    ASSERT_EQ(ppz::save::write_atomic(path, text), "") << path;
}

std::string get(const std::string &path)
{
    std::string text;
    return ppz::save::read_file(path, &text) ? text : "<missing>";
}

TEST(Migrate, CopiesSettingsLibraryAndGamesOnce)
{
    const Folders f = folders();
    ASSERT_TRUE(ppz::save::ensure_directory(f.from));
    ASSERT_TRUE(ppz::save::ensure_directory(f.from + "/games"));
    put(f.from + "/settings.bin", "settings");
    put(f.from + "/library.bin", "library");
    put(f.from + "/games/net.sav", "net save");
    put(f.from + "/games/net.stats", "net stats");
    put(f.from + "/games/mines.sav.tmp", "half written");
    put(f.from + "/app.log", "a log is not a save");

    const auto first = ppz::save::migrate(f.from, f.to);
    EXPECT_TRUE(first.ran);
    EXPECT_EQ(first.copied, 4);
    EXPECT_EQ(first.failed, 0);
    EXPECT_EQ(get(f.to + "/settings.bin"), "settings");
    EXPECT_EQ(get(f.to + "/library.bin"), "library");
    EXPECT_EQ(get(f.to + "/games/net.sav"), "net save");
    EXPECT_EQ(get(f.to + "/games/net.stats"), "net stats");
    EXPECT_EQ(get(f.to + "/games/mines.sav.tmp"), "<missing>");
    EXPECT_EQ(get(f.to + "/app.log"), "<missing>");
    // The sandbox copy stays, for an older version of the app.
    EXPECT_EQ(get(f.from + "/games/net.sav"), "net save");

    // A finished game's save is removed; the next start must not bring it back.
    ASSERT_EQ(::unlink((f.to + "/games/net.sav").c_str()), 0);
    const auto second = ppz::save::migrate(f.from, f.to);
    EXPECT_FALSE(second.ran);
    EXPECT_EQ(second.copied, 0);
    EXPECT_EQ(get(f.to + "/games/net.sav"), "<missing>");
}

TEST(Migrate, KeepsWhatTheNewFolderAlreadyHas)
{
    const Folders f = folders();
    ASSERT_TRUE(ppz::save::ensure_directory(f.from));
    ASSERT_TRUE(ppz::save::ensure_directory(f.to));
    put(f.from + "/settings.bin", "old");
    put(f.from + "/library.bin", "library");
    put(f.to + "/settings.bin", "new");

    const auto result = ppz::save::migrate(f.from, f.to);
    EXPECT_TRUE(result.ran);
    EXPECT_EQ(result.copied, 1);
    EXPECT_EQ(get(f.to + "/settings.bin"), "new");
    EXPECT_EQ(get(f.to + "/library.bin"), "library");
}

TEST(Migrate, NothingToBringIsDoneOnceToo)
{
    const Folders f = folders();
    const auto first = ppz::save::migrate(f.from, f.to);
    EXPECT_TRUE(first.ran);
    EXPECT_EQ(first.copied, 0);
    EXPECT_EQ(first.failed, 0);
    EXPECT_FALSE(ppz::save::migrate(f.from, f.to).ran);
}

TEST(Migrate, AFailedCopyIsTriedAgain)
{
    if (::geteuid() == 0)
        GTEST_SKIP() << "root reads every file";
    const Folders f = folders();
    ASSERT_TRUE(ppz::save::ensure_directory(f.from));
    put(f.from + "/settings.bin", "settings");
    put(f.from + "/library.bin", "library");
    ASSERT_EQ(::chmod((f.from + "/library.bin").c_str(), 0), 0);

    const auto first = ppz::save::migrate(f.from, f.to);
    EXPECT_TRUE(first.ran);
    EXPECT_EQ(first.copied, 1);
    EXPECT_EQ(first.failed, 1);

    ASSERT_EQ(::chmod((f.from + "/library.bin").c_str(), 0644), 0);
    const auto second = ppz::save::migrate(f.from, f.to);
    EXPECT_TRUE(second.ran);
    EXPECT_EQ(second.copied, 1);
    EXPECT_EQ(second.failed, 0);
    EXPECT_EQ(get(f.to + "/library.bin"), "library");
    EXPECT_FALSE(ppz::save::migrate(f.from, f.to).ran);
}

} // namespace
