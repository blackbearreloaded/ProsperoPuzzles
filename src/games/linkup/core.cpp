// ProsperoPuzzles - Link Up rules: join every pair of dots and fill the board.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/linkup/core.hpp"

#include "games/kit/puzzle_scene.hpp"

#include <algorithm>
#include <cstdlib>
#include <numeric>

namespace ppz::linkup
{

namespace
{

constexpr int kMinLength = 3;

std::size_t at(int index)
{
    return static_cast<std::size_t>(index);
}

// First cell of a pair within the route.
int route_start(const Puzzle &p, int pair)
{
    int start = 0;
    for (int q = 0; q < pair; ++q)
        start += p.length[at(q)];
    return start;
}

// A random Hamiltonian path: a serpentine mixed by backbite moves (each
// move joins an end to one of its neighbours and reverses the loose part).
std::vector<int> random_route(kit::Rng &rng, int n)
{
    std::vector<int> path;
    path.reserve(at(n * n));
    for (int row = 0; row < n; ++row)
        for (int i = 0; i < n; ++i)
            path.push_back(row * n + ((row % 2 == 0) ? i : n - 1 - i));
    if (rng.below(2) == 1)
        for (int &cell : path) // transpose, so the serpentine can run either way
            cell = (cell % n) * n + cell / n;

    std::vector<int> index(path.size());
    const auto reindex = [&](int from, int to)
    {
        for (int i = from; i <= to; ++i)
            index[at(path[at(i)])] = i;
    };
    reindex(0, n * n - 1);
    const int last = n * n - 1;
    const int moves = 40 * n * n;
    for (int m = 0; m < moves; ++m)
    {
        const bool front = rng.below(2) == 1;
        const int head = front ? path[0] : path[at(last)];
        const int row = head / n;
        const int col = head % n;
        int options[4];
        int count = 0;
        if (row > 0)
            options[count++] = head - n;
        if (row + 1 < n)
            options[count++] = head + n;
        if (col > 0)
            options[count++] = head - 1;
        if (col + 1 < n)
            options[count++] = head + 1;
        const int pick = options[rng.below(count)];
        const int i = index[at(pick)];
        if (front && i > 1)
        {
            std::reverse(path.begin(), path.begin() + i);
            reindex(0, i - 1);
        }
        else if (!front && i < last - 1)
        {
            std::reverse(path.begin() + i + 1, path.end());
            reindex(i + 1, last);
        }
    }
    return path;
}

// Segment lengths (each at least kMinLength) that sum to total, not too uneven.
std::vector<int> random_lengths(kit::Rng &rng, int total, int pairs)
{
    const int extra = total - kMinLength * pairs;
    const int longest = (2 * total) / pairs + 2;
    std::vector<int> lengths(at(pairs));
    for (int tries = 0; tries < 200; ++tries)
    {
        std::vector<int> bars(at(pairs - 1));
        for (int &bar : bars)
            bar = rng.below(extra + 1);
        std::sort(bars.begin(), bars.end());
        int previous = 0;
        bool ok = true;
        for (int i = 0; i < pairs; ++i)
        {
            const int bar = i + 1 < pairs ? bars[at(i)] : extra;
            lengths[at(i)] = kMinLength + bar - previous;
            previous = bar;
            ok = ok && lengths[at(i)] <= longest;
        }
        if (ok)
            break;
    }
    return lengths;
}

bool straight(int n, const int *cells, int length)
{
    bool same_row = true;
    bool same_col = true;
    for (int i = 1; i < length; ++i)
    {
        same_row = same_row && cells[i] / n == cells[0] / n;
        same_col = same_col && cells[i] % n == cells[0] % n;
    }
    return same_row || same_col;
}

int pair_count(kit::Rng &rng, int side)
{
    switch (side)
    {
    case 5:
        return 5;
    case 6:
        return 6;
    case 7:
        return 7 + rng.below(2);
    case 8:
        return 8 + rng.below(2);
    default:
        return 9 + rng.below(3);
    }
}

} // namespace

bool adjacent(int side, int a, int b)
{
    if (side <= 0)
        return false;
    const int dr = std::abs(a / side - b / side);
    const int dc = std::abs(a % side - b % side);
    return dr + dc == 1;
}

int Puzzle::dot(int pair, int which) const
{
    const int start = route_start(*this, pair);
    return route[at(which == 0 ? start : start + length[at(pair)] - 1)];
}

int Puzzle::dot_at(int cell) const
{
    for (int p = 0; p < pairs; ++p)
        if (dot(p, 0) == cell || dot(p, 1) == cell)
            return p;
    return kNone;
}

std::vector<std::uint8_t> Puzzle::solution(int pair) const
{
    const int start = route_start(*this, pair);
    return {route.begin() + start, route.begin() + start + length[at(pair)]};
}

Puzzle generate(std::uint64_t seed, int side)
{
    kit::Rng rng(seed);
    Puzzle p;
    p.side = std::clamp(side, kMinSide, kMaxSide);
    const int n = p.side;
    const int total = n * n;
    Puzzle best = p;
    bool found = false;
    for (int attempt = 0; attempt < 400 && !found; ++attempt)
    {
        const std::vector<int> route = random_route(rng, n);
        for (int cut = 0; cut < 12 && !found; ++cut)
        {
            const int pairs = pair_count(rng, n);
            const std::vector<int> lengths = random_lengths(rng, total, pairs);
            int start = 0;
            int straights = 0;
            bool ok = true;
            for (int i = 0; i < pairs && ok; ++i)
            {
                const int length = lengths[at(i)];
                const int *cells = route.data() + start;
                ok = length >= kMinLength && !adjacent(n, cells[0], cells[length - 1]);
                if (straight(n, cells, length))
                    ++straights;
                start += length;
            }
            ok = ok && start == total && straights * 2 <= pairs;
            if (!ok)
                continue;
            // Colours in a shuffled order, so neighbouring segments differ.
            std::vector<int> order(at(pairs));
            std::iota(order.begin(), order.end(), 0);
            rng.shuffle(order);
            std::vector<int> offset(at(pairs), 0);
            for (int i = 1; i < pairs; ++i)
                offset[at(i)] = offset[at(i - 1)] + lengths[at(i - 1)];
            p.pairs = pairs;
            int out = 0;
            for (int colour = 0; colour < pairs; ++colour)
            {
                const int segment = order[at(colour)];
                const int length = lengths[at(segment)];
                const bool flip = rng.below(2) == 1;
                p.length[at(colour)] = static_cast<std::uint8_t>(length);
                for (int k = 0; k < length; ++k)
                {
                    const int from = offset[at(segment)] + (flip ? length - 1 - k : k);
                    p.route[at(out++)] = static_cast<std::uint8_t>(route[at(from)]);
                }
            }
            best = p;
            found = true;
        }
    }
    if (!found)
    {
        // Practically unreachable; a plain serpentine cut into fixed runs.
        const int pairs = n;
        best.pairs = pairs;
        for (int i = 0; i < total; ++i)
        {
            const int row = i / n;
            const int col = i % n;
            best.route[at(i)] =
                static_cast<std::uint8_t>(row * n + (row % 2 == 0 ? col : n - 1 - col));
        }
        for (int i = 0; i < pairs; ++i)
            best.length[at(i)] = static_cast<std::uint8_t>(n);
    }
    return best;
}

bool valid_puzzle(const Puzzle &p)
{
    if (p.side < kMinSide || p.side > kMaxSide || p.pairs < 2 || p.pairs > kMaxPairs)
        return false;
    int sum = 0;
    for (int i = 0; i < p.pairs; ++i)
    {
        if (p.length[at(i)] < 2)
            return false;
        sum += p.length[at(i)];
    }
    if (sum != p.cells())
        return false;
    std::array<bool, kMaxCells> seen{};
    for (int i = 0; i < sum; ++i)
    {
        const int cell = p.route[at(i)];
        if (cell >= p.cells() || seen[at(cell)])
            return false;
        seen[at(cell)] = true;
    }
    int start = 0;
    for (int pair = 0; pair < p.pairs; ++pair)
    {
        for (int k = 1; k < p.length[at(pair)]; ++k)
            if (!adjacent(p.side, p.route[at(start + k - 1)], p.route[at(start + k)]))
                return false;
        start += p.length[at(pair)];
    }
    return true;
}

bool valid_paths(const Puzzle &p, const Paths &paths)
{
    std::array<bool, kMaxCells> used{};
    for (int pair = 0; pair < kMaxPairs; ++pair)
    {
        const auto &path = paths[at(pair)];
        if (path.empty())
            continue;
        if (pair >= p.pairs || path.size() > at(p.cells()))
            return false;
        const int a = p.dot(pair, 0);
        const int b = p.dot(pair, 1);
        if (path.front() != a && path.front() != b)
            return false;
        const int other = path.front() == a ? b : a;
        for (std::size_t i = 0; i < path.size(); ++i)
        {
            const int cell = path[i];
            if (cell >= p.cells() || used[at(cell)])
                return false;
            used[at(cell)] = true;
            if (i > 0 && !adjacent(p.side, path[i - 1], cell))
                return false;
            const int dot = p.dot_at(cell);
            if (dot != kNone && dot != pair)
                return false;
            if (cell == other && i + 1 != path.size())
                return false;
        }
    }
    return true;
}

int owner(const Paths &paths, int cell)
{
    for (int pair = 0; pair < kMaxPairs; ++pair)
        for (std::uint8_t c : paths[at(pair)])
            if (c == cell)
                return pair;
    return kNone;
}

bool connected(const Puzzle &p, const Paths &paths, int pair)
{
    if (pair < 0 || pair >= p.pairs)
        return false;
    const auto &path = paths[at(pair)];
    if (path.size() < 2)
        return false;
    const int a = p.dot(pair, 0);
    const int b = p.dot(pair, 1);
    return (path.front() == a && path.back() == b) || (path.front() == b && path.back() == a);
}

int connected_pairs(const Puzzle &p, const Paths &paths)
{
    int count = 0;
    for (int pair = 0; pair < p.pairs; ++pair)
        if (connected(p, paths, pair))
            ++count;
    return count;
}

bool solved(const Puzzle &p, const Paths &paths)
{
    if (p.pairs <= 0 || connected_pairs(p, paths) != p.pairs)
        return false;
    std::size_t covered = 0;
    for (int pair = 0; pair < p.pairs; ++pair)
        covered += paths[at(pair)].size();
    return covered == at(p.cells()) && valid_paths(p, paths);
}

int pick_up(const Puzzle &p, Paths &paths, int cell)
{
    if (cell < 0 || cell >= p.cells())
        return kNone;
    const int dot = p.dot_at(cell);
    if (dot != kNone)
    {
        paths[at(dot)] = {static_cast<std::uint8_t>(cell)};
        return dot;
    }
    const int pair = owner(paths, cell);
    if (pair == kNone)
        return kNone;
    auto &path = paths[at(pair)];
    const auto it = std::find(path.begin(), path.end(), static_cast<std::uint8_t>(cell));
    path.erase(it + 1, path.end());
    return pair;
}

Step extend(const Puzzle &p, Paths &paths, int pair, int cell)
{
    if (pair < 0 || pair >= p.pairs || cell < 0 || cell >= p.cells())
        return Step::rejected;
    auto &path = paths[at(pair)];
    if (path.empty() || !adjacent(p.side, path.back(), cell) || connected(p, paths, pair))
        return Step::rejected;
    const auto cell8 = static_cast<std::uint8_t>(cell);

    // Back onto its own path: cut back to that cell.
    const auto own = std::find(path.begin(), path.end(), cell8);
    if (own != path.end())
    {
        path.erase(own + 1, path.end());
        return Step::retracted;
    }
    const int dot = p.dot_at(cell);
    if (dot != kNone && dot != pair)
        return Step::rejected;
    if (dot == pair)
    {
        path.push_back(cell8);
        return Step::connected;
    }
    const int other = owner(paths, cell);
    Step result = Step::extended;
    if (other != kNone)
    {
        auto &cut = paths[at(other)];
        cut.erase(std::find(cut.begin(), cut.end(), cell8), cut.end());
        result = Step::cut;
    }
    path.push_back(cell8);
    return result;
}

} // namespace ppz::linkup
