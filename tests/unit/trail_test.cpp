// ProsperoPuzzles - Trail rules, generator and play screen tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/trail/core.hpp"
#include "games/trail/trail_scene.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>

using namespace ppz;
using namespace ppz::trail;

namespace
{

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

// Direction from one cell to an orthogonal neighbour.
Direction toward(int side, int from, int to)
{
    (void)side;
    if (to == from + 1)
        return Direction::right;
    if (to == from - 1)
        return Direction::left;
    return to > from ? Direction::down : Direction::up;
}

std::vector<std::uint8_t> solution_of(const Puzzle &p)
{
    return {p.solution.begin(), p.solution.begin() + p.side * p.side};
}

// A 4x4 serpentine with 1 at the top left, 2 at cell 5 and 3 at cell 12.
Puzzle serpentine()
{
    Puzzle p;
    p.side = 4;
    const std::uint8_t order[] = {0, 1, 2, 3, 7, 6, 5, 4, 8, 9, 10, 11, 15, 14, 13, 12};
    for (int i = 0; i < 16; ++i)
        p.solution[static_cast<std::size_t>(i)] = order[i];
    p.number[0] = 1;
    p.number[5] = 2;
    p.number[12] = 3;
    p.count = 3;
    return p;
}

} // namespace

TEST(Trail, GeneratedPuzzlesHaveExactlyOneSolution)
{
    double slowest = 0.0;
    for (int side : {5, 6, 7})
    {
        for (std::uint64_t seed = 1; seed <= 8; ++seed)
        {
            const auto t0 = std::chrono::steady_clock::now();
            const Puzzle p = generate(seed * 7919 + static_cast<std::uint64_t>(side), side);
            const double ms =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                    .count();
            slowest = std::max(slowest, ms);
            ASSERT_EQ(p.side, side);
            EXPECT_GE(p.count, 2);
            const std::vector<std::uint8_t> solution = solution_of(p);
            EXPECT_TRUE(solved(p, solution)) << "side " << side << " seed " << seed;
            EXPECT_EQ(cell_of(p, 1), solution.front());
            EXPECT_EQ(cell_of(p, p.count), solution.back());
            std::vector<std::uint8_t> found;
            EXPECT_EQ(count_solutions(p, 5, &found), 1) << "side " << side << " seed " << seed;
            EXPECT_EQ(found, solution);
            std::printf("[trail] %dx%d seed %d: %d numbers, %.1f ms\n", side, side,
                        static_cast<int>(seed), p.count, ms);
        }
    }
    std::printf("[trail] slowest generation %.1f ms\n", slowest);
}

TEST(Trail, SameSeedSamePuzzle)
{
    const Puzzle a = generate(1234, 7);
    const Puzzle b = generate(1234, 7);
    EXPECT_EQ(a.number, b.number);
    EXPECT_EQ(a.solution, b.solution);
}

TEST(Trail, StepsFollowTheRules)
{
    const Puzzle p = serpentine();
    std::vector<std::uint8_t> path = {0};
    EXPECT_EQ(step(p, &path, -1, 0), Step::off_board);
    EXPECT_EQ(step(p, &path, 0, 1), Step::extended);     // 4
    EXPECT_EQ(step(p, &path, 0, 1), Step::extended);     // 8
    EXPECT_EQ(step(p, &path, 0, 1), Step::out_of_order); // 12 is 3, but 2 is next
    EXPECT_EQ(path.size(), 3u);
    EXPECT_EQ(step(p, &path, 0, -1), Step::retracted); // back onto 4
    EXPECT_EQ(path, (std::vector<std::uint8_t>{0, 4}));
    EXPECT_EQ(step(p, &path, 1, 0), Step::extended); // 5 holds 2
    EXPECT_EQ(next_number(p, path), 3);
    EXPECT_EQ(step(p, &path, 0, -1), Step::extended); // 1
    EXPECT_EQ(step(p, &path, -1, 0), Step::crossed);  // 0 is on the path
    EXPECT_TRUE(valid_path(p, path));
    EXPECT_FALSE(solved(p, path));

    // Reaching the highest number early ends the path.
    std::vector<std::uint8_t> early = {0, 1, 5, 4, 8, 12};
    EXPECT_TRUE(valid_path(p, early));
    EXPECT_EQ(step(p, &early, 1, 0), Step::finished);
    EXPECT_FALSE(solved(p, early));

    EXPECT_TRUE(solved(p, solution_of(p)));
    EXPECT_FALSE(valid_path(p, {1, 0}));    // must start on 1
    EXPECT_FALSE(valid_path(p, {0, 5}));    // diagonal
    EXPECT_FALSE(valid_path(p, {0, 1, 0})); // repeat
    EXPECT_GE(count_solutions(p, 5), 1);    // three numbers leave this small board ambiguous
}

TEST(TrailScene, DrawsCutsUndoesSavesAndSolves)
{
    ui::Fonts fonts;
    TrailScene scene(fonts);
    scene.start({}, {});
    scene.new_puzzle(99, 0);
    std::vector<audio::Cue> cues;
    const Puzzle p = scene.puzzle();
    const std::vector<std::uint8_t> solution = solution_of(p);
    const int n = p.side;
    ASSERT_EQ(scene.path().size(), 1u);

    // The ring follows the path's head.
    scene.update(InputFrame{}, 0.016f, cues);
    EXPECT_EQ(scene.cursor_row() * n + scene.cursor_col(), solution[0]);

    // Draw three cells of the solution; undo and redo the last.
    for (int i = 1; i <= 3; ++i)
        scene.update(nav(toward(n, solution[static_cast<std::size_t>(i - 1)],
                                solution[static_cast<std::size_t>(i)])),
                     0.016f, cues);
    ASSERT_EQ(scene.path().size(), 4u);
    EXPECT_EQ(scene.cursor_row() * n + scene.cursor_col(), solution[3]);
    scene.update(press(Action::page_prev), 0.016f, cues);
    EXPECT_EQ(scene.path().size(), 3u);
    scene.update(press(Action::page_next), 0.016f, cues);
    EXPECT_EQ(scene.path().size(), 4u);
    EXPECT_TRUE(scene.in_progress());

    // Stepping back onto the previous cell erases the head; Square does too.
    scene.update(nav(toward(n, solution[3], solution[2])), 0.016f, cues);
    EXPECT_EQ(scene.path().size(), 3u);
    scene.update(press(Action::west), 0.016f, cues);
    EXPECT_EQ(scene.path().size(), 2u);
    scene.update(nav(toward(n, solution[1], solution[2])), 0.016f, cues);
    scene.update(nav(toward(n, solution[2], solution[3])), 0.016f, cues);
    ASSERT_EQ(scene.path().size(), 4u);

    // Stepping onto a path cell that is not the previous one is rejected.
    cues.clear();
    const std::size_t before = scene.path().size();
    for (Direction d : {Direction::up, Direction::down, Direction::left, Direction::right})
    {
        std::vector<std::uint8_t> probe = scene.path();
        const int head = probe.back();
        const int col = head % n + (d == Direction::right) - (d == Direction::left);
        const int row = head / n + (d == Direction::down) - (d == Direction::up);
        if (col < 0 || row < 0 || col >= n || row >= n)
            continue;
        const int cell = row * n + col;
        if (cell == probe[0] || cell == probe[1])
        {
            scene.update(nav(d), 0.016f, cues);
            EXPECT_EQ(scene.path().size(), before);
            EXPECT_NE(std::find(cues.begin(), cues.end(), audio::Cue::invalid), cues.end());
        }
    }

    // Cross: cut mode. Move the ring back to the path's second cell and cut there.
    scene.update(press(Action::confirm), 0.016f, cues);
    ASSERT_TRUE(scene.cutting());
    while (scene.cursor_row() * n + scene.cursor_col() != solution[1])
    {
        const int at = scene.cursor_row() * n + scene.cursor_col();
        const int to = solution[1];
        scene.update(nav(at / n < to / n   ? Direction::down
                         : at / n > to / n ? Direction::up
                         : at % n < to % n ? Direction::right
                                           : Direction::left),
                     0.016f, cues);
    }
    scene.update(press(Action::confirm), 0.016f, cues);
    EXPECT_FALSE(scene.cutting());
    EXPECT_EQ(scene.path().size(), 2u);
    // Circle leaves cut mode without pausing.
    scene.update(press(Action::confirm), 0.016f, cues);
    scene.update(press(Action::back), 0.016f, cues);
    EXPECT_FALSE(scene.cutting());
    EXPECT_FALSE(scene.menu_open());

    // The save restores the same puzzle and path.
    const std::string save = scene.save();
    TrailScene restored(fonts);
    restored.start(save, scene.stats());
    EXPECT_EQ(restored.path(), scene.path());
    EXPECT_EQ(restored.puzzle().number, p.number);

    // Follow the solution to the end.
    for (std::size_t i = scene.path().size(); i < solution.size(); ++i)
        scene.update(nav(toward(n, solution[i - 1], solution[i])), 0.016f, cues);
    EXPECT_TRUE(scene.is_solved());
    EXPECT_NE(std::find(cues.begin(), cues.end(), audio::Cue::complete), cues.end());
    EXPECT_FALSE(scene.in_progress());
    for (int frame = 0; frame < 120; ++frame)
        scene.update(InputFrame{}, 1.0f / 60.0f, cues);
    EXPECT_TRUE(scene.menu_open()); // the solved menu follows the flourish
}

TEST(TrailScene, CorruptSavesStartFresh)
{
    ui::Fonts fonts;
    TrailScene scene(fonts);
    scene.start(std::string("\x01\x05garbage", 9), {});
    EXPECT_EQ(scene.path().size(), 1u);
    EXPECT_EQ(scene.puzzle().side, 5);
    scene.new_puzzle(5, 2);
    EXPECT_EQ(scene.puzzle().side, 7);
    EXPECT_EQ(scene.size(), 2);
}
