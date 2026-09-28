// ProsperoPuzzles - Crowns rules: one crown per row, column and region, none touching.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/crowns/core.hpp"

#include "games/kit/puzzle_scene.hpp"

#include <algorithm>
#include <cstdlib>

namespace ppz::crowns
{

namespace
{

// Crowns in consecutive rows may not sit in neighbouring columns.
bool place_crowns(kit::Rng &rng, int side, int row, std::array<std::uint8_t, kMaxSide> *cols,
                  std::uint32_t used)
{
    if (row == side)
        return true;
    std::vector<int> order(static_cast<std::size_t>(side));
    for (int c = 0; c < side; ++c)
        order[static_cast<std::size_t>(c)] = c;
    rng.shuffle(order);
    for (int c : order)
    {
        if ((used & (1u << c)) != 0)
            continue;
        if (row > 0 && std::abs(c - (*cols)[static_cast<std::size_t>(row - 1)]) <= 1)
            continue;
        (*cols)[static_cast<std::size_t>(row)] = static_cast<std::uint8_t>(c);
        if (place_crowns(rng, side, row + 1, cols, used | (1u << c)))
            return true;
    }
    return false;
}

// Grows one region per crown outward until the board is covered.
void grow_regions(kit::Rng &rng, Puzzle *p)
{
    const int n = p->side;
    std::fill(p->region.begin(), p->region.end(), 0xff);
    std::vector<int> size(static_cast<std::size_t>(n), 1);
    for (int r = 0; r < n; ++r)
        p->region[static_cast<std::size_t>(r * n + p->solution[static_cast<std::size_t>(r)])] =
            static_cast<std::uint8_t>(r);
    int left = n * n - n;
    while (left > 0)
    {
        // Candidate (cell, region) pairs on the frontier; small regions grow first
        // more often, which keeps sizes varied but not extreme.
        std::vector<std::pair<int, int>> frontier;
        for (int i = 0; i < n * n; ++i)
        {
            if (p->region[static_cast<std::size_t>(i)] != 0xff)
                continue;
            const int r = i / n;
            const int c = i % n;
            const int nb[4][2] = {{r - 1, c}, {r + 1, c}, {r, c - 1}, {r, c + 1}};
            for (const auto &q : nb)
            {
                if (q[0] < 0 || q[1] < 0 || q[0] >= n || q[1] >= n)
                    continue;
                const int owner = p->region[static_cast<std::size_t>(q[0] * n + q[1])];
                if (owner != 0xff)
                    frontier.emplace_back(i, owner);
            }
        }
        std::pair<int, int> pick =
            frontier[static_cast<std::size_t>(rng.below(static_cast<int>(frontier.size())))];
        for (int tries = 0; tries < 2; ++tries)
        {
            const auto other =
                frontier[static_cast<std::size_t>(rng.below(static_cast<int>(frontier.size())))];
            if (size[static_cast<std::size_t>(other.second)] <
                size[static_cast<std::size_t>(pick.second)])
                pick = other;
        }
        p->region[static_cast<std::size_t>(pick.first)] = static_cast<std::uint8_t>(pick.second);
        ++size[static_cast<std::size_t>(pick.second)];
        --left;
    }
}

bool region_connected_without(const Puzzle &p, int region, int removed)
{
    const int n = p.side;
    std::vector<int> stack;
    std::vector<bool> seen(static_cast<std::size_t>(n * n), false);
    int total = 0;
    for (int i = 0; i < n * n; ++i)
    {
        if (i != removed && p.region[static_cast<std::size_t>(i)] == region)
        {
            ++total;
            if (stack.empty())
            {
                stack.push_back(i);
                seen[static_cast<std::size_t>(i)] = true;
            }
        }
    }
    if (total == 0)
        return false;
    int reached = 0;
    while (!stack.empty())
    {
        const int i = stack.back();
        stack.pop_back();
        ++reached;
        const int r = i / n;
        const int c = i % n;
        const int nb[4][2] = {{r - 1, c}, {r + 1, c}, {r, c - 1}, {r, c + 1}};
        for (const auto &q : nb)
        {
            if (q[0] < 0 || q[1] < 0 || q[0] >= n || q[1] >= n)
                continue;
            const int j = q[0] * n + q[1];
            if (j == removed || seen[static_cast<std::size_t>(j)] ||
                p.region[static_cast<std::size_t>(j)] != region)
                continue;
            seen[static_cast<std::size_t>(j)] = true;
            stack.push_back(j);
        }
    }
    return reached == total;
}

struct Solver
{
    const Puzzle &p;
    int limit;
    int found = 0;
    std::array<std::uint8_t, kMaxSide> cols{};
    std::array<std::array<std::uint8_t, kMaxSide>, 2> kept{}; // the first two solutions

    void search(int row, std::uint32_t used_cols, std::uint32_t used_regions)
    {
        if (found >= limit)
            return;
        const int n = p.side;
        if (row == n)
        {
            if (found < 2)
                kept[static_cast<std::size_t>(found)] = cols;
            ++found;
            return;
        }
        for (int c = 0; c < n; ++c)
        {
            const int region = p.region[static_cast<std::size_t>(row * n + c)];
            if ((used_cols & (1u << c)) != 0 || (used_regions & (1u << region)) != 0)
                continue;
            if (row > 0 && std::abs(c - cols[static_cast<std::size_t>(row - 1)]) <= 1)
                continue;
            cols[static_cast<std::size_t>(row)] = static_cast<std::uint8_t>(c);
            search(row + 1, used_cols | (1u << c), used_regions | (1u << region));
        }
    }
};

} // namespace

int count_solutions(const Puzzle &puzzle, int limit, std::array<std::uint8_t, kMaxSide> *first)
{
    Solver solver{puzzle, limit};
    solver.search(0, 0, 0);
    if (first != nullptr && solver.found > 0)
        *first = solver.kept[0];
    return solver.found;
}

Puzzle generate(std::uint64_t seed, int side)
{
    kit::Rng rng(seed);
    Puzzle p;
    p.side = std::clamp(side, 5, kMaxSide);
    const int n = p.side;
    for (;;)
    {
        place_crowns(rng, n, 0, &p.solution, 0);
        grow_regions(rng, &p);
        // Break alternative solutions by moving one of their crown cells into a
        // neighbouring region, keeping every region connected and the real
        // solution intact.
        for (int round = 0; round < 400; ++round)
        {
            Solver solver{p, 2};
            solver.search(0, 0, 0);
            if (solver.found == 1)
                return p;
            if (solver.found == 0)
                break; // cannot happen with our own solution in place; start over
            // The solution that is not ours.
            const auto &other = solver.kept[0] == p.solution ? solver.kept[1] : solver.kept[0];
            std::vector<int> targets;
            for (int r = 0; r < n; ++r)
            {
                const int c = other[static_cast<std::size_t>(r)];
                if (c != p.solution[static_cast<std::size_t>(r)])
                    targets.push_back(r * n + c);
            }
            rng.shuffle(targets);
            bool changed = false;
            for (int cell : targets)
            {
                const int from = p.region[static_cast<std::size_t>(cell)];
                if (!region_connected_without(p, from, cell))
                    continue;
                const int r = cell / n;
                const int c = cell % n;
                const int nb[4][2] = {{r - 1, c}, {r + 1, c}, {r, c - 1}, {r, c + 1}};
                std::vector<int> options;
                for (const auto &q : nb)
                {
                    if (q[0] < 0 || q[1] < 0 || q[0] >= n || q[1] >= n)
                        continue;
                    const int to = p.region[static_cast<std::size_t>(q[0] * n + q[1])];
                    if (to != from)
                        options.push_back(to);
                }
                if (options.empty())
                    continue;
                p.region[static_cast<std::size_t>(cell)] = static_cast<std::uint8_t>(
                    options[static_cast<std::size_t>(rng.below(static_cast<int>(options.size())))]);
                changed = true;
                break;
            }
            if (!changed)
                break;
        }
    }
}

std::vector<bool> conflicts(const Puzzle &puzzle, const std::array<std::uint8_t, kMaxCells> &marks)
{
    const int n = puzzle.side;
    std::vector<bool> bad(static_cast<std::size_t>(std::max(0, n * n)), false);
    if (n <= 0)
        return bad;
    std::vector<int> crowns;
    for (int i = 0; i < n * n; ++i)
        if (marks[static_cast<std::size_t>(i)] == kCrown)
            crowns.push_back(i);
    for (std::size_t a = 0; a < crowns.size(); ++a)
    {
        for (std::size_t b = a + 1; b < crowns.size(); ++b)
        {
            const int i = crowns[a];
            const int j = crowns[b];
            const bool same_row = i / n == j / n;
            const bool same_col = i % n == j % n;
            const bool same_region = puzzle.region[static_cast<std::size_t>(i)] ==
                                     puzzle.region[static_cast<std::size_t>(j)];
            const bool touching = std::abs(i / n - j / n) <= 1 && std::abs(i % n - j % n) <= 1;
            if (same_row || same_col || same_region || touching)
            {
                bad[static_cast<std::size_t>(i)] = true;
                bad[static_cast<std::size_t>(j)] = true;
            }
        }
    }
    return bad;
}

int crowns_placed(const Puzzle &puzzle, const std::array<std::uint8_t, kMaxCells> &marks)
{
    int count = 0;
    for (int i = 0; i < puzzle.side * puzzle.side; ++i)
        count += marks[static_cast<std::size_t>(i)] == kCrown ? 1 : 0;
    return count;
}

bool solved(const Puzzle &puzzle, const std::array<std::uint8_t, kMaxCells> &marks)
{
    if (crowns_placed(puzzle, marks) != puzzle.side)
        return false;
    const std::vector<bool> bad = conflicts(puzzle, marks);
    return std::none_of(bad.begin(), bad.end(), [](bool b) { return b; });
}

} // namespace ppz::crowns
