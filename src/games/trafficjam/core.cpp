// ProsperoPuzzles - Traffic Jam rules, solver and generator.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/trafficjam/core.hpp"

#include "games/kit/puzzle_scene.hpp"

#include <algorithm>

namespace ppz::trafficjam
{

namespace
{

using Key = std::uint64_t; // 3 bits of position per vehicle

// States explored per search before a board is given up on.
constexpr std::size_t kStateLimit = 1u << 11;
// States the solver explores before it gives up.
constexpr std::size_t kSolveLimit = 1u << 18;

Key encode(const Positions &pos, int count)
{
    Key key = 0;
    for (int i = 0; i < count; ++i)
        key |= static_cast<Key>(pos[static_cast<std::size_t>(i)] & 7u) << (3 * i);
    return key;
}

int position_of(Key key, int v)
{
    return static_cast<int>((key >> (3 * v)) & 7u);
}

Key with_position(Key key, int v, int to)
{
    const Key mask = static_cast<Key>(7) << (3 * v);
    return (key & ~mask) | (static_cast<Key>(to) << (3 * v));
}

// Cells covered by every vehicle, as a bit mask.
std::uint64_t occupied_mask(const Puzzle &p, Key key)
{
    std::uint64_t mask = 0;
    for (int v = 0; v < p.count; ++v)
    {
        const Vehicle &vehicle = p.vehicles[static_cast<std::size_t>(v)];
        const int at = position_of(key, v);
        for (int i = 0; i < vehicle.length; ++i)
            mask |= std::uint64_t{1} << vehicle_cell(vehicle, at, i);
    }
    return mask;
}

// A puzzle's vehicles as bit masks, for fast move generation.
struct Lanes
{
    int count = 0;
    std::array<int, kMaxVehicles> length{};
    // The one cell at step s of vehicle v's lane, and its body at position s.
    std::array<std::array<std::uint64_t, kSide>, kMaxVehicles> cell{};
    std::array<std::array<std::uint64_t, kSide>, kMaxVehicles> body{};

    explicit Lanes(const Puzzle &p) : count(p.count)
    {
        for (int v = 0; v < p.count; ++v)
        {
            const Vehicle &vehicle = p.vehicles[static_cast<std::size_t>(v)];
            const auto vi = static_cast<std::size_t>(v);
            length[vi] = vehicle.length;
            for (int s = 0; s < kSide; ++s)
                cell[vi][static_cast<std::size_t>(s)] = std::uint64_t{1}
                                                        << vehicle_cell(vehicle, s, 0);
            for (int s = 0; s + vehicle.length <= kSide; ++s)
                for (int i = 0; i < vehicle.length; ++i)
                    body[vi][static_cast<std::size_t>(s)] |=
                        cell[vi][static_cast<std::size_t>(s + i)];
        }
    }
};

// Calls visit(next_key, vehicle, to) for every single move from key.
template <typename Visit> void for_each_move(const Lanes &lanes, Key key, Visit visit)
{
    std::uint64_t mask = 0;
    for (int v = 0; v < lanes.count; ++v)
        mask |= lanes.body[static_cast<std::size_t>(v)]
                          [static_cast<std::size_t>(position_of(key, v) % kSide)];
    for (int v = 0; v < lanes.count; ++v)
    {
        const auto vi = static_cast<std::size_t>(v);
        const int length = lanes.length[vi];
        const int at = position_of(key, v);
        for (int to = at - 1; to >= 0; --to)
        {
            if ((mask & lanes.cell[vi][static_cast<std::size_t>(to)]) != 0)
                break;
            visit(with_position(key, v, to), v, to);
        }
        for (int to = at + 1; to + length <= kSide; ++to)
        {
            if ((mask & lanes.cell[vi][static_cast<std::size_t>(to + length - 1)]) != 0)
                break;
            visit(with_position(key, v, to), v, to);
        }
    }
}

bool key_solved(const Puzzle &p, Key key)
{
    return position_of(key, 0) == kSide - p.vehicles[0].length;
}

// Open-addressing map from a state to its index in a list of states.
class StateIndex
{
  public:
    StateIndex()
    {
        slots_.assign(1u << 12, 0);
        values_.assign(slots_.size(), 0);
    }
    // The index of key, or -1.
    int find(Key key) const
    {
        const Key stored = key + 1;
        for (std::size_t i = slot(stored);; i = (i + 1) & (slots_.size() - 1))
        {
            if (slots_[i] == 0)
                return -1;
            if (slots_[i] == stored)
                return values_[i];
        }
    }
    void insert(Key key, int value)
    {
        if ((size_ + 1) * 2 > slots_.size())
            grow();
        place(key + 1, value);
        ++size_;
    }
    // The index of key if present; otherwise stores value for it and returns -1.
    int find_or_insert(Key key, int value)
    {
        if ((size_ + 1) * 2 > slots_.size())
            grow();
        const Key stored = key + 1;
        std::size_t i = slot(stored);
        for (; slots_[i] != 0; i = (i + 1) & (slots_.size() - 1))
            if (slots_[i] == stored)
                return values_[i];
        slots_[i] = stored;
        values_[i] = value;
        ++size_;
        return -1;
    }
    void clear()
    {
        std::fill(slots_.begin(), slots_.end(), 0);
        size_ = 0;
    }

  private:
    std::size_t slot(Key stored) const
    {
        return static_cast<std::size_t>((stored * 0x9e3779b97f4a7c15ULL) >> 40) &
               (slots_.size() - 1);
    }
    void place(Key stored, int value)
    {
        std::size_t i = slot(stored);
        while (slots_[i] != 0)
            i = (i + 1) & (slots_.size() - 1);
        slots_[i] = stored;
        values_[i] = value;
    }
    void grow()
    {
        std::vector<Key> old_slots(slots_.size() * 2, 0);
        std::vector<int> old_values(slots_.size() * 2, 0);
        old_slots.swap(slots_);
        old_values.swap(values_);
        for (std::size_t i = 0; i < old_slots.size(); ++i)
            if (old_slots[i] != 0)
                place(old_slots[i], old_values[i]);
    }

    std::vector<Key> slots_;
    std::vector<int> values_;
    std::size_t size_ = 0;
};

// Adds a car or truck at a random free spot of the start position (no other
// horizontal vehicle may share the exit row). False when nothing fitted.
bool add_random_vehicle(kit::Rng &rng, Puzzle &p)
{
    if (p.count >= kMaxVehicles)
        return false;
    const std::uint64_t mask = occupied_mask(p, encode(p.start, p.count));
    for (int tries = 0; tries < 40; ++tries)
    {
        Vehicle v;
        v.length = static_cast<std::uint8_t>(rng.below(3) == 0 ? 3 : 2);
        v.vertical = rng.below(10) < 6;
        v.lane = static_cast<std::uint8_t>(rng.below(kSide));
        if (!v.vertical && v.lane == kExitRow)
            continue;
        const int at = rng.below(kSide - v.length + 1);
        std::uint64_t cells = 0;
        for (int i = 0; i < v.length; ++i)
            cells |= std::uint64_t{1} << vehicle_cell(v, at, i);
        if ((cells & mask) != 0)
            continue;
        p.vehicles[static_cast<std::size_t>(p.count)] = v;
        p.start[static_cast<std::size_t>(p.count)] = static_cast<std::uint8_t>(at);
        ++p.count;
        return true;
    }
    return false;
}

void remove_vehicle(Puzzle &p, int v)
{
    for (int i = v; i + 1 < p.count; ++i)
    {
        p.vehicles[static_cast<std::size_t>(i)] = p.vehicles[static_cast<std::size_t>(i + 1)];
        p.start[static_cast<std::size_t>(i)] = p.start[static_cast<std::size_t>(i + 1)];
    }
    --p.count;
}

// The red car on the exit row, then the given number of cars and trucks.
Puzzle random_board(kit::Rng &rng, int others)
{
    Puzzle p;
    p.count = 1;
    p.vehicles[0] = {2, false, kExitRow};
    p.start[0] = static_cast<std::uint8_t>(rng.below(kSide - 2));
    for (int i = 0; i < others; ++i)
        add_random_vehicle(rng, p);
    return p;
}

// The states reachable from the board's start, each with its distance to the
// nearest solved state (-1 if the red car can never leave). False if too big.
struct Workspace
{
    StateIndex index;
    std::vector<Key> states;
    std::vector<int> distance;
    std::vector<int> edges; // neighbours of state i: edges[first[i] .. first[i + 1])
    std::vector<int> first;
    std::vector<int> queue;
};

bool distances(const Puzzle &p, Workspace &w)
{
    w.index.clear();
    w.states.clear();
    w.edges.clear();
    w.first.clear();
    const Key start = encode(p.start, p.count);
    w.states.push_back(start);
    w.index.insert(start, 0);
    const Lanes lanes(p);
    for (std::size_t head = 0; head < w.states.size(); ++head)
    {
        if (w.states.size() >= kStateLimit)
            return false;
        w.first.push_back(static_cast<int>(w.edges.size()));
        for_each_move(lanes, w.states[head],
                      [&](Key next, int, int)
                      {
                          const int count = static_cast<int>(w.states.size());
                          const int found = w.index.find_or_insert(next, count);
                          if (found < 0)
                              w.states.push_back(next);
                          w.edges.push_back(found < 0 ? count : found);
                      });
    }
    w.first.push_back(static_cast<int>(w.edges.size()));
    // Breadth-first from every solved state at once (moves are reversible).
    w.distance.assign(w.states.size(), -1);
    w.queue.clear();
    for (std::size_t i = 0; i < w.states.size(); ++i)
    {
        if (key_solved(p, w.states[i]))
        {
            w.distance[i] = 0;
            w.queue.push_back(static_cast<int>(i));
        }
    }
    for (std::size_t head = 0; head < w.queue.size(); ++head)
    {
        const auto from = static_cast<std::size_t>(w.queue[head]);
        const int next_distance = w.distance[from] + 1;
        const auto end = static_cast<std::size_t>(w.first[from + 1]);
        for (auto e = static_cast<std::size_t>(w.first[from]); e < end; ++e)
        {
            const auto to = static_cast<std::size_t>(w.edges[e]);
            if (w.distance[to] >= 0)
                continue;
            w.distance[to] = next_distance;
            w.queue.push_back(static_cast<int>(to));
        }
    }
    return true;
}

} // namespace

std::array<std::int8_t, kCells> occupancy(const Puzzle &puzzle, const Positions &pos)
{
    std::array<std::int8_t, kCells> cells{};
    cells.fill(-1);
    for (int v = 0; v < puzzle.count; ++v)
    {
        const Vehicle &vehicle = puzzle.vehicles[static_cast<std::size_t>(v)];
        for (int i = 0; i < vehicle.length; ++i)
        {
            const int cell = vehicle_cell(vehicle, pos[static_cast<std::size_t>(v)], i);
            if (cell >= 0 && cell < kCells)
                cells[static_cast<std::size_t>(cell)] = static_cast<std::int8_t>(v);
        }
    }
    return cells;
}

bool valid(const Puzzle &puzzle, const Positions &pos)
{
    if (puzzle.count < 1 || puzzle.count > kMaxVehicles)
        return false;
    const Vehicle &red = puzzle.vehicles[0];
    if (red.vertical || red.lane != kExitRow || red.length != 2)
        return false;
    std::array<bool, kCells> used{};
    for (int v = 0; v < puzzle.count; ++v)
    {
        const Vehicle &vehicle = puzzle.vehicles[static_cast<std::size_t>(v)];
        const int at = pos[static_cast<std::size_t>(v)];
        if (vehicle.length < 2 || vehicle.length > 3 || vehicle.lane >= kSide ||
            at + vehicle.length > kSide)
            return false;
        for (int i = 0; i < vehicle.length; ++i)
        {
            const auto cell = static_cast<std::size_t>(vehicle_cell(vehicle, at, i));
            if (used[cell])
                return false;
            used[cell] = true;
        }
    }
    return true;
}

int slide_room(const Puzzle &puzzle, const Positions &pos, int v, int dir)
{
    if (v < 0 || v >= puzzle.count || (dir != -1 && dir != 1))
        return 0;
    const std::array<std::int8_t, kCells> cells = occupancy(puzzle, pos);
    const Vehicle &vehicle = puzzle.vehicles[static_cast<std::size_t>(v)];
    const int at = pos[static_cast<std::size_t>(v)];
    int room = 0;
    for (int to = at + dir; to >= 0 && to + vehicle.length <= kSide; to += dir)
    {
        const int cell = vehicle_cell(vehicle, to, dir < 0 ? 0 : vehicle.length - 1);
        if (cells[static_cast<std::size_t>(cell)] >= 0)
            break;
        ++room;
    }
    return room;
}

bool solved(const Puzzle &puzzle, const Positions &pos)
{
    return puzzle.count > 0 && pos[0] == kSide - puzzle.vehicles[0].length;
}

int solve(const Puzzle &puzzle, const Positions &pos, std::vector<Move> *path)
{
    if (path != nullptr)
        path->clear();
    if (!valid(puzzle, pos))
        return -1;
    StateIndex index;
    std::vector<Key> states{encode(pos, puzzle.count)};
    std::vector<int> parent{-1};
    std::vector<Move> via{Move{}};
    index.insert(states[0], 0);
    const Lanes lanes(puzzle);
    for (std::size_t head = 0; head < states.size() && states.size() < kSolveLimit; ++head)
    {
        if (key_solved(puzzle, states[head]))
        {
            int depth = 0;
            std::vector<Move> moves;
            for (int at = static_cast<int>(head); parent[static_cast<std::size_t>(at)] >= 0;
                 at = parent[static_cast<std::size_t>(at)])
            {
                moves.push_back(via[static_cast<std::size_t>(at)]);
                ++depth;
            }
            if (path != nullptr)
                path->assign(moves.rbegin(), moves.rend());
            return depth;
        }
        for_each_move(lanes, states[head],
                      [&](Key next, int v, int to)
                      {
                          if (index.find(next) >= 0)
                              return;
                          index.insert(next, static_cast<int>(states.size()));
                          states.push_back(next);
                          parent.push_back(static_cast<int>(head));
                          via.push_back({v, to});
                      });
    }
    return -1;
}

namespace
{

// Moves the board's start to a random reachable state that needs `target`
// moves, where target is the deepest at most `most` (or, when least >= 0 and
// reachable, a random length in least..deepest). False when nothing fits.
bool settle(Puzzle &p, kit::Rng &rng, int least, int most, Workspace &w)
{
    if (!distances(p, w))
        return false;
    const std::vector<int> &distance = w.distance;
    int deepest = -1;
    for (int d : distance)
        if (d <= most)
            deepest = std::max(deepest, d);
    if (deepest <= 0)
        return false;
    int target = deepest;
    if (least > 0 && deepest >= least)
        target = least + rng.below(deepest - least + 1);
    int matches = 0;
    for (int d : distance)
        matches += d == target ? 1 : 0;
    int pick = rng.below(matches);
    for (std::size_t i = 0; i < w.states.size(); ++i)
    {
        if (distance[i] != target || pick-- > 0)
            continue;
        for (int v = 0; v < p.count; ++v)
            p.start[static_cast<std::size_t>(v)] =
                static_cast<std::uint8_t>(position_of(w.states[i], v));
        break;
    }
    p.par = target;
    return true;
}

} // namespace

Puzzle generate(std::uint64_t seed, int level)
{
    level = std::clamp(level, 0, 2);
    constexpr int kFewest[3] = {7, 9, 10};
    constexpr int kMost[3] = {10, 12, 13};
    constexpr int kMostPar[3] = {8, 15, 60};
    constexpr int kSteps = 1000;  // boards evaluated at most
    constexpr int kPatience = 40; // fruitless mutations before starting over
    constexpr int kSlip = 10;     // one in kSlip mutations may cost a move (escapes plateaus)
    kit::Rng rng(seed * 0x2545f4914f6cdd1dULL + static_cast<std::uint64_t>(level) + 1);
    Workspace workspace;
    // Hill-climb: mutate the board (drop, add or swap a vehicle), keep changes
    // that do not make its hardest state easier, restart when stuck.
    Puzzle best;
    Puzzle current;
    bool have = false;
    int stale = 0;
    for (int step = 0; step < kSteps && best.par < kMinPar[level]; ++step)
    {
        Puzzle candidate;
        if (!have || stale > kPatience)
        {
            have = false;
            stale = 0;
            candidate =
                random_board(rng, kFewest[level] + rng.below(kMost[level] - kFewest[level] + 1));
        }
        else
        {
            candidate = current;
            int kind = rng.below(3); // 0 drop, 1 add, 2 swap
            if ((kind == 0 && candidate.count <= 1 + kFewest[level]) ||
                (kind == 1 && candidate.count >= 1 + kMost[level]))
                kind = 2;
            if (kind != 1)
                remove_vehicle(candidate, 1 + rng.below(candidate.count - 1));
            if (kind != 0 && !add_random_vehicle(rng, candidate))
            {
                ++stale;
                continue;
            }
        }
        if (!settle(candidate, rng, -1, kMostPar[level], workspace))
        {
            ++stale;
            continue;
        }
        if (!have || candidate.par >= current.par - (rng.below(kSlip) == 0 ? 1 : 0))
        {
            stale = have && candidate.par == current.par ? stale + 1 : 0;
            current = candidate;
            have = true;
        }
        else
        {
            ++stale;
        }
        if (current.par > best.par)
            best = current;
    }
    // Easy and Medium vary their length within the level's range.
    if (level < 2 && best.par >= kMinPar[level])
        settle(best, rng, kMinPar[level], kMostPar[level], workspace);
    return best;
}

} // namespace ppz::trafficjam
