// ProsperoPuzzles - Kakuro rules: digit runs that add up to their clues.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/kakuro/core.hpp"

#include "games/kit/puzzle_scene.hpp"

#include <algorithm>
#include <bit>

namespace ppz::kakuro
{

namespace
{

template <typename T> auto &at(T &items, int i)
{
    return items[static_cast<std::size_t>(i)];
}

constexpr int kMaxSum = 45;
using Masks = std::array<std::uint16_t, kMaxCells>; // digit d is bit d

int mask_sum(unsigned mask)
{
    int sum = 0;
    for (int d = 1; d <= 9; ++d)
        sum += (mask & (1u << d)) != 0 ? d : 0;
    return sum;
}

// For every (count, sum): the digit sets that make it, and their union.
struct Combos
{
    std::array<std::array<std::vector<std::uint16_t>, kMaxSum + 1>, kMaxRun + 1> sets;
    std::array<std::array<std::uint16_t, kMaxSum + 1>, kMaxRun + 1> digits{};

    Combos()
    {
        for (unsigned bits = 1; bits < 512; ++bits)
        {
            const auto mask = static_cast<std::uint16_t>(bits << 1);
            const int count = std::popcount(bits);
            const int sum = mask_sum(mask);
            at(at(sets, count), sum).push_back(mask);
            at(at(digits, count), sum) |= mask;
        }
    }
};

const Combos &combos()
{
    static const Combos table;
    return table;
}

// The runs and, for every cell, the index of its across and down run.
struct Layout
{
    std::vector<Run> runs;
    std::array<int, kMaxCells> across{};
    std::array<int, kMaxCells> down{};
    std::vector<int> whites;
};

Layout layout(const Puzzle &p)
{
    Layout lay;
    lay.runs = runs(p);
    lay.across.fill(-1);
    lay.down.fill(-1);
    for (int r = 0; r < static_cast<int>(lay.runs.size()); ++r)
    {
        const Run &run = at(lay.runs, r);
        for (int k = 0; k < run.length; ++k)
            at(run.across ? lay.across : lay.down, at(run.cells, k)) = r;
    }
    for (int i = 0; i < p.side * p.side; ++i)
        if (p.white(i))
            lay.whites.push_back(i);
    return lay;
}

struct Solver
{
    const Layout &lay;
    int limit;
    long node_limit;
    long nodes = 0;
    bool aborted = false;
    int found = 0;
    std::array<Digits, 2> kept{}; // the first two solutions

    // Narrows every cell to the digits that still fit both of its runs.
    bool propagate(Masks &m) const
    {
        bool changed = true;
        while (changed)
        {
            changed = false;
            for (const Run &run : lay.runs)
            {
                std::uint16_t fixed = 0;
                std::uint16_t open_union = 0;
                int fixed_sum = 0;
                int open = 0;
                for (int k = 0; k < run.length; ++k)
                {
                    const std::uint16_t mk = at(m, at(run.cells, k));
                    if (mk == 0)
                        return false;
                    if (std::has_single_bit(mk))
                    {
                        if ((fixed & mk) != 0)
                            return false;
                        fixed = static_cast<std::uint16_t>(fixed | mk);
                        fixed_sum += std::countr_zero(mk);
                    }
                    else
                    {
                        ++open;
                        open_union = static_cast<std::uint16_t>(open_union | mk);
                    }
                }
                const int rest = run.sum - fixed_sum;
                if (open == 0)
                {
                    if (rest != 0)
                        return false;
                    continue;
                }
                if (rest < 1 || rest > kMaxSum)
                    return false;
                std::uint16_t allowed = 0;
                for (std::uint16_t set : at(at(combos().sets, open), rest))
                {
                    if ((set & fixed) != 0 || (set & ~open_union) != 0)
                        continue;
                    bool fits = true;
                    for (int k = 0; k < run.length && fits; ++k)
                    {
                        const std::uint16_t mk = at(m, at(run.cells, k));
                        if (!std::has_single_bit(mk) && (mk & set) == 0)
                            fits = false;
                    }
                    if (fits)
                        allowed = static_cast<std::uint16_t>(allowed | set);
                }
                if (allowed == 0)
                    return false;
                for (int k = 0; k < run.length; ++k)
                {
                    std::uint16_t &mk = at(m, at(run.cells, k));
                    if (std::has_single_bit(mk))
                        continue;
                    const auto narrowed = static_cast<std::uint16_t>(mk & allowed);
                    if (narrowed == 0)
                        return false;
                    if (narrowed != mk)
                    {
                        mk = narrowed;
                        changed = true;
                    }
                }
            }
        }
        return true;
    }

    void search(Masks m)
    {
        if (found >= limit || aborted)
            return;
        if (++nodes > node_limit)
        {
            aborted = true;
            return;
        }
        if (!propagate(m))
            return;
        int best = -1;
        int best_count = 10;
        for (int cell : lay.whites)
        {
            const int count = std::popcount(at(m, cell));
            if (count > 1 && count < best_count)
            {
                best = cell;
                best_count = count;
            }
        }
        if (best < 0)
        {
            if (found < 2)
            {
                Digits &out = at(kept, found);
                out.fill(0);
                for (int cell : lay.whites)
                    at(out, cell) = static_cast<std::uint8_t>(std::countr_zero(at(m, cell)));
            }
            ++found;
            return;
        }
        const std::uint16_t options = at(m, best);
        for (int d = 1; d <= 9; ++d)
        {
            if ((options & (1u << d)) == 0)
                continue;
            Masks next = m;
            at(next, best) = static_cast<std::uint16_t>(1u << d);
            search(next);
            if (found >= limit || aborted)
                return;
        }
    }

    // Every cell starts with the digits that can make both of its clues.
    Masks initial() const
    {
        Masks m{};
        for (int cell : lay.whites)
        {
            const Run &a = at(lay.runs, at(lay.across, cell));
            const Run &d = at(lay.runs, at(lay.down, cell));
            const auto &digits = combos().digits;
            at(m, cell) = static_cast<std::uint16_t>(at(at(digits, a.length), a.sum) &
                                                     at(at(digits, d.length), d.sum));
        }
        return m;
    }

    void run()
    {
        search(initial());
    }

    // How open the puzzle stays after propagation alone (lower is tighter;
    // whites.size() means solved without guessing).
    int openness() const
    {
        Masks m = initial();
        if (!propagate(m))
            return 1 << 20;
        int total = 0;
        for (int cell : lay.whites)
            total += std::popcount(at(m, cell));
        return total;
    }
};

// The length of the white line through a cell, across or down.
int line_length(const std::array<bool, kMaxCells> &white, int n, int row, int col, bool across)
{
    const int dr = across ? 0 : 1;
    const int dc = across ? 1 : 0;
    int length = 1;
    for (int r = row - dr, c = col - dc; r >= 0 && c >= 0 && at(white, r * n + c); r -= dr, c -= dc)
        ++length;
    for (int r = row + dr, c = col + dc; r < n && c < n && at(white, r * n + c); r += dr, c += dc)
        ++length;
    return length;
}

bool whites_connected(const std::array<bool, kMaxCells> &white, int n, int count)
{
    std::vector<int> stack;
    std::array<bool, kMaxCells> seen{};
    for (int i = 0; i < n * n && stack.empty(); ++i)
    {
        if (at(white, i))
        {
            stack.push_back(i);
            at(seen, i) = true;
        }
    }
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
            if (!at(white, j) || at(seen, j))
                continue;
            at(seen, j) = true;
            stack.push_back(j);
        }
    }
    return reached == count;
}

// A 180-degree symmetric block pattern whose white cells all sit in runs of
// two or more, connected and reasonably dense.
bool make_pattern(kit::Rng &rng, int n, float block_fraction, int max_run,
                  std::array<bool, kMaxCells> *white)
{
    white->fill(false);
    for (int r = 1; r < n; ++r)
        for (int c = 1; c < n; ++c)
            at(*white, r * n + c) = true;
    const int m = n - 1;
    const int target = static_cast<int>(static_cast<float>(m * m) * block_fraction + 0.5f);
    // Every white cell needs an across and a down partner.
    const auto no_lone_cells = [&]()
    {
        for (int r = 1; r < n; ++r)
            for (int c = 1; c < n; ++c)
                if (at(*white, r * n + c) && (line_length(*white, n, r, c, true) < 2 ||
                                              line_length(*white, n, r, c, false) < 2))
                    return false;
        return true;
    };
    // Tries a block and its mirror; keeps them only if no lone cell appears.
    const auto try_block = [&](int cell)
    {
        const int mirror = (n - cell / n) * n + (n - cell % n);
        if (!at(*white, cell))
            return 0;
        at(*white, cell) = false;
        at(*white, mirror) = false;
        if (no_lone_cells())
            return cell == mirror ? 1 : 2;
        at(*white, cell) = true;
        at(*white, mirror) = true;
        return 0;
    };
    std::vector<int> cells;
    for (int r = 1; r < n; ++r)
        for (int c = 1; c < n; ++c)
            if (r * n + c <= (n - r) * n + (n - c))
                cells.push_back(r * n + c);
    rng.shuffle(cells);
    int blocks = 0;
    for (int cell : cells)
    {
        if (blocks >= target)
            break;
        blocks += try_block(cell);
    }
    // Long runs are hard to pin down: cut them where a block fits.
    for (int pass = 0; pass < 2 * n; ++pass)
    {
        int longest = -1;
        for (int i = 0; i < n * n && longest < 0; ++i)
            if (at(*white, i) && std::max(line_length(*white, n, i / n, i % n, true),
                                          line_length(*white, n, i / n, i % n, false)) > max_run)
                longest = i;
        if (longest < 0)
            break;
        const bool across = line_length(*white, n, longest / n, longest % n, true) > max_run;
        const int step = across ? 1 : n;
        int start = longest;
        while (at(*white, start - step))
            start -= step;
        std::vector<int> line;
        for (int cell = start;
             cell < n * n && at(*white, cell) && (!across || cell / n == start / n); cell += step)
            line.push_back(cell);
        rng.shuffle(line);
        for (int cell : line)
            if (try_block(cell) > 0)
                break;
    }
    const int count = static_cast<int>(std::count(white->begin(), white->end(), true));
    const float fraction = static_cast<float>(count) / static_cast<float>(n * n);
    if (fraction < 0.5f - block_fraction * 0.5f || fraction > 0.74f)
        return false;
    return whites_connected(*white, n, count);
}

// Random digits with no repeat in any run (the clues follow from them).
bool fill_digits(kit::Rng &rng, const Layout &lay, std::size_t index,
                 std::vector<std::uint16_t> &used, Puzzle *p, int *steps)
{
    if (index == lay.whites.size())
        return true;
    if (++*steps > 4000)
        return false;
    const int cell = lay.whites[index];
    const int a = at(lay.across, cell);
    const int d = at(lay.down, cell);
    const unsigned taken = at(used, a) | at(used, d);
    std::vector<int> options;
    for (int digit = 1; digit <= 9; ++digit)
        if ((taken & (1u << digit)) == 0)
            options.push_back(digit);
    rng.shuffle(options);
    for (int digit : options)
    {
        const auto bit = static_cast<std::uint16_t>(1u << digit);
        at(p->solution, cell) = static_cast<std::uint8_t>(digit);
        at(used, a) = static_cast<std::uint16_t>(at(used, a) | bit);
        at(used, d) = static_cast<std::uint16_t>(at(used, d) | bit);
        if (fill_digits(rng, lay, index + 1, used, p, steps))
            return true;
        at(used, a) = static_cast<std::uint16_t>(at(used, a) & ~bit);
        at(used, d) = static_cast<std::uint16_t>(at(used, d) & ~bit);
    }
    at(p->solution, cell) = 1; // keep the cell white for the layout
    return false;
}

// Changes the digit of one cell that logic alone cannot pin down yet, so its
// two clues change. Of the possible changes it keeps the one that leaves the
// fewest candidates open; returns that score (or -1 when nothing can change).
int tighten(kit::Rng &rng, Layout lay, Puzzle *p)
{
    const Solver probe{lay, 1, 0};
    Masks m = probe.initial();
    probe.propagate(m);
    std::vector<int> open;
    for (int cell : lay.whites)
        if (!std::has_single_bit(at(m, cell)))
            open.push_back(cell);
    rng.shuffle(open);
    if (open.size() > 5)
        open.resize(5);
    const int solved_score = static_cast<int>(lay.whites.size());
    int best_score = 1 << 30;
    int best_cell = -1;
    int best_digit = 0;
    for (int cell : open)
    {
        if (best_score == solved_score)
            break;
        Run &across = at(lay.runs, at(lay.across, cell));
        Run &down = at(lay.runs, at(lay.down, cell));
        unsigned taken = 0;
        for (const Run *run : {&across, &down})
            for (int k = 0; k < run->length; ++k)
                taken |= 1u << at(p->solution, at(run->cells, k));
        const int old = at(p->solution, cell);
        for (int digit = 1; digit <= 9 && best_score > solved_score; ++digit)
        {
            if ((taken & (1u << digit)) != 0)
                continue;
            across.sum += digit - old;
            down.sum += digit - old;
            const int score = Solver{lay, 1, 0}.openness();
            across.sum -= digit - old;
            down.sum -= digit - old;
            if (score < best_score)
            {
                best_score = score;
                best_cell = cell;
                best_digit = digit;
            }
        }
    }
    if (best_cell < 0)
        return -1;
    at(p->solution, best_cell) = static_cast<std::uint8_t>(best_digit);
    compute_clues(p);
    return best_score;
}

} // namespace

std::vector<Run> runs(const Puzzle &p)
{
    std::vector<Run> out;
    const int n = p.side;
    for (const bool across : {true, false})
    {
        for (int row = 0; row < n; ++row)
        {
            for (int col = 0; col < n; ++col)
            {
                const int i = row * n + col;
                const int before = across ? col - 1 : row - 1;
                if (!p.white(i) || before < 0)
                    continue;
                const int clue = across ? i - 1 : i - n;
                if (p.white(clue))
                    continue;
                Run run;
                run.clue = clue;
                run.across = across;
                run.sum = across ? at(p.right, clue) : at(p.down, clue);
                for (int r = row, c = col;
                     r < n && c < n && p.white(r * n + c) && run.length < kMaxRun;
                     r += across ? 0 : 1, c += across ? 1 : 0)
                    at(run.cells, run.length++) = static_cast<std::uint8_t>(r * n + c);
                out.push_back(run);
            }
        }
    }
    return out;
}

void compute_clues(Puzzle *p)
{
    p->right.fill(0);
    p->down.fill(0);
    for (const Run &run : runs(*p))
    {
        int sum = 0;
        for (int k = 0; k < run.length; ++k)
            sum += at(p->solution, at(run.cells, k));
        at(run.across ? p->right : p->down, run.clue) = static_cast<std::uint8_t>(sum);
    }
}

bool well_formed(const Puzzle &p)
{
    const int n = p.side;
    if (n < kMinSide || n > kMaxSide)
        return false;
    for (int i = 0; i < kMaxCells; ++i)
    {
        const bool inside = i < n * n;
        if (at(p.solution, i) > 9 ||
            (!inside && (at(p.solution, i) != 0 || at(p.right, i) != 0 || at(p.down, i) != 0)))
            return false;
        if (inside && (i < n || i % n == 0) && p.white(i))
            return false;
    }
    Puzzle check = p;
    compute_clues(&check);
    if (check.right != p.right || check.down != p.down)
        return false;
    std::array<int, kMaxCells> covered{};
    for (const Run &run : runs(p))
    {
        if (run.length < 2)
            return false;
        unsigned seen = 0;
        for (int k = 0; k < run.length; ++k)
        {
            const int cell = at(run.cells, k);
            const unsigned bit = 1u << at(p.solution, cell);
            if ((seen & bit) != 0)
                return false;
            seen |= bit;
            ++at(covered, cell);
        }
    }
    for (int i = 0; i < n * n; ++i)
        if (p.white(i) && at(covered, i) != 2)
            return false; // a run longer than nine cells
    return true;
}

int count_solutions(const Puzzle &puzzle, int limit, Digits *first)
{
    const Layout lay = layout(puzzle);
    Solver solver{lay, limit, 5'000'000};
    solver.run();
    if (first != nullptr && solver.found > 0)
        *first = solver.kept[0];
    return solver.found;
}

Puzzle generate(std::uint64_t seed, int side)
{
    kit::Rng rng(seed);
    const int n = std::clamp(side, kMinSide, kMaxSide);
    float block_fraction = 0.2f;
    for (int attempt = 1;; ++attempt)
    {
        // Denser boards are harder to make unique: ease off after a while.
        if (attempt % 6 == 0)
            block_fraction = std::min(0.4f, block_fraction + 0.02f);
        std::array<bool, kMaxCells> white{};
        if (!make_pattern(rng, n, block_fraction, n >= 10 ? 6 : kMaxRun, &white))
            continue;
        Puzzle p;
        p.side = n;
        for (int i = 0; i < n * n; ++i)
            at(p.solution, i) = at(white, i) ? 1 : 0;
        const Layout shape = layout(p);
        for (int fill = 0; fill < 3; ++fill)
        {
            std::vector<std::uint16_t> used(shape.runs.size(), 0);
            int steps = 0;
            if (!fill_digits(rng, shape, 0, used, &p, &steps))
                break;
            compute_clues(&p);
            // Local search: nudge digits until logic alone pins every cell
            // (then the solution is certainly unique).
            const int solved_score = static_cast<int>(shape.whites.size());
            const Layout start = layout(p);
            int score = Solver{start, 1, 0}.openness();
            int best = score;
            for (int round = 0, stale = 0; round < 80 && stale < 16 && score > solved_score;
                 ++round)
            {
                score = tighten(rng, layout(p), &p);
                if (score < 0)
                    break;
                stale = score < best ? 0 : stale + 1;
                best = std::min(best, score);
            }
            if (score == solved_score)
                return p;
            const Layout lay = layout(p);
            Solver solver{lay, 2, 3000};
            solver.run();
            if (!solver.aborted && solver.found == 1)
                return p;
        }
    }
}

std::vector<bool> conflicts(const Puzzle &puzzle, const Digits &digits)
{
    const int n = puzzle.side;
    std::vector<bool> bad(static_cast<std::size_t>(std::max(0, n * n)), false);
    if (n <= 0)
        return bad;
    for (const Run &run : runs(puzzle))
    {
        int sum = 0;
        bool full = true;
        for (int k = 0; k < run.length; ++k)
        {
            const int cell = at(run.cells, k);
            const int digit = at(digits, cell);
            sum += digit;
            full = full && digit != 0;
            for (int j = k + 1; j < run.length && digit != 0; ++j)
            {
                const int other = at(run.cells, j);
                if (at(digits, other) == digit)
                {
                    bad[static_cast<std::size_t>(cell)] = true;
                    bad[static_cast<std::size_t>(other)] = true;
                }
            }
        }
        if (full && sum != run.sum)
            for (int k = 0; k < run.length; ++k)
                bad[static_cast<std::size_t>(at(run.cells, k))] = true;
    }
    return bad;
}

int white_cells(const Puzzle &puzzle)
{
    int count = 0;
    for (int i = 0; i < puzzle.side * puzzle.side; ++i)
        count += puzzle.white(i) ? 1 : 0;
    return count;
}

int filled_cells(const Puzzle &puzzle, const Digits &digits)
{
    int count = 0;
    for (int i = 0; i < puzzle.side * puzzle.side; ++i)
        count += puzzle.white(i) && at(digits, i) != 0 ? 1 : 0;
    return count;
}

bool solved(const Puzzle &puzzle, const Digits &digits)
{
    if (puzzle.side <= 0 || filled_cells(puzzle, digits) != white_cells(puzzle))
        return false;
    const std::vector<bool> bad = conflicts(puzzle, digits);
    return std::none_of(bad.begin(), bad.end(), [](bool b) { return b; });
}

} // namespace ppz::kakuro
