// ProsperoPuzzles - Trail rules: one path through every cell, passing the numbers in order.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/trail/core.hpp"

#include "games/kit/puzzle_scene.hpp"

#include <algorithm>
#include <bit>
#include <cstdlib>

namespace ppz::trail
{

namespace
{

using Mask = std::uint64_t;

Mask bit(int cell)
{
    return Mask{1} << static_cast<unsigned>(cell);
}

// Neighbour masks and flood-fill helpers for one board size.
struct Board
{
    int side = 0;
    int cells = 0;
    Mask all = 0;
    Mask black = 0;     // cells with (row + col) odd
    Mask not_first = 0; // every cell except column 0
    Mask not_last = 0;  // every cell except the last column
    std::array<Mask, kMaxCells> near{};

    explicit Board(int n) : side(n), cells(n * n)
    {
        all = cells >= 64 ? ~Mask{0} : bit(cells) - 1;
        for (int i = 0; i < cells; ++i)
        {
            const int r = i / n;
            const int c = i % n;
            if (((r + c) & 1) != 0)
                black |= bit(i);
            if (c != 0)
                not_first |= bit(i);
            if (c != n - 1)
                not_last |= bit(i);
            Mask m = 0;
            if (r > 0)
                m |= bit(i - n);
            if (r + 1 < n)
                m |= bit(i + n);
            if (c > 0)
                m |= bit(i - 1);
            if (c + 1 < n)
                m |= bit(i + 1);
            near[static_cast<std::size_t>(i)] = m;
        }
    }

    // Everything in free reachable from seed (seed must lie inside free).
    Mask flood(Mask seed, Mask free) const
    {
        Mask reach = seed & free;
        for (;;)
        {
            Mask next = reach | ((reach << 1) & not_first) | ((reach >> 1) & not_last) |
                        (reach << static_cast<unsigned>(side)) |
                        (reach >> static_cast<unsigned>(side));
            next &= free;
            if (next == reach)
                return reach;
            reach = next;
        }
    }
};

struct Search
{
    Search(const Board &b, const Puzzle &p) : board(b), puzzle(p)
    {
    }

    Board board;
    Puzzle puzzle;
    int limit = 1;
    long budget = 0; // 0 = unlimited
    long nodes = 0;
    bool aborted = false;
    int found = 0;
    int end_cell = 0;
    std::vector<std::uint8_t> path;
    std::vector<std::vector<std::uint8_t>> solutions;

    // Rejects positions that can no longer be completed.
    bool viable(int head, Mask free) const
    {
        if (free == 0)
            return true;
        const int remaining = std::popcount(free);
        // Colours alternate along the path.
        const int black = std::popcount(free & board.black);
        const bool head_black = (board.black & bit(head)) != 0;
        const int opposite = head_black ? remaining - black : black;
        if (opposite != (remaining + 1) / 2)
            return false;
        // The rest of the board must stay reachable from the head.
        const Mask seed = board.near[static_cast<std::size_t>(head)] & free;
        if (seed == 0 || board.flood(seed, free) != free)
            return false;
        // A cell with one way in can only be where the path ends.
        const Mask open = free | bit(head);
        for (Mask f = free; f != 0; f &= f - 1)
        {
            const int u = std::countr_zero(f);
            const int degree = std::popcount(board.near[static_cast<std::size_t>(u)] & open);
            if (degree <= 1 && u != end_cell)
                return false;
        }
        return true;
    }

    void dfs(int head, Mask free, int next)
    {
        if (found >= limit || aborted)
            return;
        if (budget > 0 && ++nodes > budget)
        {
            aborted = true;
            return;
        }
        if (free == 0)
        {
            ++found;
            solutions.push_back(path);
            return;
        }
        for (Mask m = board.near[static_cast<std::size_t>(head)] & free; m != 0; m &= m - 1)
        {
            const int c = std::countr_zero(m);
            const int number = puzzle.number[static_cast<std::size_t>(c)];
            if (number != 0 && number != next)
                continue;
            const Mask rest = free & ~bit(c);
            if (c == end_cell && rest != 0)
                continue;
            if (!viable(c, rest))
                continue;
            path.push_back(static_cast<std::uint8_t>(c));
            dfs(c, rest, number != 0 ? next + 1 : next);
            path.pop_back();
            if (found >= limit || aborted)
                return;
        }
    }
};

// Returns the solution count (up to limit), or -1 when the budget ran out.
int solve(const Puzzle &puzzle, int limit, long budget,
          std::vector<std::vector<std::uint8_t>> *solutions)
{
    const int start = cell_of(puzzle, 1);
    const int end = cell_of(puzzle, puzzle.count);
    if (start < 0 || end < 0 || puzzle.side < 2 || puzzle.side > kMaxSide)
        return 0;
    const Board board(puzzle.side);
    Search s(board, puzzle);
    s.limit = limit;
    s.budget = budget;
    s.end_cell = end;
    s.path.push_back(static_cast<std::uint8_t>(start));
    const Mask free = board.all & ~bit(start);
    if (board.cells == 1)
        return 1;
    if (s.viable(start, free))
        s.dfs(start, free, 2);
    if (s.aborted)
        return -1;
    if (solutions != nullptr)
        *solutions = std::move(s.solutions);
    return s.found;
}

// A random Hamiltonian path: a serpentine mixed by many backbite moves.
std::vector<std::uint8_t> random_path(kit::Rng &rng, const Board &board)
{
    const int n = board.side;
    const int cells = board.cells;
    std::vector<std::uint8_t> path;
    for (int r = 0; r < n; ++r)
        for (int k = 0; k < n; ++k)
            path.push_back(static_cast<std::uint8_t>(r * n + ((r & 1) != 0 ? n - 1 - k : k)));
    std::vector<int> pos(static_cast<std::size_t>(cells));
    auto reindex = [&](int from, int to)
    {
        for (int i = from; i <= to; ++i)
            pos[path[static_cast<std::size_t>(i)]] = i;
    };
    reindex(0, cells - 1);
    const int moves = cells * 80;
    for (int m = 0; m < moves; ++m)
    {
        const bool at_end = rng.below(2) == 1;
        const int end = at_end ? path.back() : path.front();
        std::vector<int> options;
        for (Mask f = board.near[static_cast<std::size_t>(end)]; f != 0; f &= f - 1)
            options.push_back(std::countr_zero(f));
        const int x =
            options[static_cast<std::size_t>(rng.below(static_cast<int>(options.size())))];
        const int i = pos[static_cast<std::size_t>(x)];
        if (at_end)
        {
            if (i == cells - 2)
                continue;
            std::reverse(path.begin() + i + 1, path.end());
            reindex(i + 1, cells - 1);
        }
        else
        {
            if (i == 1)
                continue;
            std::reverse(path.begin(), path.begin() + i);
            reindex(0, i - 1);
        }
    }
    return path;
}

void number_checkpoints(Puzzle *p, const std::vector<bool> &checkpoint)
{
    p->number.fill(0);
    int k = 0;
    for (std::size_t i = 0; i < checkpoint.size(); ++i)
        if (checkpoint[i])
            p->number[p->solution[i]] = static_cast<std::uint8_t>(++k);
    p->count = k;
}

constexpr long kBudget = 400000; // search nodes before a position counts as too hard

// Drops numbers that uniqueness does not need, crowded ones first, down to floor.
void trim(Puzzle *p, std::vector<bool> *checkpoint, kit::Rng &rng, int floor)
{
    const int cells = static_cast<int>(checkpoint->size());
    std::vector<bool> tried(checkpoint->size(), false);
    for (;;)
    {
        std::vector<int> at;
        for (int t = 0; t < cells; ++t)
            if ((*checkpoint)[static_cast<std::size_t>(t)])
                at.push_back(t);
        if (static_cast<int>(at.size()) <= floor)
            return;
        int best = -1;
        int best_gap = 0;
        for (std::size_t k = 1; k + 1 < at.size(); ++k)
        {
            if (tried[static_cast<std::size_t>(at[k])])
                continue;
            const int gap = at[k + 1] - at[k - 1] + rng.below(3);
            if (best < 0 || gap < best_gap)
            {
                best = at[k];
                best_gap = gap;
            }
        }
        if (best < 0)
            return;
        tried[static_cast<std::size_t>(best)] = true;
        (*checkpoint)[static_cast<std::size_t>(best)] = false;
        number_checkpoints(p, *checkpoint);
        if (solve(*p, 2, kBudget, nullptr) != 1)
        {
            (*checkpoint)[static_cast<std::size_t>(best)] = true;
            number_checkpoints(p, *checkpoint);
        }
    }
}

} // namespace

int cell_of(const Puzzle &puzzle, int n)
{
    const int cells = puzzle.side * puzzle.side;
    for (int i = 0; i < cells; ++i)
        if (puzzle.number[static_cast<std::size_t>(i)] == n)
            return i;
    return -1;
}

int count_solutions(const Puzzle &puzzle, int limit, std::vector<std::uint8_t> *first,
                    long node_budget)
{
    std::vector<std::vector<std::uint8_t>> solutions;
    const int count = solve(puzzle, limit, node_budget, &solutions);
    if (first != nullptr && !solutions.empty())
        *first = solutions.front();
    return count;
}

Puzzle generate(std::uint64_t seed, int side)
{
    side = std::clamp(side, 4, kMaxSide);
    kit::Rng rng(seed ^ (0x7a11ULL * static_cast<std::uint64_t>(side)));
    const Board board(side);
    const int cells = board.cells;
    for (;;)
    {
        Puzzle p;
        p.side = side;
        const std::vector<std::uint8_t> path = random_path(rng, board);
        std::copy(path.begin(), path.end(), p.solution.begin());

        // Numbers spaced along the path, a little jittered.
        std::vector<bool> checkpoint(static_cast<std::size_t>(cells), false);
        checkpoint.front() = true;
        checkpoint.back() = true;
        const int wanted = side - 1 + rng.below(2);
        const int span = (cells - 1) / std::max(1, wanted - 1);
        for (int k = 1; k + 1 < wanted; ++k)
        {
            const int jitter = span / 3 > 0 ? rng.below(2 * (span / 3) + 1) - span / 3 : 0;
            const int at = std::clamp(k * (cells - 1) / (wanted - 1) + jitter, 1, cells - 2);
            checkpoint[static_cast<std::size_t>(at)] = true;
        }

        // Add numbers until the path is the only one.
        for (int round = 0; round < cells; ++round)
        {
            number_checkpoints(&p, checkpoint);
            std::vector<std::vector<std::uint8_t>> found;
            const int count = solve(p, 2, kBudget, &found);
            if (count == 1)
            {
                trim(&p, &checkpoint, rng, side + 1 + rng.below(2));
                return p;
            }
            std::vector<int> candidates;
            if (count == 2)
            {
                const auto &alt = found[0] == path ? found[1] : found[0];
                std::vector<int> alt_pos(static_cast<std::size_t>(cells));
                for (int i = 0; i < cells; ++i)
                    alt_pos[alt[static_cast<std::size_t>(i)]] = i;
                // A new number breaks the rival path when the rival reaches
                // its cell outside the numbers around it.
                int before = 0;
                for (int t = 1; t + 1 < cells; ++t)
                {
                    if (checkpoint[static_cast<std::size_t>(t)])
                    {
                        before = t;
                        continue;
                    }
                    int after = t + 1;
                    while (!checkpoint[static_cast<std::size_t>(after)])
                        ++after;
                    const int here = alt_pos[path[static_cast<std::size_t>(t)]];
                    if (here < alt_pos[path[static_cast<std::size_t>(before)]] ||
                        here > alt_pos[path[static_cast<std::size_t>(after)]])
                        candidates.push_back(t);
                }
                if (candidates.empty())
                {
                    int t = 0;
                    while (alt[static_cast<std::size_t>(t)] == path[static_cast<std::size_t>(t)])
                        ++t;
                    candidates.push_back(t);
                }
            }
            else
            {
                // The search was too slow: split the longest stretch without numbers.
                int best = 0;
                int best_len = 0;
                int last = 0;
                for (int t = 1; t < cells; ++t)
                {
                    if (!checkpoint[static_cast<std::size_t>(t)])
                        continue;
                    if (t - last > best_len)
                    {
                        best_len = t - last;
                        best = last + (t - last) / 2;
                    }
                    last = t;
                }
                candidates.push_back(best);
            }
            checkpoint[static_cast<std::size_t>(candidates[static_cast<std::size_t>(
                rng.below(static_cast<int>(candidates.size())))])] = true;
        }
    }
}

int next_number(const Puzzle &puzzle, const std::vector<std::uint8_t> &path)
{
    int reached = 0;
    for (std::uint8_t cell : path)
        reached = std::max<int>(reached, puzzle.number[cell]);
    return reached + 1;
}

bool valid_path(const Puzzle &puzzle, const std::vector<std::uint8_t> &path)
{
    const int n = puzzle.side;
    const int cells = n * n;
    if (path.empty() || static_cast<int>(path.size()) > cells)
        return false;
    Mask seen = 0;
    int next = 1;
    for (std::size_t i = 0; i < path.size(); ++i)
    {
        const int c = path[i];
        if (c >= cells || (seen & bit(c)) != 0)
            return false;
        if (next > puzzle.count) // nothing may follow the highest number
            return false;
        if (i > 0)
        {
            const int p = path[i - 1];
            const int dr = c / n - p / n;
            const int dc = c % n - p % n;
            if (std::abs(dr) + std::abs(dc) != 1)
                return false;
        }
        const int number = puzzle.number[static_cast<std::size_t>(c)];
        if (i == 0 && number != 1)
            return false;
        if (number != 0)
        {
            if (number != next)
                return false;
            ++next;
        }
        seen |= bit(c);
    }
    return true;
}

bool solved(const Puzzle &puzzle, const std::vector<std::uint8_t> &path)
{
    return static_cast<int>(path.size()) == puzzle.side * puzzle.side && valid_path(puzzle, path) &&
           puzzle.number[path.back()] == puzzle.count;
}

Step step(const Puzzle &puzzle, std::vector<std::uint8_t> *path, int dcol, int drow)
{
    const int n = puzzle.side;
    const int head = path->back();
    const int col = head % n + dcol;
    const int row = head / n + drow;
    if (col < 0 || row < 0 || col >= n || row >= n)
        return Step::off_board;
    const auto cell = static_cast<std::uint8_t>(row * n + col);
    if (path->size() >= 2 && (*path)[path->size() - 2] == cell)
    {
        path->pop_back();
        return Step::retracted;
    }
    if (std::find(path->begin(), path->end(), cell) != path->end())
        return Step::crossed;
    if (puzzle.number[head] == puzzle.count)
        return Step::finished;
    const int number = puzzle.number[cell];
    if (number != 0 && number != next_number(puzzle, *path))
        return Step::out_of_order;
    path->push_back(cell);
    return Step::extended;
}

} // namespace ppz::trail
