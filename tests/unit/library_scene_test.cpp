// ProsperoPuzzles - Library screen navigation tests (input through the tracker).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/input.hpp"
#include "core/library.hpp"
#include "games/registry.hpp"
#include "ui/library_scene.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace
{

ppz::PadSample pad(std::uint32_t buttons)
{
    ppz::PadSample s;
    s.buttons = buttons;
    s.connected = true;
    return s;
}

struct Harness
{
    ppz::Library library{ppz::games::library_entries()};
    ppz::ui::LibraryScene scene{library};
    ppz::InputTracker tracker;
    std::uint64_t now = 0;
    std::vector<ppz::audio::Cue> cues;

    void frame(std::uint32_t buttons)
    {
        const std::vector<ppz::PadSample> samples = {pad(buttons)};
        now += 16667;
        scene.update(tracker.update(samples, now), 1.0f / 60.0f, cues);
    }
    void tap(std::uint32_t button)
    {
        frame(button);
        frame(0);
    }
};

TEST(LibraryScene, DpadDownWalksEveryRowToTheEnd)
{
    Harness h;
    const std::string first = h.scene.focused_id();
    std::vector<std::string> seen = {first};
    const int count = static_cast<int>(h.library.items().size());
    const int rows = (count + 5) / 6; // six columns
    for (int i = 0; i < rows + 2; ++i)
    {
        h.tap(ppz::pad_bits::kDown);
        seen.push_back(h.scene.focused_id());
    }
    // Every move down lands on a new row until the last row, then it stops.
    const auto last = static_cast<std::size_t>(rows - 1);
    for (std::size_t i = 1; i <= last; ++i)
        EXPECT_NE(seen[i], seen[i - 1]) << "row " << i;
    EXPECT_EQ(seen[last + 1], seen[last]);
    // Column 0 of the last row, or the last game if that row is shorter.
    EXPECT_EQ(h.library.index_of(seen[last]), std::min(count - 1, static_cast<int>(last) * 6));
}

TEST(LibraryScene, HeldDownRepeats)
{
    Harness h;
    for (int i = 0; i < 90; ++i) // 1.5 s held
        h.frame(ppz::pad_bits::kDown);
    EXPECT_GE(h.library.index_of(h.scene.focused_id()), 36);
}

TEST(LibraryScene, DownWorksAcrossTheFavoritesSection)
{
    Harness h;
    h.library.toggle_favorite("net");
    h.library.toggle_favorite("solo");
    h.scene.refresh();
    h.scene.focus_game("solo");
    h.tap(ppz::pad_bits::kDown);
    EXPECT_EQ(h.library.index_of(h.scene.focused_id()), 3); // row 1, column 1
    h.tap(ppz::pad_bits::kDown);
    EXPECT_EQ(h.library.index_of(h.scene.focused_id()), 9);
    h.tap(ppz::pad_bits::kUp);
    h.tap(ppz::pad_bits::kUp);
    EXPECT_EQ(h.scene.focused_id(), "solo");
}

} // namespace
