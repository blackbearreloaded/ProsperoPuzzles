// ProsperoPuzzles - 2048 and Tenfold screen and save-format tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "games/g2048/g2048_scene.hpp"
#include "games/tenfold/tenfold_scene.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace
{

ppz::InputFrame nav(ppz::Direction direction)
{
    ppz::InputFrame frame;
    frame.nav = direction;
    return frame;
}

ppz::InputFrame press(ppz::Action action)
{
    ppz::InputFrame frame;
    frame.pressed = ppz::action_bit(action);
    return frame;
}

TEST(G2048Scene, SavesRoundTripWithUndo)
{
    ppz::g2048::Game game{11};
    game.move(ppz::g2048::Direction::left);
    std::optional<ppz::g2048::Snapshot> undo = game.snapshot();
    game.move(ppz::g2048::Direction::up);
    const std::string data = ppz::g2048::encode_game(game.snapshot(), 2, undo);
    ppz::g2048::Snapshot state;
    std::uint32_t moves = 0;
    std::optional<ppz::g2048::Snapshot> restored;
    ASSERT_TRUE(ppz::g2048::decode_game(data, &state, &moves, &restored));
    EXPECT_EQ(state.cells, game.snapshot().cells);
    EXPECT_EQ(moves, 2u);
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->cells, undo->cells);
    EXPECT_FALSE(ppz::g2048::decode_game(data.substr(1), &state, &moves, &restored));
    EXPECT_FALSE(ppz::g2048::decode_game(data + "x", &state, &moves, &restored));

    ppz::g2048::Stats stats{123456, 4, 1, 11};
    ppz::g2048::Stats read;
    ASSERT_TRUE(ppz::g2048::decode_stats(ppz::g2048::encode_stats(stats), &read));
    EXPECT_EQ(read.best, 123456u);
    EXPECT_EQ(read.won, 1u);
}

TEST(G2048Scene, MovesUndoesPausesAndResumes)
{
    ppz::ui::Fonts fonts;
    ppz::g2048::G2048Scene scene(fonts);
    scene.start({}, {});
    std::vector<ppz::audio::Cue> cues;
    int moved = 0;
    for (auto d :
         {ppz::Direction::left, ppz::Direction::up, ppz::Direction::right, ppz::Direction::down})
    {
        const auto before = scene.game().snapshot().cells;
        scene.update(nav(d), 0.016f, cues);
        moved += before != scene.game().snapshot().cells ? 1 : 0;
    }
    ASSERT_GT(moved, 0);
    EXPECT_TRUE(scene.in_progress());

    // Holding a direction does not keep sliding.
    auto repeat = nav(ppz::Direction::left);
    repeat.nav_repeat = true;
    const auto held = scene.game().snapshot().cells;
    scene.update(repeat, 0.016f, cues);
    EXPECT_EQ(held, scene.game().snapshot().cells);

    const std::string saved = scene.save();
    ppz::g2048::G2048Scene resumed(fonts);
    resumed.start(saved, scene.stats());
    EXPECT_EQ(resumed.game().snapshot().cells, scene.game().snapshot().cells);

    scene.update(press(ppz::Action::menu), 0.016f, cues);
    EXPECT_TRUE(scene.menu_open());
    scene.update(press(ppz::Action::back), 0.016f, cues);
    EXPECT_FALSE(scene.menu_open());
}

TEST(TenfoldScene, SelectMergeHintUndoAndResume)
{
    ppz::ui::Fonts fonts;
    ppz::tenfold::TenfoldScene scene(fonts);
    scene.start({}, {});
    std::vector<ppz::audio::Cue> cues;
    scene.update(press(ppz::Action::west), 0.016f, cues); // hint moves onto a group
    const int cursor = scene.cursor();
    ASSERT_GE(scene.game().group_at(static_cast<std::uint8_t>(cursor)).count, 2);
    scene.update(press(ppz::Action::confirm), 0.016f, cues);
    EXPECT_TRUE(scene.has_selection());
    const auto score = scene.game().snapshot().score;
    scene.update(press(ppz::Action::confirm), 0.016f, cues); // merge at the cursor
    EXPECT_FALSE(scene.has_selection());
    EXPECT_GT(scene.game().snapshot().score, score);
    EXPECT_TRUE(scene.in_progress());

    ppz::tenfold::TenfoldScene resumed(fonts);
    resumed.start(scene.save(), scene.stats());
    EXPECT_EQ(resumed.game().snapshot().cells, scene.game().snapshot().cells);

    scene.update(press(ppz::Action::page_prev), 0.016f, cues);
    EXPECT_EQ(scene.game().snapshot().moves, 0u);

    ppz::tenfold::Snapshot state;
    EXPECT_FALSE(ppz::tenfold::decode_game("", &state));
    ppz::tenfold::Stats stats{99, 3, 1, 10};
    ppz::tenfold::Stats read;
    ASSERT_TRUE(ppz::tenfold::decode_stats(ppz::tenfold::encode_stats(stats), &read));
    EXPECT_EQ(read.highest, 10);
}

TEST(TenfoldScene, CircleDeselectsBeforePausing)
{
    ppz::ui::Fonts fonts;
    ppz::tenfold::TenfoldScene scene(fonts);
    scene.start({}, {});
    std::vector<ppz::audio::Cue> cues;
    scene.update(press(ppz::Action::west), 0.016f, cues);
    scene.update(press(ppz::Action::confirm), 0.016f, cues);
    ASSERT_TRUE(scene.has_selection());
    scene.update(press(ppz::Action::back), 0.016f, cues);
    EXPECT_FALSE(scene.has_selection());
    EXPECT_FALSE(scene.menu_open());
    scene.update(press(ppz::Action::back), 0.016f, cues);
    EXPECT_TRUE(scene.menu_open());
}

} // namespace
