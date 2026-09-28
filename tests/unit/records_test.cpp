// ProsperoPuzzles - Personal record tests: codecs and library summaries.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/bytes.hpp"
#include "core/library.hpp"
#include "games/g2048/g2048_scene.hpp"
#include "games/records.hpp"
#include "games/registry.hpp"

#include <gtest/gtest.h>

using namespace ppz;
using namespace ppz::games;

TEST(Records, TimedStatsKeepTheFastestTimePerSize)
{
    TimedStats stats;
    EXPECT_TRUE(stats.record("Easy 9x9", 300));  // first time sets the record
    EXPECT_FALSE(stats.record("Easy 9x9", 310)); // slower: no record
    EXPECT_TRUE(stats.record("Easy 9x9", 250));
    EXPECT_TRUE(stats.record("Hard 9x9", 900));
    EXPECT_EQ(stats.best_for("Easy 9x9"), 250u);
    EXPECT_EQ(stats.best_for("Hard 9x9"), 900u);
    EXPECT_EQ(stats.best_for("Unknown"), 0u);

    stats.played = 7;
    stats.solved = 3;
    TimedStats back;
    ASSERT_TRUE(decode_timed(encode_timed(stats), &back));
    EXPECT_EQ(back.played, 7u);
    EXPECT_EQ(back.solved, 3u);
    EXPECT_EQ(back.best, stats.best);
    EXPECT_FALSE(decode_timed("junk", &back));
    EXPECT_FALSE(decode_timed(encode_timed(stats) + "x", &back));
}

TEST(Records, DescribeEachKindOfGame)
{
    // A Tatham puzzle with two bests.
    TimedStats timed;
    timed.played = 4;
    timed.solved = 2;
    timed.record("Easy 9x9", 83);
    timed.record("Hard 9x9", 402);
    Record solo = describe(*find("solo"), encode_timed(timed));
    EXPECT_TRUE(solo.beaten);
    EXPECT_NE(solo.line.find("Easy 9x9 1:23"), std::string::npos) << solo.line;
    EXPECT_NE(solo.line.find("Solved 2"), std::string::npos);

    // Played but never solved: not beaten, and it says so.
    TimedStats tried;
    tried.played = 1;
    Record net = describe(*find("net"), encode_timed(tried));
    EXPECT_FALSE(net.beaten);
    EXPECT_EQ(net.line, "Not solved yet");

    // 2048: best score, beaten once 2048 is reached.
    g2048::Stats s;
    s.best = 12340;
    s.played = 3;
    s.won = 1;
    s.highest = 11;
    Record g = describe(*find("g2048"), g2048::encode_stats(s));
    EXPECT_TRUE(g.beaten);
    EXPECT_NE(g.line.find("12,340"), std::string::npos) << g.line;
    EXPECT_NE(g.line.find("2,048"), std::string::npos);

    // A native puzzle: kit stats with a best on the middle size.
    bytes::Writer w;
    w.put<std::uint8_t>(1);
    w.put<std::uint32_t>(5); // played
    w.put<std::uint32_t>(2); // solved
    w.put<std::uint32_t>(0);
    w.put<std::uint32_t>(95);
    w.put<std::uint32_t>(0);
    w.put<std::uint8_t>(1);
    Record crowns = describe(*find("crowns"), w.data());
    EXPECT_TRUE(crowns.beaten);
    EXPECT_NE(crowns.line.find("1:35"), std::string::npos) << crowns.line;

    // Nothing saved yet.
    EXPECT_FALSE(describe(*find("crowns"), "").beaten);
    EXPECT_TRUE(describe(*find("crowns"), "").line.empty());
}

TEST(Records, LibraryRemembersCompletedGames)
{
    Library library(library_entries());
    EXPECT_FALSE(library.is_completed("solo"));
    library.set_completed("solo", true);
    EXPECT_TRUE(library.is_completed("solo"));
    library.set_completed("solo", false);
    EXPECT_FALSE(library.is_completed("solo"));
    library.set_completed("no-such-game", true); // ignored
}
