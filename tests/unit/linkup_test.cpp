// ProsperoPuzzles - Link Up rules, generator and play screen tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/linkup/core.hpp"
#include "games/linkup/linkup_scene.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>

using namespace ppz;
using namespace ppz::linkup;

namespace
{

Paths solution_paths(const Puzzle &p)
{
    Paths paths{};
    for (int pair = 0; pair < p.pairs; ++pair)
        paths[static_cast<std::size_t>(pair)] = p.solution(pair);
    return paths;
}

InputFrame press(Action action)
{
    InputFrame f;
    f.pressed = action_bit(action);
    return f;
}

InputFrame nav(Direction d)
{
    InputFrame f;
    f.nav = d;
    return f;
}

Direction towards(int side, int from, int to)
{
    if (to == from - side)
        return Direction::up;
    if (to == from + side)
        return Direction::down;
    if (to == from - 1)
        return Direction::left;
    return Direction::right;
}

// Walks the cursor (not drawing) to cell.
void walk_to(LinkUpScene &scene, int cell, std::vector<audio::Cue> &cues)
{
    const int side = scene.puzzle().side;
    const int r = cell / side;
    const int c = cell % side;
    while (scene.cursor_row() != r || scene.cursor_col() != c)
    {
        const Direction d = scene.cursor_row() < r   ? Direction::down
                            : scene.cursor_row() > r ? Direction::up
                            : scene.cursor_col() < c ? Direction::right
                                                     : Direction::left;
        scene.update(nav(d), 0.016f, cues);
    }
}

} // namespace

TEST(LinkUp, GeneratedPuzzlesCoverTheBoardWithGoodPairs)
{
    const int min_pairs[] = {5, 7, 9};
    const int max_pairs[] = {5, 8, 11};
    const int sides[] = {5, 7, 9};
    for (int size = 0; size < 3; ++size)
    {
        const int side = sides[size];
        for (std::uint64_t seed = 1; seed <= 8; ++seed)
        {
            const Puzzle p = generate(seed * 7919 + static_cast<std::uint64_t>(side), side);
            ASSERT_EQ(p.side, side);
            EXPECT_TRUE(valid_puzzle(p)) << "side " << side << " seed " << seed;
            EXPECT_GE(p.pairs, min_pairs[size]);
            EXPECT_LE(p.pairs, max_pairs[size]);
            int straights = 0;
            for (int pair = 0; pair < p.pairs; ++pair)
            {
                const auto path = p.solution(pair);
                EXPECT_GE(path.size(), 3u);
                EXPECT_FALSE(adjacent(side, p.dot(pair, 0), p.dot(pair, 1)));
                bool row = true;
                bool col = true;
                for (std::uint8_t cell : path)
                {
                    row = row && cell / side == path[0] / side;
                    col = col && cell % side == path[0] % side;
                }
                straights += (row || col) ? 1 : 0;
            }
            EXPECT_LE(straights * 2, p.pairs);
            const Paths paths = solution_paths(p);
            EXPECT_TRUE(valid_paths(p, paths));
            EXPECT_TRUE(solved(p, paths));
            EXPECT_EQ(connected_pairs(p, paths), p.pairs);
        }
    }
    // Deterministic for a seed.
    const Puzzle a = generate(1234, 9);
    const Puzzle b = generate(1234, 9);
    EXPECT_EQ(a.route, b.route);
    EXPECT_EQ(a.length, b.length);
}

TEST(LinkUp, LargeGenerationIsFast)
{
    double worst = 0.0;
    for (std::uint64_t seed = 1; seed <= 20; ++seed)
    {
        const auto start = std::chrono::steady_clock::now();
        const Puzzle p = generate(seed * 104729, 9);
        const double ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                .count();
        worst = std::max(worst, ms);
        EXPECT_TRUE(valid_puzzle(p));
    }
    std::printf("linkup: worst 9x9 generation %.2f ms (sanitized build)\n", worst);
    EXPECT_LT(worst, 1000.0);
}

TEST(LinkUp, DrawingRulesExtendRetractCutAndConnect)
{
    const Puzzle p = generate(42, 5);
    Paths paths{};
    // Draw pair 0 up to one cell short of its other dot.
    const auto route = p.solution(0);
    EXPECT_EQ(pick_up(p, paths, route[0]), 0);
    ASSERT_EQ(paths[0].size(), 1u);
    for (std::size_t k = 1; k + 1 < route.size(); ++k)
        EXPECT_EQ(extend(p, paths, 0, route[k]), Step::extended);
    EXPECT_FALSE(connected(p, paths, 0));
    EXPECT_FALSE(solved(p, paths));
    // Step back retracts.
    Paths copy = paths;
    if (route.size() >= 3)
    {
        EXPECT_EQ(extend(p, copy, 0, route[route.size() - 3]), Step::retracted);
        EXPECT_EQ(copy[0].size(), route.size() - 2);
    }
    // Completing reaches the other dot; nothing extends a connected path.
    EXPECT_EQ(extend(p, paths, 0, route.back()), Step::connected);
    EXPECT_TRUE(connected(p, paths, 0));
    EXPECT_EQ(connected_pairs(p, paths), 1);
    EXPECT_EQ(extend(p, paths, 0, route[route.size() - 2]), Step::rejected);

    // Pair 1 draws through pair 0: pair 0 loses its tail from that cell on.
    // Find a cell of pair 0 (not a dot) next to a free cell or pair 1's dot.
    Paths cross{};
    cross[0] = route;
    bool tested = false;
    for (std::size_t k = 1; k + 1 < route.size() && !tested; ++k)
    {
        for (int pair = 1; pair < p.pairs && !tested; ++pair)
        {
            for (int which = 0; which < 2 && !tested; ++which)
            {
                const int dot = p.dot(pair, which);
                if (!adjacent(p.side, dot, route[k]))
                    continue;
                Paths t = cross;
                EXPECT_EQ(pick_up(p, t, dot), pair);
                EXPECT_EQ(extend(p, t, pair, route[k]), Step::cut);
                EXPECT_EQ(t[0].size(), k);
                EXPECT_EQ(owner(t, route[k]), pair);
                EXPECT_TRUE(valid_paths(p, t));
                tested = true;
            }
        }
    }
    EXPECT_TRUE(tested);

    // Another pair's dot is rejected; picking up a path cell cuts it back.
    Paths full = solution_paths(p);
    const int other_dot = p.dot(1, 0);
    Paths empty{};
    for (int pair = 0; pair < p.pairs; ++pair)
    {
        if (pair == 1)
            continue;
        for (int which = 0; which < 2; ++which)
        {
            if (!adjacent(p.side, p.dot(pair, which), other_dot))
                continue;
            Paths t{};
            pick_up(p, t, p.dot(pair, which));
            EXPECT_EQ(extend(p, t, pair, other_dot), Step::rejected);
        }
    }
    const auto route1 = p.solution(1);
    EXPECT_EQ(pick_up(p, full, route1[1]), 1);
    EXPECT_EQ(full[1].size(), 2u);
    EXPECT_FALSE(solved(p, full));
    EXPECT_EQ(pick_up(p, empty, route1[1]), kNone); // an empty cell picks nothing
}

TEST(LinkUp, ValidPathsRejectsBrokenStates)
{
    const Puzzle p = generate(7, 5);
    Paths paths = solution_paths(p);
    EXPECT_TRUE(valid_paths(p, paths));
    Paths shared = paths;
    shared[1].push_back(shared[0][1]); // a cell on two paths
    EXPECT_FALSE(valid_paths(p, shared));
    Paths gap = paths;
    gap[0].erase(gap[0].begin() + 1); // a jump
    EXPECT_FALSE(valid_paths(p, gap));
    Paths loose{};
    loose[0] = {paths[0][1]}; // not starting on a dot
    EXPECT_FALSE(valid_paths(p, loose));
}

TEST(LinkUpScene, DrawsUndoesSavesAndSolves)
{
    ui::Fonts fonts;
    LinkUpScene scene(fonts);
    scene.start({}, {});
    scene.new_puzzle(99, 0);
    std::vector<audio::Cue> cues;
    const Puzzle p = scene.puzzle();
    const int side = p.side;

    // Cross on an empty-of-path non-dot cell is rejected.
    const auto route0 = p.solution(0);
    walk_to(scene, route0[1], cues);
    cues.clear();
    scene.update(press(Action::confirm), 0.016f, cues);
    EXPECT_EQ(scene.drawing(), kNone);
    EXPECT_NE(std::find(cues.begin(), cues.end(), audio::Cue::invalid), cues.end());

    // Pick up pair 0 and draw two steps: the cursor follows the head.
    walk_to(scene, route0[0], cues);
    scene.update(press(Action::confirm), 0.016f, cues);
    EXPECT_EQ(scene.drawing(), 0);
    scene.update(nav(towards(side, route0[0], route0[1])), 0.016f, cues);
    scene.update(nav(towards(side, route0[1], route0[2])), 0.016f, cues);
    EXPECT_EQ(scene.cursor_row() * side + scene.cursor_col(), route0[2]);
    // Circle lets go (not the pause menu), then the gesture is one undo step.
    scene.update(press(Action::back), 0.016f, cues);
    EXPECT_FALSE(scene.menu_open());
    EXPECT_EQ(scene.drawing(), kNone);
    EXPECT_EQ(scene.paths()[0].size(), 3u);
    EXPECT_TRUE(scene.in_progress());
    scene.update(press(Action::page_prev), 0.016f, cues);
    EXPECT_TRUE(scene.paths()[0].empty());
    scene.update(press(Action::page_next), 0.016f, cues);
    EXPECT_EQ(scene.paths()[0].size(), 3u);

    // The save restores the same board and paths.
    const std::string save = scene.save();
    LinkUpScene restored(fonts);
    restored.start(save, scene.stats());
    EXPECT_EQ(restored.puzzle().route, p.route);
    EXPECT_EQ(restored.puzzle().length, p.length);
    EXPECT_EQ(restored.paths(), scene.paths());
    // A corrupt save falls back to a fresh puzzle.
    LinkUpScene broken(fonts);
    broken.start(save.substr(0, save.size() - 3), {});
    EXPECT_TRUE(valid_puzzle(broken.puzzle()));

    // Square clears the colour under the cursor.
    scene.update(press(Action::west), 0.016f, cues);
    EXPECT_TRUE(scene.paths()[0].empty());

    // Draw every pair along the generator's route.
    for (int pair = 0; pair < p.pairs; ++pair)
    {
        const auto route = p.solution(pair);
        walk_to(scene, route[0], cues);
        scene.update(press(Action::confirm), 0.016f, cues);
        ASSERT_EQ(scene.drawing(), pair);
        cues.clear();
        for (std::size_t k = 1; k < route.size(); ++k)
            scene.update(nav(towards(side, route[k - 1], route[k])), 0.016f, cues);
        EXPECT_EQ(scene.drawing(), kNone) << "pair " << pair;
        EXPECT_NE(std::find(cues.begin(), cues.end(), audio::Cue::connect), cues.end());
    }
    EXPECT_TRUE(scene.is_solved());
    EXPECT_NE(std::find(cues.begin(), cues.end(), audio::Cue::complete), cues.end());
    EXPECT_FALSE(scene.in_progress());
    for (int frame = 0; frame < 120; ++frame)
        scene.update(InputFrame{}, 1.0f / 60.0f, cues);
    EXPECT_TRUE(scene.menu_open()); // the solved menu follows the flourish
}

TEST(LinkUpScene, CircleOpensPauseWhenNotDrawingAndSizesStartNewPuzzles)
{
    ui::Fonts fonts;
    LinkUpScene scene(fonts);
    scene.start({}, {});
    std::vector<audio::Cue> cues;
    scene.update(press(Action::back), 0.016f, cues);
    EXPECT_TRUE(scene.menu_open());
    scene.update(press(Action::confirm), 0.016f, cues); // Resume
    EXPECT_FALSE(scene.menu_open());
    scene.new_puzzle(5, 2);
    EXPECT_EQ(scene.puzzle().side, 9);
    EXPECT_EQ(scene.size(), 2);
    scene.new_puzzle(5, 1);
    EXPECT_EQ(scene.puzzle().side, 7);
}
