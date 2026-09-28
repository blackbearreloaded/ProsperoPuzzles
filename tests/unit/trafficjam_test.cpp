// ProsperoPuzzles - Traffic Jam rules, generator and play screen tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/trafficjam/core.hpp"
#include "games/trafficjam/trafficjam_scene.hpp"

#include <gtest/gtest.h>

#include <algorithm>

using namespace ppz;
using namespace ppz::trafficjam;

namespace
{

InputFrame press(Action action)
{
    InputFrame f;
    f.pressed = action_bit(action);
    return f;
}

InputFrame nav(Direction direction)
{
    InputFrame f;
    f.nav = direction;
    return f;
}

bool has_cue(const std::vector<audio::Cue> &cues, audio::Cue cue)
{
    return std::find(cues.begin(), cues.end(), cue) != cues.end();
}

// Red car at the far left, a truck standing in column 3 across the exit row.
Puzzle blocked_lot()
{
    Puzzle p;
    p.count = 2;
    p.vehicles[0] = {2, false, kExitRow};
    p.vehicles[1] = {3, true, 3};
    p.start[0] = 0;
    p.start[1] = 0;
    return p;
}

// Walks the cursor onto vehicle v (the lot is 6 x 6, so a few steps suffice).
void move_cursor_to(TrafficJamScene &scene, int v, std::vector<audio::Cue> &cues)
{
    const Puzzle &p = scene.puzzle();
    const Vehicle &vehicle = p.vehicles[static_cast<std::size_t>(v)];
    const int at = scene.positions()[static_cast<std::size_t>(v)];
    const int col = vehicle.vertical ? vehicle.lane : at;
    const int row = vehicle.vertical ? at : vehicle.lane;
    for (int guard = 0; guard < 40; ++guard)
    {
        const auto cells = occupancy(p, scene.positions());
        if (cells[static_cast<std::size_t>(scene.cursor_row() * kSide + scene.cursor_col())] == v)
            return;
        scene.update(nav(scene.cursor_row() < row   ? Direction::down
                         : scene.cursor_row() > row ? Direction::up
                         : scene.cursor_col() < col ? Direction::right
                                                    : Direction::left),
                     0.016f, cues);
    }
    FAIL() << "cursor never reached vehicle " << v;
}

// Grabs vehicle v, slides it to `to` one press at a time, and lets go.
void play_move(TrafficJamScene &scene, const Move &move, std::vector<audio::Cue> &cues)
{
    move_cursor_to(scene, move.vehicle, cues);
    scene.update(press(Action::confirm), 0.016f, cues);
    ASSERT_EQ(scene.grabbed(), move.vehicle);
    const bool vertical = scene.puzzle().vehicles[static_cast<std::size_t>(move.vehicle)].vertical;
    const auto v = static_cast<std::size_t>(move.vehicle);
    for (int guard = 0; guard < kSide && scene.positions()[v] != move.to; ++guard)
    {
        const bool forward = move.to > scene.positions()[v];
        scene.update(nav(vertical ? (forward ? Direction::down : Direction::up)
                                  : (forward ? Direction::right : Direction::left)),
                     0.016f, cues);
    }
    EXPECT_EQ(scene.positions()[v], move.to);
    scene.update(press(Action::confirm), 0.016f, cues);
}

} // namespace

TEST(TrafficJam, GeneratedPuzzlesNeedTheirParMoves)
{
    constexpr int kMostPar[3] = {8, 15, 99};
    for (int level = 0; level < 3; ++level)
    {
        for (std::uint64_t seed = 1; seed <= 5; ++seed)
        {
            const Puzzle p = generate(seed * 104729 + static_cast<std::uint64_t>(level), level);
            ASSERT_TRUE(valid(p, p.start)) << "level " << level << " seed " << seed;
            EXPECT_FALSE(p.vehicles[0].vertical);
            EXPECT_EQ(p.vehicles[0].lane, kExitRow);
            for (int v = 1; v < p.count; ++v)
            {
                const Vehicle &vehicle = p.vehicles[static_cast<std::size_t>(v)];
                EXPECT_FALSE(!vehicle.vertical && vehicle.lane == kExitRow);
            }
            EXPECT_GE(p.par, kMinPar[level]);
            EXPECT_LE(p.par, kMostPar[level]);
            std::vector<Move> path;
            ASSERT_EQ(solve(p, p.start, &path), p.par) << "level " << level << " seed " << seed;
            ASSERT_EQ(static_cast<int>(path.size()), p.par);
            // Playing the solution frees the red car, and only at its last move.
            Positions pos = p.start;
            for (std::size_t i = 0; i < path.size(); ++i)
            {
                EXPECT_FALSE(solved(p, pos));
                const auto v = static_cast<std::size_t>(path[i].vehicle);
                const int dir = path[i].to > pos[v] ? 1 : -1;
                EXPECT_GE(slide_room(p, pos, path[i].vehicle, dir), std::abs(path[i].to - pos[v]));
                pos[v] = static_cast<std::uint8_t>(path[i].to);
                ASSERT_TRUE(valid(p, pos));
            }
            EXPECT_TRUE(solved(p, pos));
        }
    }
    // The same seed gives the same lot.
    const Puzzle a = generate(7, 2);
    const Puzzle b = generate(7, 2);
    EXPECT_EQ(a.count, b.count);
    EXPECT_EQ(a.start, b.start);
    EXPECT_EQ(a.par, b.par);
}

TEST(TrafficJam, SlidingRoomOverlapsAndTheExit)
{
    Puzzle p = blocked_lot();
    Positions pos = p.start;
    ASSERT_TRUE(valid(p, pos));
    EXPECT_EQ(slide_room(p, pos, 0, 1), 1);  // up to the truck
    EXPECT_EQ(slide_room(p, pos, 0, -1), 0); // against the wall
    EXPECT_EQ(slide_room(p, pos, 1, 1), 3);
    EXPECT_EQ(slide_room(p, pos, 1, -1), 0);
    EXPECT_FALSE(solved(p, pos));
    std::vector<Move> path;
    EXPECT_EQ(solve(p, pos, &path), 2); // truck down, then the red car out
    ASSERT_EQ(path.size(), 2u);
    EXPECT_EQ(path[0].vehicle, 1);
    EXPECT_EQ(path[1].vehicle, 0);
    EXPECT_EQ(path[1].to, 4);

    pos[0] = 3; // overlaps the truck
    EXPECT_FALSE(valid(p, pos));
    pos[1] = 3;
    pos[0] = 4;
    EXPECT_TRUE(valid(p, pos));
    EXPECT_TRUE(solved(p, pos));
    EXPECT_EQ(solve(p, pos), 0);

    // Walled in for good: a horizontal truck in the exit row ahead of the red car.
    p.count = 2;
    p.vehicles[1] = {3, false, kExitRow};
    pos[0] = 0;
    pos[1] = 3;
    EXPECT_EQ(solve(p, pos), -1);
}

TEST(TrafficJamScene, GrabsSlidesUndoesSavesAndSolves)
{
    ui::Fonts fonts;
    TrafficJamScene scene(fonts);
    scene.start({}, {});
    scene.new_puzzle(99, 0);
    std::vector<audio::Cue> cues;
    const Puzzle p = scene.puzzle();
    const Positions start = scene.positions();
    std::vector<Move> path;
    ASSERT_EQ(solve(p, start, &path), p.par);
    ASSERT_GE(p.par, kMinPar[0]);

    // Cross on an empty bay is rejected.
    const auto cells = occupancy(p, start);
    const auto empty =
        static_cast<int>(std::find(cells.begin(), cells.end(), std::int8_t{-1}) - cells.begin());
    ASSERT_LT(empty, kCells);
    for (int guard = 0; guard < 40 && scene.cursor_row() * kSide + scene.cursor_col() != empty;
         ++guard)
        scene.update(nav(scene.cursor_row() < empty / kSide   ? Direction::down
                         : scene.cursor_row() > empty / kSide ? Direction::up
                         : scene.cursor_col() < empty % kSide ? Direction::right
                                                              : Direction::left),
                     0.016f, cues);
    ASSERT_EQ(scene.cursor_row() * kSide + scene.cursor_col(), empty);
    cues.clear();
    scene.update(press(Action::confirm), 0.016f, cues);
    EXPECT_TRUE(has_cue(cues, audio::Cue::invalid));
    EXPECT_EQ(scene.grabbed(), -1);
    cues.clear();

    // The first solution move: one grab, however many cells, is one undo step.
    play_move(scene, path[0], cues);
    EXPECT_TRUE(has_cue(cues, audio::Cue::pickup));
    EXPECT_TRUE(has_cue(cues, audio::Cue::slide));
    EXPECT_TRUE(has_cue(cues, audio::Cue::drop));
    EXPECT_EQ(scene.grabbed(), -1);
    Positions after = start;
    after[static_cast<std::size_t>(path[0].vehicle)] = static_cast<std::uint8_t>(path[0].to);
    EXPECT_EQ(scene.positions(), after);
    EXPECT_TRUE(scene.in_progress());
    scene.update(press(Action::page_prev), 0.016f, cues);
    EXPECT_EQ(scene.positions(), start);
    scene.update(press(Action::page_next), 0.016f, cues);
    EXPECT_EQ(scene.positions(), after);

    // While held: across the lane is a soft no, and Circle lets go (no pause menu).
    move_cursor_to(scene, 0, cues);
    scene.update(press(Action::confirm), 0.016f, cues);
    ASSERT_EQ(scene.grabbed(), 0);
    cues.clear();
    scene.update(nav(Direction::up), 0.016f, cues);
    EXPECT_TRUE(has_cue(cues, audio::Cue::invalid));
    EXPECT_EQ(scene.positions(), after);
    EXPECT_EQ(scene.grabbed(), 0);
    scene.update(press(Action::back), 0.016f, cues);
    EXPECT_EQ(scene.grabbed(), -1);
    EXPECT_FALSE(scene.menu_open());

    // The save restores the same lot and positions.
    const std::string save = scene.save();
    TrafficJamScene restored(fonts);
    restored.start(save, scene.stats());
    EXPECT_EQ(restored.positions(), scene.positions());
    EXPECT_EQ(restored.puzzle().count, p.count);
    EXPECT_EQ(restored.puzzle().start, p.start);
    EXPECT_EQ(restored.puzzle().par, p.par);

    // Play out the rest of the solution.
    for (std::size_t i = 1; i < path.size(); ++i)
        play_move(scene, path[i], cues);
    EXPECT_TRUE(scene.is_solved());
    EXPECT_TRUE(has_cue(cues, audio::Cue::complete));
    EXPECT_FALSE(scene.in_progress());
    for (int frame = 0; frame < 120; ++frame)
        scene.update(InputFrame{}, 1.0f / 60.0f, cues);
    EXPECT_TRUE(scene.menu_open()); // the solved menu follows the drive-out
}

TEST(TrafficJamScene, CircleOpensPauseAndLevelsStartNewPuzzles)
{
    ui::Fonts fonts;
    TrafficJamScene scene(fonts);
    scene.start({}, {});
    std::vector<audio::Cue> cues;
    scene.update(press(Action::back), 0.016f, cues);
    EXPECT_TRUE(scene.menu_open());
    scene.update(press(Action::confirm), 0.016f, cues);
    EXPECT_FALSE(scene.menu_open());
    scene.new_puzzle(5, 2);
    EXPECT_EQ(scene.size(), 2);
    EXPECT_GE(scene.puzzle().par, kMinPar[2]);
}
