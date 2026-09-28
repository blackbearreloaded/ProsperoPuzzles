// ProsperoPuzzles - Color Sort rules: pour coloured balls until every tube holds one colour.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/colorsort/core.hpp"

#include "games/kit/puzzle_scene.hpp"

#include <algorithm>
#include <unordered_set>

namespace ppz::colorsort
{

namespace
{

bool in_range(const Board &board, int tube)
{
    return tube >= 0 && tube < board.tubes;
}

std::size_t at(int index)
{
    return static_cast<std::size_t>(index);
}

// A state with the tube order forgotten: each tube packed into 16 bits
// (four nibbles of colour + 1, 0 for an empty slot), then sorted.
using Key = std::array<std::uint16_t, kMaxTubes>;

Key key_of(const Board &board)
{
    Key key{};
    for (int t = 0; t < board.tubes; ++t)
    {
        std::uint16_t code = 0;
        for (int i = 0; i < board.count[at(t)]; ++i)
            code = static_cast<std::uint16_t>(code | ((board.ball[at(t)][at(i)] + 1U) << (4 * i)));
        key[at(t)] = code;
    }
    std::sort(key.begin(), key.begin() + board.tubes);
    return key;
}

struct KeyHash
{
    std::size_t operator()(const Key &key) const
    {
        std::uint64_t h = 0x9e3779b97f4a7c15ULL;
        for (std::uint16_t code : key)
        {
            h ^= code;
            h *= 0xbf58476d1ce4e5b9ULL;
            h ^= h >> 29;
        }
        return static_cast<std::size_t>(h);
    }
};

constexpr int kMaxMoves = kMaxTubes * (kMaxTubes - 1);

struct Frame
{
    Board board;
    std::array<Move, kMaxMoves> moves{};
    int moves_count = 0;
    int next = 0;
    Move via{}; // the move that led here
};

// Useful moves, most promising first: onto a matching top that takes the whole
// run, then partial pours, then into one empty tube. Pointless moves (out of a
// complete tube, a single-colour tube into an empty one) are skipped.
void list_moves(Frame &frame)
{
    const Board &b = frame.board;
    std::array<Move, kMaxMoves> later{};
    int later_count = 0;
    std::array<Move, kMaxMoves> last{};
    int last_count = 0;
    frame.moves_count = 0;
    for (int from = 0; from < b.tubes; ++from)
    {
        const int count = b.count[at(from)];
        if (count == 0 || tube_complete(b, from))
            continue;
        const int run = top_run(b, from);
        const bool uniform = run == count;
        bool tried_empty = false;
        for (int to = 0; to < b.tubes; ++to)
        {
            const int amount = pour_amount(b, from, to);
            if (amount == 0)
                continue;
            const Move move{static_cast<std::uint8_t>(from), static_cast<std::uint8_t>(to)};
            if (b.count[at(to)] == 0)
            {
                if (uniform || tried_empty)
                    continue;
                tried_empty = true;
                last[at(last_count++)] = move;
            }
            else if (amount == run)
            {
                frame.moves[at(frame.moves_count++)] = move;
            }
            else
            {
                later[at(later_count++)] = move;
            }
        }
    }
    for (int i = 0; i < later_count; ++i)
        frame.moves[at(frame.moves_count++)] = later[at(i)];
    for (int i = 0; i < last_count; ++i)
        frame.moves[at(frame.moves_count++)] = last[at(i)];
}

} // namespace

int Board::top(int tube) const
{
    if (tube < 0 || tube >= tubes || count[at(tube)] == 0)
        return -1;
    return ball[at(tube)][at(count[at(tube)] - 1)];
}

int top_run(const Board &board, int tube)
{
    if (!in_range(board, tube))
        return 0;
    const int count = board.count[at(tube)];
    const int colour = board.top(tube);
    int run = 0;
    while (run < count && board.ball[at(tube)][at(count - 1 - run)] == colour)
        ++run;
    return run;
}

int pour_amount(const Board &board, int from, int to)
{
    if (from == to || !in_range(board, from) || !in_range(board, to))
        return 0;
    const int source = board.count[at(from)];
    const int target = board.count[at(to)];
    if (source == 0 || target >= kCapacity)
        return 0;
    if (target > 0 && board.top(to) != board.top(from))
        return 0;
    return std::min(top_run(board, from), kCapacity - target);
}

int pour(Board &board, int from, int to)
{
    const int amount = pour_amount(board, from, to);
    for (int i = 0; i < amount; ++i)
    {
        auto &source = board.count[at(from)];
        auto &target = board.count[at(to)];
        board.ball[at(to)][at(target)] = board.ball[at(from)][at(source - 1)];
        board.ball[at(from)][at(source - 1)] = 0;
        --source;
        ++target;
    }
    return amount;
}

bool tube_complete(const Board &board, int tube)
{
    return in_range(board, tube) && board.count[at(tube)] == kCapacity &&
           top_run(board, tube) == kCapacity;
}

bool solved(const Board &board)
{
    for (int t = 0; t < board.tubes; ++t)
        if (board.count[at(t)] != 0 && !tube_complete(board, t))
            return false;
    return board.tubes > 0;
}

int sorted_count(const Board &board)
{
    int sorted = 0;
    for (int t = 0; t < board.tubes; ++t)
        if (tube_complete(board, t))
            ++sorted;
    return sorted;
}

bool valid(const Board &board)
{
    if (board.colours < 2 || board.colours > kMaxColours ||
        board.tubes != board.colours + kEmptyTubes)
        return false;
    std::array<int, kMaxColours> seen{};
    for (int t = 0; t < board.tubes; ++t)
    {
        const int count = board.count[at(t)];
        if (count > kCapacity)
            return false;
        for (int i = 0; i < count; ++i)
        {
            const int colour = board.ball[at(t)][at(i)];
            if (colour >= board.colours)
                return false;
            ++seen[at(colour)];
        }
    }
    for (int c = 0; c < board.colours; ++c)
        if (seen[at(c)] != kCapacity)
            return false;
    return true;
}

Verdict solve(const Board &board, int budget, std::vector<Move> *path)
{
    if (path != nullptr)
        path->clear();
    if (solved(board))
        return Verdict::solvable;
    std::unordered_set<Key, KeyHash> visited;
    visited.reserve(static_cast<std::size_t>(std::max(16, budget)) + 1);
    visited.insert(key_of(board));
    std::vector<Frame> stack;
    stack.reserve(256);
    stack.emplace_back();
    stack.back().board = board;
    list_moves(stack.back());
    while (!stack.empty())
    {
        Frame &frame = stack.back();
        if (frame.next >= frame.moves_count)
        {
            stack.pop_back();
            continue;
        }
        const Move move = frame.moves[at(frame.next++)];
        Board next = frame.board;
        pour(next, move.from, move.to);
        if (!visited.insert(key_of(next)).second)
            continue;
        if (solved(next))
        {
            if (path != nullptr)
            {
                for (std::size_t i = 1; i < stack.size(); ++i)
                    path->push_back(stack[i].via);
                path->push_back(move);
            }
            return Verdict::solvable;
        }
        if (static_cast<int>(visited.size()) > budget)
            return Verdict::gave_up;
        stack.emplace_back();
        stack.back().board = next;
        stack.back().via = move;
        list_moves(stack.back());
    }
    return Verdict::unsolvable;
}

Board generate(std::uint64_t seed, int colours)
{
    colours = std::clamp(colours, 2, kMaxColours);
    kit::Rng rng(seed);
    std::vector<std::uint8_t> balls;
    for (int c = 0; c < colours; ++c)
        for (int i = 0; i < kCapacity; ++i)
            balls.push_back(static_cast<std::uint8_t>(c));
    Board board;
    for (;;)
    {
        rng.shuffle(balls);
        board = {};
        board.colours = colours;
        board.tubes = colours + kEmptyTubes;
        for (int t = 0; t < colours; ++t)
        {
            board.count[at(t)] = kCapacity;
            for (int i = 0; i < kCapacity; ++i)
                board.ball[at(t)][at(i)] = balls[at(t * kCapacity + i)];
        }
        bool starts_complete = false;
        for (int t = 0; t < colours; ++t)
            starts_complete = starts_complete || tube_complete(board, t);
        if (!starts_complete && solve(board, kSolveBudget) == Verdict::solvable)
            return board;
    }
}

} // namespace ppz::colorsort
