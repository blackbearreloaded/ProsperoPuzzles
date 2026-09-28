// ProsperoPuzzles - Sokoban rules: push every crate onto a target, never pull.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/sokoban/core.hpp"

#include "games/kit/puzzle_scene.hpp"

#include <algorithm>
#include <bit>
#include <cstdlib>

namespace ppz::sokoban
{

namespace
{

using Mask = std::array<bool, kMaxCells>;

// A set of up to 128 cells (the solver's bitboard). Shifts take 1..63.
struct Bits
{
    std::uint64_t lo = 0;
    std::uint64_t hi = 0;

    void set(int i)
    {
        (i < 64 ? lo : hi) |= std::uint64_t{1} << (i & 63);
    }
    void reset(int i)
    {
        (i < 64 ? lo : hi) &= ~(std::uint64_t{1} << (i & 63));
    }
    bool test(int i) const
    {
        return (((i < 64 ? lo : hi) >> (i & 63)) & 1u) != 0;
    }
    int lowest() const
    {
        return lo != 0 ? std::countr_zero(lo) : 64 + std::countr_zero(hi);
    }
    Bits operator&(const Bits &o) const
    {
        return {lo & o.lo, hi & o.hi};
    }
    Bits operator|(const Bits &o) const
    {
        return {lo | o.lo, hi | o.hi};
    }
    Bits operator~() const
    {
        return {~lo, ~hi};
    }
    Bits operator<<(int k) const
    {
        return {lo << k, (hi << k) | (lo >> (64 - k))};
    }
    Bits operator>>(int k) const
    {
        return {(lo >> k) | (hi << (64 - k)), hi >> k};
    }
    bool operator==(const Bits &o) const = default;
};

// An open-addressing set of non-zero keys.
class KeySet
{
  public:
    KeySet() : slots_(4096, 0)
    {
    }
    // False when the key was already present.
    bool insert(std::uint64_t key)
    {
        if ((count_ + 1) * 2 > slots_.size())
            grow();
        if (!place(&slots_, key))
            return false;
        ++count_;
        return true;
    }

  private:
    static bool place(std::vector<std::uint64_t> *slots, std::uint64_t key)
    {
        const std::size_t mask = slots->size() - 1;
        for (std::size_t i = (key * 0x9e3779b97f4a7c15ULL) >> 20;; ++i)
        {
            std::uint64_t &slot = (*slots)[i & mask];
            if (slot == key)
                return false;
            if (slot == 0)
            {
                slot = key;
                return true;
            }
        }
    }
    void grow()
    {
        std::vector<std::uint64_t> bigger(slots_.size() * 2, 0);
        for (std::uint64_t key : slots_)
            if (key != 0)
                place(&bigger, key);
        slots_.swap(bigger);
    }

    std::vector<std::uint64_t> slots_;
    std::size_t count_ = 0;
};

struct SizeSpec
{
    int interior;    // the room is carved inside interior x interior cells
    int crates;      // crates and targets
    int rooms;       // rooms tried
    int walks;       // reverse walks per room
    int pulls;       // pulls per walk
    int min_floor;   // smallest acceptable floor
    int max_floor;   // largest acceptable floor (open rooms are dull)
    int pieces;      // internal wall pieces tried (plus up to one more)
    int solve_top;   // the best rooms (by the walk heuristic) that are solved
    int solve_limit; // solver node budget per room
};

constexpr SizeSpec kSpecs[3] = {
    {6, 2, 24, 6, 36, 14, 26, 1, 24, 60000},
    {7, 3, 20, 6, 50, 20, 34, 2, 20, 60000},
    {8, 4, 14, 6, 64, 26, 42, 3, 6, 8000},
};
// A room the solver cannot finish within its budget is scored as this many
// pushes (it is solvable by construction, and likely hard).
constexpr int kAssumedPushes = 18;

std::size_t at(int cell)
{
    return static_cast<std::size_t>(cell);
}

bool floor_cell(std::uint8_t c)
{
    return c == kFloor || c == kTarget;
}

// Marks the cells reachable from `from` without crossing walls or blocked
// cells; returns the smallest reachable index (a canonical keeper position).
int reach(const Puzzle &p, const Mask &blocked, int from, Mask *seen)
{
    seen->fill(false);
    std::array<std::uint8_t, kMaxCells> stack{};
    int top = 0;
    stack[at(top++)] = static_cast<std::uint8_t>(from);
    (*seen)[at(from)] = true;
    int lowest = from;
    while (top > 0)
    {
        const int cell = stack[at(--top)];
        lowest = std::min(lowest, cell);
        for (int d = 0; d < 4; ++d)
        {
            const int next = neighbour(p, cell, d);
            if (next < 0 || (*seen)[at(next)] || blocked[at(next)] || !is_floor(p, next))
                continue;
            (*seen)[at(next)] = true;
            stack[at(top++)] = static_cast<std::uint8_t>(next);
        }
    }
    return lowest;
}

Mask crate_mask(const Puzzle &p, const State &s)
{
    Mask m{};
    for (int i = 0; i < p.crates; ++i)
        m[at(s.crates[at(i)])] = true;
    return m;
}

// ---- room carving ----

// A square work area of side n, cells true for floor.
struct Carve
{
    int n = 0;
    Mask floor{};

    bool open(int c, int r) const
    {
        return c >= 0 && r >= 0 && c < n && r < n && floor[at(r * n + c)];
    }
    int degree(int c, int r) const
    {
        return open(c, r - 1) + open(c, r + 1) + open(c - 1, r) + open(c + 1, r);
    }
    // Size of the component holding (c, r), and its cells in *component.
    int component(int start, Mask *component) const
    {
        component->fill(false);
        std::array<int, kMaxCells> stack{};
        int top = 0;
        stack[at(top++)] = start;
        (*component)[at(start)] = true;
        int count = 0;
        while (top > 0)
        {
            const int cell = stack[at(--top)];
            ++count;
            const int c = cell % n;
            const int r = cell / n;
            const int nb[4][2] = {{c, r - 1}, {c, r + 1}, {c - 1, r}, {c + 1, r}};
            for (const auto &q : nb)
            {
                if (!open(q[0], q[1]) || (*component)[at(q[1] * n + q[0])])
                    continue;
                (*component)[at(q[1] * n + q[0])] = true;
                stack[at(top++)] = q[1] * n + q[0];
            }
        }
        return count;
    }
    int count() const
    {
        return static_cast<int>(std::count(floor.begin(), floor.begin() + n * n, true));
    }
    bool connected() const
    {
        for (int i = 0; i < n * n; ++i)
            if (floor[at(i)])
            {
                Mask m{};
                return component(i, &m) == count();
            }
        return false;
    }
};

// Carves a connected room: a union of rectangles with a few pillars, without
// dead-end cells, cropped to its walls.
bool build_room(kit::Rng &rng, const SizeSpec &spec, Puzzle *out)
{
    Carve cv;
    cv.n = spec.interior + 2;
    const int n = cv.n;
    const int rects = 3 + rng.below(3);
    int placed = 0;
    for (int attempt = 0; attempt < 40 && placed < rects; ++attempt)
    {
        const int w = 2 + rng.below(spec.interior / 2);
        const int h = 2 + rng.below(spec.interior / 2);
        const int x = 1 + rng.below(spec.interior - w + 1);
        const int y = 1 + rng.below(spec.interior - h + 1);
        bool touches = placed == 0;
        for (int r = y; r < y + h && !touches; ++r)
            for (int c = x; c < x + w && !touches; ++c)
                touches = cv.open(c, r) || cv.degree(c, r) > 0;
        if (!touches)
            continue;
        for (int r = y; r < y + h; ++r)
            for (int c = x; c < x + w; ++c)
                cv.floor[at(r * n + c)] = true;
        ++placed;
    }

    // Internal wall pieces (pillars, bars and corners) so the room is not a box.
    // A piece stays only if the floor remains connected without dead ends.
    static constexpr int kPieces[6][3][2] = {
        {{0, 0}, {0, 0}, {0, 0}}, {{0, 0}, {1, 0}, {1, 0}}, {{0, 0}, {0, 1}, {0, 1}},
        {{0, 0}, {1, 0}, {0, 1}}, {{0, 0}, {1, 0}, {1, 1}}, {{0, 0}, {1, 0}, {2, 0}},
    };
    const int pieces = spec.pieces + rng.below(2);
    int walled = 0;
    for (int k = 0; k < pieces * 4 && walled < pieces; ++k)
    {
        const auto &piece = kPieces[rng.below(6)];
        const int x = 1 + rng.below(spec.interior);
        const int y = 1 + rng.below(spec.interior);
        bool fits = true;
        for (const auto &o : piece)
            fits = fits && cv.open(x + o[0], y + o[1]) && cv.degree(x + o[0], y + o[1]) >= 3;
        if (!fits)
            continue;
        const Mask before = cv.floor;
        for (const auto &o : piece)
            cv.floor[at((y + o[1]) * n + x + o[0])] = false;
        bool ok = cv.connected();
        for (int i = 0; i < n * n && ok; ++i)
            ok = !cv.floor[at(i)] || cv.degree(i % n, i / n) >= 2;
        if (ok)
            ++walled;
        else
            cv.floor = before;
    }

    // Keep the largest component, then trim dead ends (which never disconnects).
    Mask best{};
    int best_size = 0;
    Mask done{};
    for (int i = 0; i < n * n; ++i)
    {
        if (!cv.floor[at(i)] || done[at(i)])
            continue;
        Mask comp{};
        const int size = cv.component(i, &comp);
        for (int j = 0; j < n * n; ++j)
            done[at(j)] = done[at(j)] || comp[at(j)];
        if (size > best_size)
        {
            best_size = size;
            best = comp;
        }
    }
    cv.floor = best;
    for (bool changed = true; changed;)
    {
        changed = false;
        for (int r = 0; r < n; ++r)
            for (int c = 0; c < n; ++c)
                if (cv.open(c, r) && cv.degree(c, r) <= 1)
                {
                    cv.floor[at(r * n + c)] = false;
                    changed = true;
                }
    }
    const int floor_cells = cv.count();
    if (floor_cells < spec.min_floor || floor_cells > spec.max_floor)
        return false;

    // Crop to the floor's bounding box plus a ring of wall.
    int c0 = n;
    int c1 = -1;
    int r0 = n;
    int r1 = -1;
    for (int r = 0; r < n; ++r)
        for (int c = 0; c < n; ++c)
            if (cv.open(c, r))
            {
                c0 = std::min(c0, c);
                c1 = std::max(c1, c);
                r0 = std::min(r0, r);
                r1 = std::max(r1, r);
            }
    if (c1 - c0 < 3 || r1 - r0 < 3)
        return false;
    Puzzle p;
    p.cols = c1 - c0 + 3;
    p.rows = r1 - r0 + 3;
    for (int r = 0; r < p.rows; ++r)
    {
        for (int c = 0; c < p.cols; ++c)
        {
            const int sc = c + c0 - 1;
            const int sr = r + r0 - 1;
            std::uint8_t type = kVoid;
            if (cv.open(sc, sr))
                type = kFloor;
            else
                for (int dr = -1; dr <= 1; ++dr)
                    for (int dc = -1; dc <= 1; ++dc)
                        if (cv.open(sc + dc, sr + dr))
                            type = kWall;
            p.cell[at(r * p.cols + c)] = type;
        }
    }
    *out = p;
    return true;
}

// ---- reverse play ----

// Puts the targets on floor cells from which a crate can be pulled, preferring
// cells away from the walls.
bool place_targets(kit::Rng &rng, int crates, Puzzle *p)
{
    std::vector<int> pool;
    for (int cell = 0; cell < p->cols * p->rows; ++cell)
    {
        if (p->cell[at(cell)] != kFloor)
            continue;
        int open = 0;
        bool pullable = false;
        for (int d = 0; d < 4; ++d)
        {
            const int a = neighbour(*p, cell, d);
            if (a < 0 || !is_floor(*p, a))
                continue;
            ++open;
            const int b = neighbour(*p, a, d);
            pullable = pullable || (b >= 0 && is_floor(*p, b));
        }
        if (!pullable)
            continue;
        for (int k = 0; k < open * open; ++k) // weight by openness
            pool.push_back(cell);
    }
    p->crates = 0;
    while (p->crates < crates)
    {
        if (pool.empty())
            return false;
        const int cell = pool[at(rng.below(static_cast<int>(pool.size())))];
        pool.erase(std::remove(pool.begin(), pool.end(), cell), pool.end());
        p->cell[at(cell)] = kTarget;
        p->start.crates[at(p->crates++)] = static_cast<std::uint8_t>(cell);
    }
    return true;
}

// A start position: no crate on a target, none stuck in a dead corner.
bool fair_start(const Puzzle &p, const State &s)
{
    for (int i = 0; i < p.crates; ++i)
    {
        const int cell = s.crates[at(i)];
        if (p.cell[at(cell)] == kTarget || dead_corner(p, cell))
            return false;
    }
    return true;
}

struct Candidate
{
    State state;
    int score = -1;
};

// Starts from the solved position and plays random reverse moves: the keeper
// walks anywhere it can reach and pulls a crate one step as it backs away.
// Every pull undoes a legal push, so the best position found is solvable.
Candidate reverse_walk(kit::Rng &rng, const Puzzle &p, int pulls)
{
    State s = p.start; // crates on their targets
    std::vector<int> free;
    for (int cell = 0; cell < p.cols * p.rows; ++cell)
        if (p.cell[at(cell)] == kFloor)
            free.push_back(cell);
    if (free.empty())
        return {};
    s.keeper = static_cast<std::uint8_t>(free[at(rng.below(static_cast<int>(free.size())))]);

    std::array<int, kMaxCrates> last_dir{};
    last_dir.fill(-1);
    int changes = 0;
    int last_crate = -1;
    Candidate best;
    struct Pull
    {
        int crate;
        int dir;
    };
    std::vector<Pull> options;
    std::vector<Pull> same;
    for (int step = 0; step < pulls; ++step)
    {
        const Mask blocked = crate_mask(p, s);
        Mask seen{};
        reach(p, blocked, s.keeper, &seen);
        options.clear();
        same.clear();
        for (int i = 0; i < p.crates; ++i)
        {
            for (int d = 0; d < 4; ++d)
            {
                const int stand = neighbour(p, s.crates[at(i)], d);
                if (stand < 0 || !seen[at(stand)])
                    continue;
                const int back = neighbour(p, stand, d);
                if (back < 0 || !is_floor(p, back) || blocked[at(back)])
                    continue;
                options.push_back({i, d});
                if (i == last_crate)
                    same.push_back({i, d});
            }
        }
        if (options.empty())
            break;
        const std::vector<Pull> &from = !same.empty() && rng.below(3) != 0 ? same : options;
        const Pull pull = from[at(rng.below(static_cast<int>(from.size())))];
        const int stand = neighbour(p, s.crates[at(pull.crate)], pull.dir);
        s.crates[at(pull.crate)] = static_cast<std::uint8_t>(stand);
        s.keeper = static_cast<std::uint8_t>(neighbour(p, stand, pull.dir));
        if (last_dir[at(pull.crate)] >= 0 && last_dir[at(pull.crate)] != pull.dir)
            ++changes;
        last_dir[at(pull.crate)] = pull.dir;
        last_crate = pull.crate;

        if (!fair_start(p, s))
            continue;
        int distance = 0;
        for (int i = 0; i < p.crates; ++i)
        {
            const int a = s.crates[at(i)];
            const int b = p.start.crates[at(i)];
            distance += std::abs(a % p.cols - b % p.cols) + std::abs(a / p.cols - b / p.cols);
        }
        const int score = distance * 2 + changes * 3;
        if (score > best.score)
            best = {s, score};
    }
    if (best.score < 0)
        return best;
    // The keeper may start anywhere it can reach in that position.
    Mask seen{};
    reach(p, crate_mask(p, best.state), best.state.keeper, &seen);
    std::vector<int> spots;
    for (int cell = 0; cell < p.cols * p.rows; ++cell)
        if (seen[at(cell)])
            spots.push_back(cell);
    best.state.keeper =
        static_cast<std::uint8_t>(spots[at(rng.below(static_cast<int>(spots.size())))]);
    return best;
}

// A fixed little room, only used if generation somehow finds nothing.
Puzzle fallback()
{
    static const char *const kRows[] = {"#######", "#.....#", "#.$.$.#", "#..@..#",
                                        "#.o.o.#", "#.....#", "#######"};
    Puzzle p;
    p.cols = 7;
    p.rows = 7;
    for (int r = 0; r < 7; ++r)
    {
        for (int c = 0; c < 7; ++c)
        {
            const char ch = kRows[r][c];
            const int cell = r * 7 + c;
            p.cell[at(cell)] = ch == '#' ? kWall : ch == 'o' ? kTarget : kFloor;
            if (ch == '$')
                p.start.crates[at(p.crates++)] = static_cast<std::uint8_t>(cell);
            if (ch == '@')
                p.start.keeper = static_cast<std::uint8_t>(cell);
        }
    }
    p.min_pushes = 2;
    return p;
}

// A keeper path from `from` to `to` avoiding crates, appended to moves.
bool walk_path(const Puzzle &p, const Mask &blocked, int from, int to, std::vector<int> *moves)
{
    std::array<int, kMaxCells> via{};
    via.fill(-1);
    std::array<int, kMaxCells> queue{};
    int head = 0;
    int tail = 0;
    queue[at(tail++)] = from;
    via[at(from)] = 4; // the start
    while (head < tail && via[at(to)] < 0)
    {
        const int cell = queue[at(head++)];
        for (int d = 0; d < 4; ++d)
        {
            const int next = neighbour(p, cell, d);
            if (next < 0 || via[at(next)] >= 0 || blocked[at(next)] || !is_floor(p, next))
                continue;
            via[at(next)] = d;
            queue[at(tail++)] = next;
        }
    }
    if (via[at(to)] < 0)
        return false;
    std::vector<int> path;
    for (int cell = to; cell != from;)
    {
        const int d = via[at(cell)];
        path.push_back(d);
        cell = neighbour(p, cell, d ^ 1);
    }
    moves->insert(moves->end(), path.rbegin(), path.rend());
    return true;
}

} // namespace

bool is_floor(const Puzzle &puzzle, int cell)
{
    return cell >= 0 && cell < puzzle.cols * puzzle.rows && floor_cell(puzzle.cell[at(cell)]);
}

int neighbour(const Puzzle &puzzle, int cell, int dir)
{
    if (dir < 0 || dir > 3 || puzzle.cols <= 0)
        return -1;
    const int c = cell % puzzle.cols + kDirCol[dir];
    const int r = cell / puzzle.cols + kDirRow[dir];
    if (c < 0 || r < 0 || c >= puzzle.cols || r >= puzzle.rows)
        return -1;
    return r * puzzle.cols + c;
}

int crate_at(const Puzzle &puzzle, const State &state, int cell)
{
    for (int i = 0; i < puzzle.crates; ++i)
        if (state.crates[at(i)] == cell)
            return i;
    return -1;
}

Step move(const Puzzle &puzzle, State *state, int dir, int *pushed)
{
    if (pushed != nullptr)
        *pushed = -1;
    const int next = neighbour(puzzle, state->keeper, dir);
    if (!is_floor(puzzle, next))
        return Step::none;
    const int crate = crate_at(puzzle, *state, next);
    if (crate < 0)
    {
        state->keeper = static_cast<std::uint8_t>(next);
        return Step::walk;
    }
    const int beyond = neighbour(puzzle, next, dir);
    if (!is_floor(puzzle, beyond) || crate_at(puzzle, *state, beyond) >= 0)
        return Step::blocked;
    state->crates[at(crate)] = static_cast<std::uint8_t>(beyond);
    state->keeper = static_cast<std::uint8_t>(next);
    if (pushed != nullptr)
        *pushed = crate;
    return Step::push;
}

int crates_on_targets(const Puzzle &puzzle, const State &state)
{
    int on = 0;
    for (int i = 0; i < puzzle.crates; ++i)
        on += puzzle.cell[at(state.crates[at(i)])] == kTarget ? 1 : 0;
    return on;
}

bool solved(const Puzzle &puzzle, const State &state)
{
    return puzzle.crates > 0 && crates_on_targets(puzzle, state) == puzzle.crates;
}

bool dead_corner(const Puzzle &puzzle, int cell)
{
    if (!is_floor(puzzle, cell) || puzzle.cell[at(cell)] == kTarget)
        return false;
    const auto wall = [&](int d) { return !is_floor(puzzle, neighbour(puzzle, cell, d)); };
    return (wall(0) || wall(1)) && (wall(2) || wall(3));
}

int solve(const Puzzle &puzzle, const State &state, int node_limit, std::vector<int> *moves)
{
    struct Node
    {
        State state;
        int parent;
        int crate;
        int dir;
        int depth;
    };
    const int n = puzzle.crates;
    const int cols = puzzle.cols;
    if (n <= 0 || n > kMaxCrates || cols < 2 || cols >= 64 || puzzle.rows * cols > kMaxCells)
        return -1;
    // Cell sets as bitboards, so the keeper's reach is a few shifts per step.
    Bits floor;
    Bits not_first; // cells that have a left neighbour
    Bits not_last;  // cells that have a right neighbour
    std::array<bool, kMaxCells> dead{};
    for (int cell = 0; cell < puzzle.rows * cols; ++cell)
    {
        if (is_floor(puzzle, cell))
            floor.set(cell);
        if (cell % cols > 0)
            not_first.set(cell);
        if (cell % cols < cols - 1)
            not_last.set(cell);
        dead[at(cell)] = dead_corner(puzzle, cell);
    }
    const auto crate_bits = [&](const State &s)
    {
        Bits b;
        for (int i = 0; i < n; ++i)
            b.set(s.crates[at(i)]);
        return b;
    };
    const auto reach_bits = [&](int from, const Bits &crates)
    {
        const Bits open = floor & ~crates;
        Bits r;
        r.set(from);
        for (;;)
        {
            const Bits next =
                (r | ((r & not_first) >> 1) | ((r & not_last) << 1) | (r >> cols) | (r << cols)) &
                open;
            if (next == r)
                return r;
            r = next;
        }
    };
    const auto key = [&](const State &s, const Bits &area)
    {
        std::array<std::uint8_t, kMaxCrates> sorted = s.crates;
        std::sort(sorted.begin(), sorted.begin() + n);
        auto k = static_cast<std::uint64_t>(area.lowest());
        for (int i = 0; i < n; ++i)
            k = (k << 7) | sorted[at(i)];
        return k;
    };
    std::vector<Node> nodes;
    KeySet visited;
    nodes.push_back({state, -1, -1, -1, 0});
    visited.insert(key(state, reach_bits(state.keeper, crate_bits(state))));
    for (std::size_t head = 0; head < nodes.size(); ++head)
    {
        const Node current = nodes[head];
        if (solved(puzzle, current.state))
        {
            if (moves != nullptr)
            {
                std::vector<const Node *> chain;
                for (int i = static_cast<int>(head); i > 0; i = nodes[at(i)].parent)
                    chain.push_back(&nodes[at(i)]);
                moves->clear();
                State s = state;
                for (auto it = chain.rbegin(); it != chain.rend(); ++it)
                {
                    const int crate = s.crates[at((*it)->crate)];
                    const int stand = neighbour(puzzle, crate, (*it)->dir ^ 1);
                    walk_path(puzzle, crate_mask(puzzle, s), s.keeper, stand, moves);
                    moves->push_back((*it)->dir);
                    s.keeper = static_cast<std::uint8_t>(crate);
                    s.crates[at((*it)->crate)] =
                        static_cast<std::uint8_t>(neighbour(puzzle, crate, (*it)->dir));
                }
            }
            return current.depth;
        }
        const Bits blocked = crate_bits(current.state);
        const Bits seen = reach_bits(current.state.keeper, blocked);
        for (int i = 0; i < n; ++i)
        {
            const int crate = current.state.crates[at(i)];
            for (int d = 0; d < 4; ++d)
            {
                const int stand = neighbour(puzzle, crate, d ^ 1);
                const int dest = neighbour(puzzle, crate, d);
                if (stand < 0 || dest < 0 || !seen.test(stand) || !floor.test(dest) ||
                    blocked.test(dest) || dead[at(dest)])
                    continue;
                Node next{current.state, static_cast<int>(head), i, d, current.depth + 1};
                next.state.crates[at(i)] = static_cast<std::uint8_t>(dest);
                next.state.keeper = static_cast<std::uint8_t>(crate);
                Bits moved = blocked;
                moved.reset(crate);
                moved.set(dest);
                if (!visited.insert(key(next.state, reach_bits(crate, moved))))
                    continue;
                nodes.push_back(next);
                if (static_cast<int>(nodes.size()) > node_limit)
                    return -1;
            }
        }
    }
    return -1;
}

Puzzle generate(std::uint64_t seed, int size)
{
    const SizeSpec &spec = kSpecs[std::clamp(size, 0, 2)];
    kit::Rng rng(seed * 0x2545f4914f6cdd1dULL + static_cast<std::uint64_t>(size) + 1);
    std::vector<std::pair<int, Puzzle>> candidates; // (heuristic score, puzzle)
    int rooms = 0;
    for (int attempt = 0; attempt < 400 && rooms < spec.rooms; ++attempt)
    {
        Puzzle room;
        if (!build_room(rng, spec, &room))
            continue;
        ++rooms;
        Puzzle room_best;
        int room_score = -1;
        for (int walk = 0; walk < spec.walks; ++walk)
        {
            Puzzle p = room;
            if (!place_targets(rng, spec.crates, &p))
                break;
            const Candidate c = reverse_walk(rng, p, spec.pulls);
            if (c.score > room_score)
            {
                room_score = c.score;
                room_best = p;
                room_best.start = c.state;
            }
        }
        if (room_score >= 0)
            candidates.emplace_back(room_score, room_best);
    }
    if (candidates.empty())
        return fallback();

    // The most promising rooms are solved; the most pushes wins.
    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const auto &a, const auto &b) { return a.first > b.first; });
    const int solved_rooms = std::min(spec.solve_top, static_cast<int>(candidates.size()));
    long best_score = -1;
    std::size_t best = 0;
    for (int i = 0; i < solved_rooms; ++i)
    {
        Puzzle &p = candidates[at(i)].second;
        // Over budget leaves min_pushes at -1 (unknown); the room stays solvable.
        p.min_pushes = solve(p, p.start, spec.solve_limit);
        const long score =
            candidates[at(i)].first + 100L * (p.min_pushes < 0 ? kAssumedPushes : p.min_pushes);
        if (score > best_score)
        {
            best_score = score;
            best = at(i);
        }
    }
    return candidates[best].second;
}

} // namespace ppz::sokoban
