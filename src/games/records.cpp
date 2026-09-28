// ProsperoPuzzles - Personal records: best times and scores, read back for the library.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/records.hpp"

#include "core/bytes.hpp"
#include "games/g2048/g2048_scene.hpp"
#include "games/kit/puzzle_scene.hpp"
#include "games/tenfold/tenfold_scene.hpp"

#include <cstdio>

namespace ppz::games
{

namespace
{

constexpr std::uint8_t kTimedVersion = 1;
constexpr std::size_t kMaxSizes = 64;
constexpr const char *kDot = "  \xC2\xB7  ";

std::string thousands(std::uint64_t value)
{
    const std::string raw = std::to_string(value);
    std::string out;
    for (std::size_t i = 0; i < raw.size(); ++i)
    {
        if (i != 0 && (raw.size() - i) % 3 == 0)
            out += ',';
        out += raw[i];
    }
    return out;
}

std::string solved_text(std::uint32_t solved)
{
    return "Solved " + std::to_string(solved);
}

} // namespace

std::string clock_text(std::uint32_t seconds)
{
    char text[24];
    std::snprintf(text, sizeof(text), "%u:%02u", seconds / 60, seconds % 60);
    return text;
}

bool TimedStats::record(const std::string &size, std::uint32_t seconds)
{
    seconds = std::max<std::uint32_t>(1, seconds);
    for (auto &[label, best] : this->best)
    {
        if (label == size)
        {
            if (seconds < best)
            {
                best = seconds;
                return true;
            }
            return false;
        }
    }
    if (this->best.size() < kMaxSizes)
        this->best.emplace_back(size, seconds);
    return true;
}

std::uint32_t TimedStats::best_for(const std::string &size) const
{
    for (const auto &[label, seconds] : best)
        if (label == size)
            return seconds;
    return 0;
}

std::string encode_timed(const TimedStats &stats)
{
    bytes::Writer w;
    w.put(kTimedVersion);
    w.put(stats.played);
    w.put(stats.solved);
    w.put(static_cast<std::uint16_t>(stats.best.size()));
    for (const auto &[label, seconds] : stats.best)
    {
        w.put(static_cast<std::uint8_t>(std::min<std::size_t>(label.size(), 255)));
        for (std::size_t i = 0; i < label.size() && i < 255; ++i)
            w.put(static_cast<std::uint8_t>(label[i]));
        w.put(seconds);
    }
    return w.data();
}

bool decode_timed(std::string_view data, TimedStats *stats)
{
    bytes::Reader r(data);
    if (r.get<std::uint8_t>() != kTimedVersion)
        return false;
    TimedStats s;
    s.played = r.get<std::uint32_t>();
    s.solved = r.get<std::uint32_t>();
    const std::uint16_t count = r.get<std::uint16_t>();
    if (count > kMaxSizes)
        return false;
    for (std::uint16_t i = 0; i < count && r.ok(); ++i)
    {
        const std::uint8_t length = r.get<std::uint8_t>();
        std::string label;
        for (std::uint8_t k = 0; k < length && r.ok(); ++k)
            label.push_back(static_cast<char>(r.get<std::uint8_t>()));
        s.best.emplace_back(std::move(label), r.get<std::uint32_t>());
    }
    if (!r.finished())
        return false;
    *stats = std::move(s);
    return true;
}

Record describe(const GameInfo &game, std::string_view stats)
{
    Record record;
    if (stats.empty())
        return record;
    switch (game.kind)
    {
    case Kind::g2048:
    {
        g2048::Stats s;
        if (!g2048::decode_stats(stats, &s) || s.played == 0)
            break;
        record.beaten = s.won > 0;
        record.line = "Best score " + thousands(s.best);
        if (s.highest > 0)
            record.line += kDot + std::string("Best tile ") + thousands(1ull << s.highest);
        break;
    }
    case Kind::tenfold:
    {
        tenfold::Stats s;
        if (!tenfold::decode_stats(stats, &s) || (s.played == 0 && s.best == 0))
            break;
        record.beaten = s.reached_ten > 0;
        record.line = "Best score " + thousands(s.best);
        if (s.reached_ten > 0)
            record.line += kDot + std::string("Made ten ") + std::to_string(s.reached_ten) +
                           (s.reached_ten == 1 ? " time" : " times");
        break;
    }
    case Kind::native:
    {
        kit::PuzzleStats s;
        int last_size = 0;
        if (!kit::decode_stats(stats, &s, &last_size) || game.create == nullptr)
            break;
        record.beaten = s.solved > 0;
        if (s.solved == 0)
        {
            if (s.played > 0)
                record.line = "Not solved yet";
            break;
        }
        // Size names come from the game itself.
        const auto scene = game.create(ui::Fonts{});
        const kit::PuzzleScene *puzzle = scene->as_puzzle();
        for (int size = 0; size < 3; ++size)
        {
            const std::uint32_t best = s.best_seconds[static_cast<std::size_t>(size)];
            if (best == 0)
                continue;
            if (!record.line.empty())
                record.line += kDot;
            record.line += std::string(puzzle != nullptr ? puzzle->size_label(size) : "") + " " +
                           clock_text(best);
        }
        record.line = "Best " + record.line + kDot + solved_text(s.solved);
        break;
    }
    case Kind::sgt:
    {
        TimedStats s;
        if (!decode_timed(stats, &s))
            break;
        record.beaten = s.solved > 0;
        if (s.solved == 0)
        {
            if (s.played > 0)
                record.line = "Not solved yet";
            break;
        }
        std::string bests;
        int shown = 0;
        for (const auto &[label, seconds] : s.best)
        {
            if (shown++ == 3)
            {
                bests += kDot + std::string("\xE2\x80\xA6");
                break;
            }
            if (!bests.empty())
                bests += kDot;
            bests += label + " " + clock_text(seconds);
        }
        record.line = "Best " + bests + kDot + solved_text(s.solved);
        break;
    }
    }
    return record;
}

} // namespace ppz::games
