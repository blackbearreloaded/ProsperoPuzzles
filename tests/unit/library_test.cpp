// ProsperoPuzzles - Library ordering, favorites and filter tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/library.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{

using ppz::Library;
using ppz::LibraryEntry;
using ppz::LibraryFilter;
using ppz::Section;

Library make_library()
{
    return Library({{"tenfold", "Tenfold", ""},
                    {"net", "Net", ""},
                    {"g2048", "2048", ""},
                    {"lightup", "Light Up", ""},
                    {"blackbox", "Black Box", ""},
                    {"netslide", "Netslide", ""},
                    {"loopy", "Loopy", ""},
                    {"bridges", "bridges", ""}}); // lowercase to prove case-insensitivity
}

std::vector<std::string> ids(const Library &library)
{
    std::vector<std::string> out;
    for (const auto &item : library.items())
        out.push_back(library.entries()[item.entry].id);
    return out;
}

TEST(Library, SortsAlphabeticallyWithDigitsFirstIgnoringCase)
{
    const Library library = make_library();
    EXPECT_EQ(ids(library), (std::vector<std::string>{"g2048", "blackbox", "bridges", "lightup",
                                                      "loopy", "net", "netslide", "tenfold"}));
}

TEST(Library, FavoritesArePinnedFirstInTheirOwnAzSection)
{
    Library library = make_library();
    EXPECT_TRUE(library.toggle_favorite("tenfold"));
    EXPECT_TRUE(library.toggle_favorite("lightup"));
    EXPECT_EQ(ids(library), (std::vector<std::string>{"lightup", "tenfold", "g2048", "blackbox",
                                                      "bridges", "loopy", "net", "netslide"}));
    EXPECT_EQ(library.items()[0].section, Section::favorites);
    EXPECT_EQ(library.items()[2].section, Section::all_games);
    EXPECT_FALSE(library.toggle_favorite("lightup"));
    EXPECT_EQ(library.index_of("lightup"), 4); // after pinned Tenfold, 2048, Black Box, bridges
    EXPECT_FALSE(library.toggle_favorite("unknown"));
}

TEST(Library, FiltersFavoritesAndInProgress)
{
    Library library = make_library();
    library.toggle_favorite("net");
    library.set_in_progress("loopy", true);
    library.set_in_progress("g2048", true);
    library.set_filter(LibraryFilter::favorites);
    EXPECT_EQ(ids(library), std::vector<std::string>{"net"});
    library.set_filter(LibraryFilter::in_progress);
    EXPECT_EQ(ids(library), (std::vector<std::string>{"g2048", "loopy"}));
    library.set_in_progress("loopy", false);
    EXPECT_EQ(ids(library), std::vector<std::string>{"g2048"});
    EXPECT_EQ(library.index_of("net"), -1);
}

TEST(Library, LetterJumpsMoveBetweenInitialGroups)
{
    const Library library = make_library();
    // # (2048) | B (Black Box, bridges) | L (Light Up, Loopy) | N (Net, Netslide) | T
    EXPECT_EQ(library.next_letter(0), 1);
    EXPECT_EQ(library.next_letter(1), 3);
    EXPECT_EQ(library.next_letter(3), 5);
    EXPECT_EQ(library.next_letter(7), 7);
    EXPECT_EQ(library.previous_letter(6), 5); // start of its own group first
    EXPECT_EQ(library.previous_letter(5), 3);
    EXPECT_EQ(library.previous_letter(1), 0);
    EXPECT_EQ(library.previous_letter(0), 0);
    EXPECT_EQ(Library::initial("2048"), '#');
    EXPECT_EQ(Library::initial("light up"), 'L');
}

TEST(Library, PersistsFavoritesAndLastFocus)
{
    Library library = make_library();
    library.toggle_favorite("net");
    library.toggle_favorite("g2048");
    const std::string data = library.encode("loopy");

    Library restored = make_library();
    std::string focused;
    ASSERT_TRUE(restored.decode(data, &focused));
    EXPECT_EQ(focused, "loopy");
    EXPECT_TRUE(restored.is_favorite("net"));
    EXPECT_TRUE(restored.is_favorite("g2048"));
    EXPECT_FALSE(restored.is_favorite("tenfold"));
    EXPECT_EQ(ids(restored).front(), "g2048");

    EXPECT_TRUE(restored.decode("f removed-game\n", &focused)); // unknown ids ignored
    EXPECT_FALSE(restored.is_favorite("net"));
    EXPECT_FALSE(restored.decode("garbage", &focused));
}

} // namespace
