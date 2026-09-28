// ProsperoPuzzles - Library model: A-Z ordering, favorites, filters.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/library.hpp"

#include <algorithm>
#include <cctype>
#include <numeric>

namespace ppz
{

namespace
{

// Digits sort before letters; otherwise case-insensitive byte order.
int collate_key(char c)
{
    const auto u = static_cast<unsigned char>(c);
    if (std::isdigit(u) != 0)
        return u - '0';
    if (std::isalpha(u) != 0)
        return 100 + std::tolower(u);
    return 50 + u;
}

std::size_t find_entry(const std::vector<LibraryEntry> &entries, std::string_view id)
{
    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        if (entries[i].id == id)
            return i;
    }
    return entries.size();
}

} // namespace

bool Library::name_less(const LibraryEntry &a, const LibraryEntry &b)
{
    const std::string &x = a.display_name;
    const std::string &y = b.display_name;
    const std::size_t n = std::min(x.size(), y.size());
    for (std::size_t i = 0; i < n; ++i)
    {
        const int kx = collate_key(x[i]);
        const int ky = collate_key(y[i]);
        if (kx != ky)
            return kx < ky;
    }
    if (x.size() != y.size())
        return x.size() < y.size();
    return a.id < b.id;
}

char Library::initial(std::string_view name)
{
    if (name.empty())
        return '#';
    const auto u = static_cast<unsigned char>(name.front());
    return std::isalpha(u) != 0 ? static_cast<char>(std::toupper(u)) : '#';
}

Library::Library(std::vector<LibraryEntry> entries)
    : entries_(std::move(entries)), favorite_(entries_.size(), false),
      in_progress_(entries_.size(), false)
{
    sorted_.resize(entries_.size());
    std::iota(sorted_.begin(), sorted_.end(), 0);
    std::sort(sorted_.begin(), sorted_.end(),
              [this](std::size_t a, std::size_t b) { return name_less(entries_[a], entries_[b]); });
    rebuild();
}

void Library::rebuild()
{
    items_.clear();
    if (filter_ != LibraryFilter::in_progress)
    {
        for (std::size_t index : sorted_)
        {
            if (favorite_[index])
                items_.push_back({index, Section::favorites});
        }
    }
    if (filter_ == LibraryFilter::favorites)
        return;
    for (std::size_t index : sorted_)
    {
        if (filter_ == LibraryFilter::all && favorite_[index])
            continue; // already listed in the favorites section
        if (filter_ == LibraryFilter::in_progress && !in_progress_[index])
            continue;
        items_.push_back({index, Section::all_games});
    }
}

void Library::set_filter(LibraryFilter filter)
{
    filter_ = filter;
    rebuild();
}

bool Library::is_favorite(std::string_view id) const
{
    const std::size_t index = find_entry(entries_, id);
    return index < entries_.size() && favorite_[index];
}

bool Library::toggle_favorite(std::string_view id)
{
    const std::size_t index = find_entry(entries_, id);
    if (index >= entries_.size())
        return false;
    favorite_[index] = !favorite_[index];
    rebuild();
    return favorite_[index];
}

void Library::set_in_progress(std::string_view id, bool in_progress)
{
    const std::size_t index = find_entry(entries_, id);
    if (index >= entries_.size() || in_progress_[index] == in_progress)
        return;
    in_progress_[index] = in_progress;
    if (filter_ == LibraryFilter::in_progress)
        rebuild();
}

bool Library::is_in_progress(std::string_view id) const
{
    const std::size_t index = find_entry(entries_, id);
    return index < entries_.size() && in_progress_[index];
}

int Library::index_of(std::string_view id) const
{
    for (std::size_t i = 0; i < items_.size(); ++i)
    {
        if (entries_[items_[i].entry].id == id)
            return static_cast<int>(i);
    }
    return -1;
}

int Library::next_letter(int current) const
{
    if (current < 0 || current >= static_cast<int>(items_.size()))
        return current;
    const LibraryItem &here = items_[static_cast<std::size_t>(current)];
    const char letter = initial(entries_[here.entry].display_name);
    for (std::size_t i = static_cast<std::size_t>(current) + 1; i < items_.size(); ++i)
    {
        if (items_[i].section != here.section ||
            initial(entries_[items_[i].entry].display_name) != letter)
            return static_cast<int>(i);
    }
    return current;
}

int Library::previous_letter(int current) const
{
    if (current <= 0 || current >= static_cast<int>(items_.size()))
        return current;
    // Step to the start of the previous group, then to that group's first item.
    std::size_t i = static_cast<std::size_t>(current);
    const auto group_of = [this](std::size_t k)
    { return std::make_pair(items_[k].section, initial(entries_[items_[k].entry].display_name)); };
    const auto here = group_of(i);
    while (i > 0 && group_of(i - 1) == here)
        --i;
    if (i != static_cast<std::size_t>(current))
        return static_cast<int>(i); // first press: jump to the start of this group
    if (i == 0)
        return current;
    const auto previous = group_of(i - 1);
    --i;
    while (i > 0 && group_of(i - 1) == previous)
        --i;
    return static_cast<int>(i);
}

std::string Library::encode(std::string_view last_focused) const
{
    // Text, one record per line: "f <id>" favorites, "l <id>" last focused.
    std::string out;
    for (std::size_t index : sorted_)
    {
        if (favorite_[index])
            out += "f " + entries_[index].id + "\n";
    }
    if (!last_focused.empty())
        out += "l " + std::string(last_focused) + "\n";
    return out;
}

bool Library::decode(std::string_view data, std::string *last_focused)
{
    std::fill(favorite_.begin(), favorite_.end(), false);
    last_focused->clear();
    std::size_t start = 0;
    bool ok = true;
    while (start < data.size())
    {
        std::size_t end = data.find('\n', start);
        if (end == std::string_view::npos)
            end = data.size();
        const std::string_view line = data.substr(start, end - start);
        start = end + 1;
        if (line.empty())
            continue;
        if (line.size() < 3 || line[1] != ' ')
        {
            ok = false;
            continue;
        }
        const std::string_view id = line.substr(2);
        if (line[0] == 'f')
        {
            const std::size_t index = find_entry(entries_, id);
            if (index < entries_.size())
                favorite_[index] = true;
        }
        else if (line[0] == 'l')
        {
            last_focused->assign(id);
        }
        else
        {
            ok = false;
        }
    }
    rebuild();
    return ok;
}

} // namespace ppz
