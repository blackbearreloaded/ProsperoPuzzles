// ProsperoPuzzles - Library model: A-Z ordering, favorites, filters.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ppz
{

struct LibraryEntry
{
    std::string id;           // stable key: "g2048", "tenfold", "net", ...
    std::string display_name; // "2048", "Light Up", ...
    std::string tagline;
};

enum class LibraryFilter : std::uint8_t
{
    all,
    favorites,
    in_progress,
};

enum class Section : std::uint8_t
{
    favorites,
    all_games,
};

struct LibraryItem
{
    std::size_t entry = 0; // index into entries()
    Section section = Section::all_games;
};

// Orders games A-Z (case-insensitive, digits before letters, then by id),
// with favorites pinned first as their own A-Z section (PLAN.md D10).
class Library
{
  public:
    explicit Library(std::vector<LibraryEntry> entries);

    const std::vector<LibraryEntry> &entries() const
    {
        return entries_;
    }

    void set_filter(LibraryFilter filter);
    LibraryFilter filter() const
    {
        return filter_;
    }

    bool is_favorite(std::string_view id) const;
    // Returns the new favorite state.
    bool toggle_favorite(std::string_view id);
    void set_in_progress(std::string_view id, bool in_progress);
    bool is_in_progress(std::string_view id) const;

    // Visible items for the current filter, in display order.
    const std::vector<LibraryItem> &items() const
    {
        return items_;
    }
    // Index in items() of the given game, or -1 if not visible.
    int index_of(std::string_view id) const;

    // Letter-jump: index of the first item whose name starts after (or
    // before) the current item's initial, within the same section. Returns
    // current when there is no further group.
    int next_letter(int current) const;
    int previous_letter(int current) const;
    static char initial(std::string_view name); // 'A'..'Z', or '#' for digits/other

    // Persistence of favorites and the last focused game.
    std::string encode(std::string_view last_focused) const;
    // Returns false on malformed data; unknown ids are ignored.
    bool decode(std::string_view data, std::string *last_focused);

    static bool name_less(const LibraryEntry &a, const LibraryEntry &b);

  private:
    void rebuild();

    std::vector<LibraryEntry> entries_;
    std::vector<std::size_t> sorted_; // entries_ indices, A-Z
    std::vector<bool> favorite_;
    std::vector<bool> in_progress_;
    LibraryFilter filter_ = LibraryFilter::all;
    std::vector<LibraryItem> items_;
};

} // namespace ppz
