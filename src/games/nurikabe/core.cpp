// ProsperoPuzzles - Nurikabe rules: numbered islands in one connected sea, no 2x2 pools.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/nurikabe/core.hpp"

#include "games/kit/puzzle_scene.hpp"

#include <algorithm>

namespace ppz::nurikabe
{

namespace
{

std::size_t z(int i)
{
    return static_cast<std::size_t>(i);
}

using Flags = std::array<bool, kMaxCells>;
using Ints = std::array<int, kMaxCells>;

// Orthogonal neighbours of cell i on an n x n board.
int neighbours(int n, int i, std::array<int, 4> *out)
{
    if (n <= 0)
        return 0;
    const int r = i / n;
    const int c = i % n;
    int k = 0;
    if (r > 0)
        (*out)[z(k++)] = i - n;
    if (r + 1 < n)
        (*out)[z(k++)] = i + n;
    if (c > 0)
        (*out)[z(k++)] = i - 1;
    if (c + 1 < n)
        (*out)[z(k++)] = i + 1;
    return k;
}

// Flood fill from start over cells accepted by in(); visit() sees each cell once.
template <typename In, typename Visit>
int flood(int n, int start, Flags *seen, const In &in, const Visit &visit)
{
    Ints stack{};
    int top = 0;
    stack[z(top++)] = start;
    (*seen)[z(start)] = true;
    int count = 0;
    while (top > 0)
    {
        const int i = stack[z(--top)];
        ++count;
        visit(i);
        std::array<int, 4> nb{};
        const int k = neighbours(n, i, &nb);
        for (int j = 0; j < k; ++j)
        {
            const int q = nb[z(j)];
            if (!(*seen)[z(q)] && in(q))
            {
                (*seen)[z(q)] = true;
                stack[z(top++)] = q;
            }
        }
    }
    return count;
}

// ---- solver ----

using State = std::array<std::uint8_t, kMaxCells>;
constexpr std::uint8_t kOpen = 0;
constexpr std::uint8_t kWater = 1;
constexpr std::uint8_t kLand = 2;

struct Island
{
    int size = 0;
    int value = 0; // the clue, 0 for land not yet tied to a number
    int clues = 0;
};

// Deductions to a fixed point, then depth-first guessing that counts solutions.
struct Solver
{
    const Puzzle &p;
    int n;
    int cells;
    int limit;
    int budget;
    int work = 0;
    bool aborted = false;
    int found = 0;
    std::array<Cells, 2> kept{}; // the first two solutions
    int land_total = 0;
    int sea_total = 0;

    Solver(const Puzzle &puzzle, int limit_, int budget_)
        : p(puzzle), n(puzzle.side), cells(puzzle.side * puzzle.side), limit(limit_),
          budget(budget_)
    {
        for (int i = 0; i < cells; ++i)
            land_total += p.clue[z(i)];
        sea_total = cells - land_total;
    }

    State initial() const
    {
        State s{};
        for (int i = 0; i < cells; ++i)
            s[z(i)] = p.clue[z(i)] != 0 ? kLand : kOpen;
        return s;
    }

    struct Cut
    {
        Ints disc{};
        Ints low{};
        Ints water{};
        Flags must{};
        int time = 0;
    };

    bool propagate(State &s);
    bool propagate_islands(State &s, bool *changed) const;
    bool propagate_sea(State &s, bool *changed) const;
    void cut_walk(const State &s, int u, int parent, Cut *cut) const;
    void search(State s);
};

// Deductions from land: islands, their room to grow and their reach.
bool Solver::propagate_islands(State &s, bool *changed) const
{
    Ints comp{};
    comp.fill(-1);
    std::array<Island, kMaxCells> isl{};
    int count = 0;
    Flags seen{};
    for (int i = 0; i < cells; ++i)
    {
        if (s[z(i)] != kLand || seen[z(i)])
            continue;
        Island &island = isl[z(count)];
        island.size = flood(
            n, i, &seen, [&](int q) { return s[z(q)] == kLand; },
            [&](int q)
            {
                comp[z(q)] = count;
                if (p.clue[z(q)] != 0)
                {
                    ++island.clues;
                    island.value = p.clue[z(q)];
                }
            });
        if (island.clues > 1 || (island.clues == 1 && island.size > island.value))
            return false;
        ++count;
    }
    const auto complete = [&](int k)
    { return isl[z(k)].value > 0 && isl[z(k)].size == isl[z(k)].value; };

    // Open cells next to two numbered islands, or to a finished one, are sea.
    Ints owner{}; // -1 none, else the one numbered island alongside
    for (int i = 0; i < cells; ++i)
    {
        owner[z(i)] = -1;
        if (s[z(i)] != kOpen)
            continue;
        std::array<int, 4> nb{};
        const int k = neighbours(n, i, &nb);
        bool water = false;
        for (int j = 0; j < k; ++j)
        {
            const int q = nb[z(j)];
            if (s[z(q)] != kLand)
                continue;
            const int c = comp[z(q)];
            if (isl[z(c)].value == 0)
                continue;
            if (complete(c) || (owner[z(i)] >= 0 && owner[z(i)] != c))
                water = true;
            owner[z(i)] = c;
        }
        if (water)
        {
            s[z(i)] = kWater;
            *changed = true;
        }
    }
    if (*changed)
        return true;

    // An unfinished island (or stray land) with one way out must take it.
    Ints exits{};
    Ints exit_cell{};
    for (int i = 0; i < cells; ++i)
    {
        if (s[z(i)] != kOpen)
            continue;
        std::array<int, 4> nb{};
        const int k = neighbours(n, i, &nb);
        std::array<int, 4> counted{-1, -1, -1, -1};
        for (int j = 0; j < k; ++j)
        {
            const int q = nb[z(j)];
            if (s[z(q)] != kLand)
                continue;
            const int c = comp[z(q)];
            if (std::find(counted.begin(), counted.end(), c) != counted.end())
                continue;
            counted[z(j)] = c;
            ++exits[z(c)];
            exit_cell[z(c)] = i;
        }
    }
    for (int c = 0; c < count; ++c)
    {
        if (complete(c))
            continue;
        if (exits[z(c)] == 0)
            return false;
        if (exits[z(c)] == 1)
        {
            s[z(exit_cell[z(c)])] = kLand;
            *changed = true;
        }
    }
    if (*changed)
        return true;

    // Cells no unfinished island can reach within its size are sea; an island
    // that cannot reach enough cells is a contradiction.
    Flags reach{};
    Ints dist{};
    Ints queue{};
    for (int c = 0; c < count; ++c)
    {
        const Island &island = isl[z(c)];
        if (island.value == 0 || island.size >= island.value)
            continue;
        const int room = island.value - island.size;
        dist.fill(1 << 20);
        int head = 0;
        int tail = 0;
        for (int i = 0; i < cells; ++i)
            if (comp[z(i)] == c)
            {
                dist[z(i)] = 0;
                queue[z(tail++)] = i;
            }
        int reached = 0;
        while (head < tail)
        {
            const int i = queue[z(head++)];
            const int d = dist[z(i)] + 1;
            if (d > room)
                continue;
            std::array<int, 4> nb{};
            const int k = neighbours(n, i, &nb);
            for (int j = 0; j < k; ++j)
            {
                const int q = nb[z(j)];
                if (dist[z(q)] <= d || s[z(q)] == kWater)
                    continue;
                if (s[z(q)] == kLand && isl[z(comp[z(q)])].value != 0)
                    continue;
                if (s[z(q)] == kOpen && owner[z(q)] >= 0 && owner[z(q)] != c)
                    continue;
                dist[z(q)] = d;
                queue[z(tail++)] = q;
                reach[z(q)] = true;
                ++reached;
            }
        }
        if (reached < room)
            return false;
    }
    for (int i = 0; i < cells; ++i)
    {
        if (reach[z(i)])
            continue;
        if (s[z(i)] == kOpen)
        {
            s[z(i)] = kWater;
            *changed = true;
        }
        else if (s[z(i)] == kLand && isl[z(comp[z(i)])].value == 0)
        {
            return false;
        }
    }
    return true;
}

// Deductions from the sea: it must stay one connected body.
bool Solver::propagate_sea(State &s, bool *changed) const
{
    int sea = 0;
    int start = -1;
    for (int i = 0; i < cells; ++i)
        if (s[z(i)] == kWater)
        {
            ++sea;
            if (start < 0)
                start = i;
        }
    if (sea == 0)
        return true;
    // One walk over the wet cells: all sea must be reachable from one sea cell,
    // and an open cell that alone links some sea to the rest is sea.
    Cut cut;
    cut.disc.fill(-1);
    cut_walk(s, start, -1, &cut);
    for (int i = 0; i < cells; ++i)
        if (s[z(i)] == kWater && cut.disc[z(i)] < 0)
            return false;
    for (int i = 0; i < cells; ++i)
        if (cut.must[z(i)])
        {
            s[z(i)] = kWater;
            *changed = true;
        }
    if (*changed)
        return true;
    // Sea pieces with a single way out.
    Flags seen{};
    int pieces = 0;
    Ints piece{};
    for (int i = 0; i < cells; ++i)
    {
        if (s[z(i)] != kWater || seen[z(i)])
            continue;
        flood(
            n, i, &seen, [&](int q) { return s[z(q)] == kWater; },
            [&](int q) { piece[z(q)] = pieces; });
        ++pieces;
    }
    if (pieces > 1 || sea < sea_total)
    {
        Ints exits{};
        Ints exit_cell{};
        for (int i = 0; i < cells; ++i)
        {
            if (s[z(i)] != kOpen)
                continue;
            std::array<int, 4> nb{};
            const int k = neighbours(n, i, &nb);
            std::array<int, 4> counted{-1, -1, -1, -1};
            for (int j = 0; j < k; ++j)
            {
                const int q = nb[z(j)];
                if (s[z(q)] != kWater)
                    continue;
                const int c = piece[z(q)];
                if (std::find(counted.begin(), counted.end(), c) != counted.end())
                    continue;
                counted[z(j)] = c;
                ++exits[z(c)];
                exit_cell[z(c)] = i;
            }
        }
        for (int c = 0; c < pieces; ++c)
        {
            if (exits[z(c)] == 0)
                return false;
            if (exits[z(c)] == 1)
            {
                s[z(exit_cell[z(c)])] = kWater;
                *changed = true;
            }
        }
    }
    return true;
}

// Depth-first walk over wet cells (Tarjan's cut vertices): an open cell whose
// removal cuts off a subtree holding sea must itself be sea (the walk starts on sea).
void Solver::cut_walk(const State &s, int u, int parent, Cut *cut) const
{
    cut->disc[z(u)] = cut->low[z(u)] = cut->time++;
    cut->water[z(u)] = s[z(u)] == kWater ? 1 : 0;
    std::array<int, 4> nb{};
    const int k = neighbours(n, u, &nb);
    for (int j = 0; j < k; ++j)
    {
        const int v = nb[z(j)];
        if (s[z(v)] == kLand)
            continue;
        if (cut->disc[z(v)] < 0)
        {
            cut_walk(s, v, u, cut);
            cut->low[z(u)] = std::min(cut->low[z(u)], cut->low[z(v)]);
            cut->water[z(u)] += cut->water[z(v)];
            if (s[z(u)] == kOpen && cut->low[z(v)] >= cut->disc[z(u)] && cut->water[z(v)] > 0)
                cut->must[z(u)] = true;
        }
        else if (v != parent)
        {
            cut->low[z(u)] = std::min(cut->low[z(u)], cut->disc[z(v)]);
        }
    }
}

bool Solver::propagate(State &s)
{
    for (;;)
    {
        if (++work > budget)
        {
            aborted = true;
            return false;
        }
        bool changed = false;
        int sea = 0;
        int land = 0;
        for (int i = 0; i < cells; ++i)
        {
            sea += s[z(i)] == kWater ? 1 : 0;
            land += s[z(i)] == kLand ? 1 : 0;
        }
        if (sea > sea_total || land > land_total)
            return false;
        if (sea == sea_total || land == land_total)
        {
            const std::uint8_t fill = sea == sea_total ? kLand : kWater;
            for (int i = 0; i < cells; ++i)
                if (s[z(i)] == kOpen)
                {
                    s[z(i)] = fill;
                    changed = true;
                }
            if (changed)
                continue;
        }
        // No 2x2 pool: the last open cell of three sea cells is land.
        for (int r = 0; r + 1 < n; ++r)
        {
            for (int c = 0; c + 1 < n; ++c)
            {
                const int i = r * n + c;
                const int block[4] = {i, i + 1, i + n, i + n + 1};
                int water = 0;
                int open = -1;
                for (int q : block)
                {
                    if (s[z(q)] == kWater)
                        ++water;
                    else if (s[z(q)] == kOpen)
                        open = q;
                }
                if (water == 4)
                    return false;
                if (water == 3 && open >= 0)
                {
                    s[z(open)] = kLand;
                    changed = true;
                }
            }
        }
        if (changed)
            continue;
        if (!propagate_islands(s, &changed))
            return false;
        if (changed)
            continue;
        if (!propagate_sea(s, &changed))
            return false;
        if (!changed)
            return true;
    }
}

void Solver::search(State s)
{
    if (found >= limit || aborted)
        return;
    if (!propagate(s))
        return;
    // Guess next to land first: those guesses settle the most.
    int pick = -1;
    for (int i = 0; i < cells && pick < 0; ++i)
    {
        if (s[z(i)] != kOpen)
            continue;
        std::array<int, 4> nb{};
        const int k = neighbours(n, i, &nb);
        for (int j = 0; j < k; ++j)
            if (s[z(nb[z(j)])] == kLand)
                pick = i;
    }
    for (int i = 0; i < cells && pick < 0; ++i)
        if (s[z(i)] == kOpen)
            pick = i;
    if (pick < 0)
    {
        Cells sea{};
        for (int i = 0; i < cells; ++i)
            sea[z(i)] = s[z(i)] == kWater ? 1 : 0;
        if (valid_solution(p, sea))
        {
            if (found < 2)
                kept[z(found)] = sea;
            ++found;
        }
        return;
    }
    State land = s;
    land[z(pick)] = kLand;
    search(land);
    State water = s;
    water[z(pick)] = kWater;
    search(water);
}

// ---- generator ----

struct Layout
{
    int n = 0;
    Ints owner{}; // island index, -1 for sea
    std::vector<int> size;
    std::vector<int> clue_at; // each island's numbered cell
    Flags touched{};          // cells a change already altered (never changed back)
};

bool sea_connected_without(const Layout &l, int removed)
{
    int total = 0;
    int start = -1;
    for (int i = 0; i < l.n * l.n; ++i)
        if (i != removed && l.owner[z(i)] < 0)
        {
            ++total;
            if (start < 0)
                start = i;
        }
    if (total == 0)
        return false;
    Flags seen{};
    if (removed >= 0)
        seen[z(removed)] = true;
    const int reached =
        flood(l.n, start, &seen, [&](int q) { return l.owner[z(q)] < 0; }, [](int) {});
    return reached == total;
}

// Whether cell i touches an island other than self (-1: any island).
bool touches_other(const Layout &l, int i, int self)
{
    std::array<int, 4> nb{};
    const int k = neighbours(l.n, i, &nb);
    for (int j = 0; j < k; ++j)
    {
        const int o = l.owner[z(nb[z(j)])];
        if (o >= 0 && o != self)
            return true;
    }
    return false;
}

// The one island next to cell, -1 for none, -2 for several.
int single_neighbour(const Layout &l, int cell)
{
    std::array<int, 4> nb{};
    const int k = neighbours(l.n, cell, &nb);
    int island = -1;
    for (int j = 0; j < k; ++j)
    {
        const int o = l.owner[z(nb[z(j)])];
        if (o < 0)
            continue;
        if (island >= 0 && island != o)
            return -2;
        island = o;
    }
    return island;
}

void claim(Layout *l, int cell, int island)
{
    l->owner[z(cell)] = island;
    if (island >= static_cast<int>(l->size.size()))
        l->size.push_back(0);
    ++l->size[z(island)];
}

// Seeds islands in an all-sea board and grows each to a random size while the
// sea stays connected and islands stay apart, then fills every 2x2 pool.
bool build(kit::Rng &rng, int n, int max_size, Layout *out)
{
    Layout l;
    l.n = n;
    const int cells = n * n;
    l.owner.fill(-1);
    const int goal = cells * 2 / 5;
    int land = 0;
    std::vector<int> options;
    while (land < goal)
    {
        options.clear();
        for (int i = 0; i < cells; ++i)
            if (l.owner[z(i)] < 0 && !touches_other(l, i, -1) && sea_connected_without(l, i))
                options.push_back(i);
        if (options.empty())
            break;
        const int id = static_cast<int>(l.size.size());
        claim(&l, options[z(rng.below(static_cast<int>(options.size())))], id);
        ++land;
        const int target = 1 + rng.below(max_size);
        while (l.size[z(id)] < target)
        {
            options.clear();
            for (int i = 0; i < cells; ++i)
                if (l.owner[z(i)] < 0 && single_neighbour(l, i) == id &&
                    sea_connected_without(l, i))
                    options.push_back(i);
            if (options.empty())
                break;
            claim(&l, options[z(rng.below(static_cast<int>(options.size())))], id);
            ++land;
        }
    }
    // Pools: a pool cell joins its one neighbouring island or becomes an island
    // of one, as long as the sea stays connected.
    for (int round = 0; round < cells; ++round)
    {
        std::vector<int> pools;
        for (int r = 0; r + 1 < n; ++r)
            for (int c = 0; c + 1 < n; ++c)
            {
                const int i = r * n + c;
                if (l.owner[z(i)] < 0 && l.owner[z(i + 1)] < 0 && l.owner[z(i + n)] < 0 &&
                    l.owner[z(i + n + 1)] < 0)
                    pools.push_back(i);
            }
        if (pools.empty())
        {
            *out = l;
            return true;
        }
        const int top_left = pools[z(rng.below(static_cast<int>(pools.size())))];
        std::vector<int> block = {top_left, top_left + 1, top_left + n, top_left + n + 1};
        rng.shuffle(block);
        bool fixed = false;
        for (int cell : block)
        {
            const int island = single_neighbour(l, cell);
            if (island == -2 || !sea_connected_without(l, cell))
                continue;
            if (island >= 0 && l.size[z(island)] >= max_size + 2)
                continue;
            claim(&l, cell, island >= 0 ? island : static_cast<int>(l.size.size()));
            fixed = true;
            break;
        }
        if (!fixed)
            return false;
    }
    return false;
}

// Puts each island's number on a random cell of it.
void place_numbers(kit::Rng &rng, Layout *l)
{
    const int cells = l->n * l->n;
    l->clue_at.assign(l->size.size(), -1);
    std::vector<int> members;
    for (int island = 0; island < static_cast<int>(l->size.size()); ++island)
    {
        members.clear();
        for (int i = 0; i < cells; ++i)
            if (l->owner[z(i)] == island)
                members.push_back(i);
        if (!members.empty())
            l->clue_at[z(island)] = members[z(rng.below(static_cast<int>(members.size())))];
    }
}

Puzzle to_puzzle(const Layout &l)
{
    Puzzle p;
    p.side = l.n;
    for (std::size_t island = 0; island < l.clue_at.size(); ++island)
        if (l.clue_at[island] >= 0)
            p.clue[z(l.clue_at[island])] = static_cast<std::uint8_t>(l.size[island]);
    for (int i = 0; i < l.n * l.n; ++i)
        p.solution[z(i)] = l.owner[z(i)] < 0 ? 1 : 0;
    return p;
}

// Whether turning cell into sea would complete a 2x2 pool.
bool makes_pool(const Layout &l, int cell)
{
    const int n = l.n;
    const int r = cell / n;
    const int c = cell % n;
    for (int tr = std::max(0, r - 1); tr <= std::min(r, n - 2); ++tr)
        for (int tc = std::max(0, c - 1); tc <= std::min(c, n - 2); ++tc)
        {
            bool pool = true;
            for (int q : {tr * n + tc, tr * n + tc + 1, (tr + 1) * n + tc, (tr + 1) * n + tc + 1})
                if (q != cell && l.owner[z(q)] >= 0)
                    pool = false;
            if (pool)
                return true;
        }
    return false;
}

bool island_connected_without(const Layout &l, int island, int removed)
{
    int start = -1;
    for (int i = 0; i < l.n * l.n && start < 0; ++i)
        if (i != removed && l.owner[z(i)] == island)
            start = i;
    if (start < 0)
        return false;
    Flags seen{};
    seen[z(removed)] = true;
    const int reached =
        flood(l.n, start, &seen, [&](int q) { return l.owner[z(q)] == island; }, [](int) {});
    return reached == l.size[z(island)] - 1;
}

// ---- small layout changes at one cell; each keeps our solution valid and
// marks the cell so it is never changed back ----

// A sea cell away from every island becomes an island of one.
bool new_island_at(Layout *l, int cell)
{
    if (l->touched[z(cell)] || l->owner[z(cell)] >= 0 || touches_other(*l, cell, -1) ||
        !sea_connected_without(*l, cell))
        return false;
    l->clue_at.push_back(cell);
    claim(l, cell, static_cast<int>(l->size.size()));
    l->touched[z(cell)] = true;
    return true;
}

// A sea cell next to exactly one island joins it.
bool join_at(Layout *l, int cell, int max_size)
{
    if (l->touched[z(cell)] || l->owner[z(cell)] >= 0)
        return false;
    const int island = single_neighbour(*l, cell);
    if (island < 0 || l->size[z(island)] >= max_size || !sea_connected_without(*l, cell))
        return false;
    claim(l, cell, island);
    l->touched[z(cell)] = true;
    return true;
}

// An island cell (not the numbered one) goes back to sea.
bool leave_at(Layout *l, int cell)
{
    const int island = l->owner[z(cell)];
    if (l->touched[z(cell)] || island < 0 || l->clue_at[z(island)] == cell ||
        l->size[z(island)] < 2 || !island_connected_without(*l, island, cell) ||
        makes_pool(*l, cell))
        return false;
    l->owner[z(cell)] = -1;
    --l->size[z(island)];
    if (!sea_connected_without(*l, -1))
    {
        l->owner[z(cell)] = island;
        ++l->size[z(island)];
        return false;
    }
    l->touched[z(cell)] = true;
    return true;
}

// An island cell whose removal cuts its island in two goes back to sea; the
// cut-off part becomes an island with a number of its own.
bool split_at(kit::Rng &rng, Layout *l, int cell)
{
    const int island = l->owner[z(cell)];
    if (l->touched[z(cell)] || island < 0 || l->clue_at[z(island)] == cell ||
        l->size[z(island)] < 3 || makes_pool(*l, cell))
        return false;
    l->owner[z(cell)] = -1;
    Flags seen{};
    const int kept = flood(
        l->n, l->clue_at[z(island)], &seen, [&](int q) { return l->owner[z(q)] == island; },
        [](int) {});
    std::vector<int> part;
    for (int i = 0; i < l->n * l->n; ++i)
        if (l->owner[z(i)] == island && !seen[z(i)])
            part.push_back(i);
    bool ok = !part.empty() && sea_connected_without(*l, -1);
    if (ok)
    {
        // The cut-off part must itself be one piece.
        Flags seen_part{};
        ok = flood(
                 l->n, part.front(), &seen_part,
                 [&](int q) { return l->owner[z(q)] == island && !seen[z(q)]; },
                 [](int) {}) == static_cast<int>(part.size());
    }
    if (!ok)
    {
        l->owner[z(cell)] = island;
        return false;
    }
    const int fresh = static_cast<int>(l->size.size());
    for (int q : part)
        claim(l, q, fresh);
    l->size[z(island)] = kept;
    l->clue_at.push_back(part[z(rng.below(static_cast<int>(part.size())))]);
    l->touched[z(cell)] = true;
    return true;
}

// The island's number moves onto this cell.
bool move_clue_to(Layout *l, int cell)
{
    const int island = l->owner[z(cell)];
    if (l->touched[z(cell)] || island < 0 || l->clue_at[z(island)] == cell)
        return false;
    l->clue_at[z(island)] = cell;
    l->touched[z(cell)] = true;
    return true;
}

// Rules out another solution: any change at a cell where it differs from ours
// breaks it (a new or resized number no longer fits it).
bool rule_out(kit::Rng &rng, int max_size, Layout *l, const Cells &other)
{
    std::vector<int> diff;
    for (int i = 0; i < l->n * l->n; ++i)
        if ((l->owner[z(i)] < 0) != (other[z(i)] != 0))
            diff.push_back(i);
    rng.shuffle(diff);
    for (int cell : diff)
        if (new_island_at(l, cell))
            return true;
    for (int cell : diff)
        if (join_at(l, cell, max_size) || leave_at(l, cell))
            return true;
    return false;
}

// One change at a cell the deductions left open.
bool inform(kit::Rng &rng, int max_size, Layout *l, std::vector<int> open)
{
    rng.shuffle(open);
    for (int cell : open)
        if (new_island_at(l, cell) || split_at(rng, l, cell) || move_clue_to(l, cell))
            return true;
    for (int cell : open)
        if (join_at(l, cell, max_size) || leave_at(l, cell))
            return true;
    return false;
}

// Cells the deductions leave open (the puzzle has a solution, so they cannot
// fail unless the budget runs out: then every cell counts as open).
std::vector<int> open_cells(const Puzzle &p, State *state)
{
    Solver solver(p, 1, 1 << 12);
    *state = solver.initial();
    std::vector<int> open;
    const bool ok = solver.propagate(*state);
    for (int i = 0; i < p.side * p.side; ++i)
        if (!ok || (*state)[z(i)] == kOpen)
            open.push_back(i);
    return open;
}

constexpr int kAttempts = 40;   // fresh layouts before settling for the best one
constexpr int kRounds = 40;     // changes per layout
constexpr int kTrials = 8;      // candidate changes compared per round
constexpr int kSearchOpen = 24; // open cells left when a search takes over
constexpr int kBudget = 3000;   // deduction passes per search

} // namespace

int count_solutions(const Puzzle &puzzle, int limit, Cells *first, int budget)
{
    if (puzzle.side < kMinSide || puzzle.side > kMaxSide)
        return 0;
    Solver solver(puzzle, limit, budget);
    solver.search(solver.initial());
    if (first != nullptr && solver.found > 0)
        *first = solver.kept[0];
    if (solver.aborted && solver.found < limit)
        return -1;
    return solver.found;
}

bool deduce(const Puzzle &puzzle, Cells *state)
{
    if (puzzle.side < kMinSide || puzzle.side > kMaxSide)
        return false;
    Solver solver(puzzle, 1, 1 << 20);
    State s = solver.initial();
    const bool ok = solver.propagate(s);
    *state = s;
    return ok;
}

bool solves_by_logic(const Puzzle &puzzle)
{
    Cells state{};
    if (!deduce(puzzle, &state))
        return false;
    Cells sea{};
    for (int i = 0; i < puzzle.side * puzzle.side; ++i)
    {
        if (state[z(i)] == kOpen)
            return false;
        sea[z(i)] = state[z(i)] == kWater ? 1 : 0;
    }
    return valid_solution(puzzle, sea);
}

Puzzle generate(std::uint64_t seed, int side)
{
    kit::Rng rng(seed);
    const int n = std::clamp(side, kMinSide, kMaxSide);
    const int max_size = n <= 6 ? 5 : 6;
    Puzzle best;
    std::size_t best_open = z(kMaxCells) + 1;
    for (int attempt = 0;; ++attempt)
    {
        Layout layout;
        if (!build(rng, n, max_size, &layout))
            continue;
        place_numbers(rng, &layout);
        // Deduce; where the deductions stall, add information (a new number,
        // a split island, a number moved onto an open cell), keeping the change
        // that leaves the fewest open cells. Once few cells stay open, a search
        // proves the solution unique or finds another, which a change where
        // the two differ rules out.
        for (int round = 0; round < kRounds; ++round)
        {
            const Puzzle p = to_puzzle(layout);
            State state{};
            const std::vector<int> open = open_cells(p, &state);
            if (open.empty())
                return p;
            if (open.size() < best_open)
            {
                best = p;
                best_open = open.size();
            }
            if (static_cast<int>(open.size()) <= kSearchOpen)
            {
                Solver solver(p, 2, kBudget);
                solver.search(state);
                if (!solver.aborted && solver.found == 1)
                    return p;
                if (!solver.aborted && solver.found == 2)
                {
                    const Cells &other =
                        solver.kept[0] == p.solution ? solver.kept[1] : solver.kept[0];
                    if (rule_out(rng, max_size, &layout, other))
                        continue;
                }
            }
            Layout pick;
            std::size_t pick_open = z(kMaxCells) + 1;
            for (int trial = 0; trial < kTrials; ++trial)
            {
                Layout changed = layout;
                if (!inform(rng, max_size, &changed, open))
                    break;
                State after{};
                const std::size_t left = open_cells(to_puzzle(changed), &after).size();
                if (left < pick_open)
                {
                    pick = changed;
                    pick_open = left;
                }
            }
            if (pick_open > z(kMaxCells))
                break;
            layout = pick;
        }
        if (attempt + 1 >= kAttempts && best_open <= z(kMaxCells))
            return best;
    }
}

bool valid_solution(const Puzzle &puzzle, const Cells &sea)
{
    const int n = puzzle.side;
    if (n < kMinSide || n > kMaxSide)
        return false;
    const int cells = n * n;
    int total = 0;
    int start = -1;
    for (int i = 0; i < cells; ++i)
    {
        if (sea[z(i)] == 0)
            continue;
        if (puzzle.clue[z(i)] != 0)
            return false;
        ++total;
        if (start < 0)
            start = i;
    }
    for (int r = 0; r + 1 < n; ++r)
        for (int c = 0; c + 1 < n; ++c)
        {
            const int i = r * n + c;
            if (sea[z(i)] != 0 && sea[z(i + 1)] != 0 && sea[z(i + n)] != 0 &&
                sea[z(i + n + 1)] != 0)
                return false;
        }
    Flags seen{};
    if (start >= 0 &&
        flood(n, start, &seen, [&](int q) { return sea[z(q)] != 0; }, [](int) {}) != total)
        return false;
    for (int i = 0; i < cells; ++i)
    {
        if (sea[z(i)] != 0 || seen[z(i)])
            continue;
        int clues = 0;
        int value = 0;
        const int size = flood(
            n, i, &seen, [&](int q) { return sea[z(q)] == 0; },
            [&](int q)
            {
                if (puzzle.clue[z(q)] != 0)
                {
                    ++clues;
                    value = puzzle.clue[z(q)];
                }
            });
        if (clues != 1 || size != value)
            return false;
    }
    return true;
}

Errors errors(const Puzzle &puzzle, const Cells &marks)
{
    const int n = puzzle.side;
    Errors e;
    if (n < kMinSide || n > kMaxSide)
        return e;
    const int cells = n * n;
    e.bad.assign(z(cells), false);
    const auto sea = [&](int q) { return marks[z(q)] == kSea && puzzle.clue[z(q)] == 0; };
    for (int r = 0; r + 1 < n; ++r)
        for (int c = 0; c + 1 < n; ++c)
        {
            const int i = r * n + c;
            if (sea(i) && sea(i + 1) && sea(i + n) && sea(i + n + 1))
                e.pools.push_back(i);
        }
    // Settled land: numbers and dots joined edge to edge. It is wrong when it
    // holds two numbers, outgrows its number, or is walled in by sea at the
    // wrong size (or with no number at all).
    const auto settled = [&](int q) { return puzzle.clue[z(q)] != 0 || marks[z(q)] == kDot; };
    Flags seen{};
    std::vector<int> members;
    for (int i = 0; i < cells; ++i)
    {
        if (!settled(i) || seen[z(i)])
            continue;
        members.clear();
        int clues = 0;
        int value = 0;
        bool closed = true;
        const int size = flood(n, i, &seen, settled,
                               [&](int q)
                               {
                                   members.push_back(q);
                                   if (puzzle.clue[z(q)] != 0)
                                   {
                                       ++clues;
                                       value = puzzle.clue[z(q)];
                                   }
                                   std::array<int, 4> nb{};
                                   const int k = neighbours(n, q, &nb);
                                   for (int j = 0; j < k; ++j)
                                       if (!settled(nb[z(j)]) && !sea(nb[z(j)]))
                                           closed = false;
                               });
        const bool wrong =
            clues > 1 || (clues == 1 && size > value) || (closed && (clues == 0 || size != value));
        if (wrong)
            for (int q : members)
                e.bad[z(q)] = true;
    }
    // A number walled in by sea with too little room left.
    Flags room{};
    for (int i = 0; i < cells; ++i)
    {
        if (puzzle.clue[z(i)] == 0)
            continue;
        room.fill(false);
        const int space = flood(n, i, &room, [&](int q) { return !sea(q); }, [](int) {});
        if (space < puzzle.clue[z(i)])
            e.bad[z(i)] = true;
    }
    return e;
}

bool solved(const Puzzle &puzzle, const Cells &marks)
{
    Cells sea{};
    for (int i = 0; i < kMaxCells; ++i)
        sea[z(i)] = marks[z(i)] == kSea ? 1 : 0;
    return valid_solution(puzzle, sea);
}

int sea_marked(const Puzzle &puzzle, const Cells &marks)
{
    int count = 0;
    for (int i = 0; i < puzzle.side * puzzle.side && i < kMaxCells; ++i)
        count += marks[z(i)] == kSea && puzzle.clue[z(i)] == 0 ? 1 : 0;
    return count;
}

int sea_target(const Puzzle &puzzle)
{
    int count = 0;
    for (int i = 0; i < puzzle.side * puzzle.side && i < kMaxCells; ++i)
        count += puzzle.solution[z(i)] != 0 ? 1 : 0;
    return count;
}

} // namespace ppz::nurikabe
