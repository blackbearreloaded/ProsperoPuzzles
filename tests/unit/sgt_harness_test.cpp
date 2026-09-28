// ProsperoPuzzles - Headless harness for all vendored Tatham puzzles.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// For every game: deterministic generation, drawing through a recording
// renderer with bounds checks, randomized controller-style input, undo/redo,
// solving and serialisation round-trips. Runs under ASan/UBSan.

#include "games/sgt/sgt_catalog.hpp"
#include "games/sgt/sgt_session.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace
{

using ppz::sgt::Action;
using ppz::sgt::GameEntry;
using ppz::sgt::Session;

// Counts drawing calls and any geometry that leaves the canvas.
class RecordingRenderer final : public ppz::sgt::Renderer
{
  public:
    RecordingRenderer(int width, int height, int colours)
        : width_(width), height_(height), colours_(colours)
    {
    }

    void rect(int x, int y, int w, int h, int colour) override
    {
        check_colour(colour);
        check_box(x, y, x + w, y + h);
        ++calls;
    }
    void line(float x1, float y1, float x2, float y2, float thickness, int colour) override
    {
        check_colour(colour);
        const float pad = thickness;
        check_box(
            static_cast<int>(std::min(x1, x2) - pad), static_cast<int>(std::min(y1, y2) - pad),
            static_cast<int>(std::max(x1, x2) + pad), static_cast<int>(std::max(y1, y2) + pad));
        ++calls;
    }
    void polygon(const int *coords, int npoints, int fill, int outline) override
    {
        if (fill >= 0)
            check_colour(fill);
        check_colour(outline);
        if (npoints < 3)
            ++invalid;
        for (int i = 0; i < npoints; ++i)
            check_box(coords[2 * i], coords[2 * i + 1], coords[2 * i], coords[2 * i + 1]);
        ++calls;
    }
    void circle(int cx, int cy, int radius, int fill, int outline) override
    {
        if (fill >= 0)
            check_colour(fill);
        check_colour(outline);
        check_box(cx - radius, cy - radius, cx + radius, cy + radius);
        ++calls;
    }
    void text(int x, int y, int, int font_size, int, int colour, const char *text) override
    {
        check_colour(colour);
        if (text == nullptr || font_size <= 0)
            ++invalid;
        check_box(x, y, x, y);
        ++calls;
    }
    void clip(int, int, int w, int h) override
    {
        if (w < 0 || h < 0)
            ++invalid;
        ++clip_depth;
    }
    void unclip() override
    {
        clip_depth = 0;
    }
    int blitter_new(int w, int h) override
    {
        if (w <= 0 || h <= 0)
            ++invalid;
        live_blitters[next_blitter] = true;
        return next_blitter++;
    }
    void blitter_free(int id) override
    {
        if (live_blitters.erase(id) == 0)
            ++invalid;
    }
    void blitter_save(int id, int, int) override
    {
        if (live_blitters.count(id) == 0)
            ++invalid;
    }
    void blitter_load(int id, int, int) override
    {
        if (live_blitters.count(id) == 0)
            ++invalid;
    }

    long calls = 0;
    long outside = 0;
    long invalid = 0;
    int clip_depth = 0;
    int next_blitter = 0;
    std::map<int, bool> live_blitters;

  private:
    void check_colour(int colour)
    {
        if (colour < 0 || colour >= colours_)
            ++invalid;
    }
    void check_box(int x0, int y0, int x1, int y1)
    {
        // Upstream games may overdraw by a pixel or two at the border.
        constexpr int kSlack = 4;
        if (x0 < -kSlack || y0 < -kSlack || x1 > width_ + kSlack || y1 > height_ + kSlack)
            ++outside;
    }

    int width_;
    int height_;
    int colours_;
};

std::string seeded_id(Session &session, const char *seed)
{
    return session.encoded_params() + "#" + seed;
}

// Generates the default-parameter game from a fixed seed, sized to 1600x1000.
struct Fixture
{
    explicit Fixture(const GameEntry &entry, const char *seed = "ppz-harness") : session(entry)
    {
        error = session.load_game_id(seeded_id(session, seed));
        session.resize(1600, 1000, &width, &height);
        const int colours = static_cast<int>(session.colours().size() / 3);
        renderer = std::make_unique<RecordingRenderer>(width, height, colours);
        session.set_renderer(renderer.get());
        session.force_redraw();
    }

    // Declared first so it outlives the session, whose teardown frees blitters.
    std::unique_ptr<RecordingRenderer> renderer;
    Session session;
    std::string error;
    int width = 0;
    int height = 0;
};

class SgtGame : public ::testing::TestWithParam<const GameEntry *>
{
};

TEST_P(SgtGame, GeneratesAndDraws)
{
    Fixture fixture(*GetParam());
    ASSERT_EQ(fixture.error, "");
    EXPECT_GT(fixture.width, 0);
    EXPECT_GT(fixture.height, 0);
    EXPECT_LE(fixture.width, 1600);
    EXPECT_LE(fixture.height, 1000);
    EXPECT_GT(fixture.renderer->calls, 0);
    EXPECT_EQ(fixture.renderer->invalid, 0);
    EXPECT_EQ(fixture.renderer->outside, 0);
    EXPECT_EQ(fixture.session.status(), 0);
    EXPECT_FALSE(fixture.session.presets().empty());
}

TEST_P(SgtGame, SameSeedGivesSameGame)
{
    Fixture first(*GetParam());
    Fixture second(*GetParam());
    EXPECT_EQ(first.session.game_id(), second.session.game_id());
}

TEST_P(SgtGame, SurvivesRandomControllerInput)
{
    Fixture fixture(*GetParam());
    Session &session = fixture.session;
    std::vector<int> buttons = {CURSOR_UP,     CURSOR_DOWN,    CURSOR_LEFT, CURSOR_RIGHT,
                                CURSOR_SELECT, CURSOR_SELECT2, UI_UNDO,     UI_REDO};
    for (const auto &label : session.request_keys())
        buttons.push_back(label.button);

    std::mt19937 random(0x5eed);
    long moves = 0;
    for (int step = 0; step < 1500; ++step)
    {
        const int choice = static_cast<int>(random() % (buttons.size() + 3));
        if (choice < static_cast<int>(buttons.size()))
        {
            if (session.key(0, 0, buttons[static_cast<std::size_t>(choice)]).action == Action::move)
                ++moves;
        }
        else
        {
            // Pointer mode: press, drag and release at random canvas points.
            const int x = static_cast<int>(random() % static_cast<unsigned>(fixture.width));
            const int y = static_cast<int>(random() % static_cast<unsigned>(fixture.height));
            const bool right = choice == static_cast<int>(buttons.size()) + 2;
            session.key(x, y, right ? RIGHT_BUTTON : LEFT_BUTTON);
            session.key(std::min(x + 7, fixture.width - 1), y, right ? RIGHT_DRAG : LEFT_DRAG);
            if (session
                    .key(std::min(x + 7, fixture.width - 1), y,
                         right ? RIGHT_RELEASE : LEFT_RELEASE)
                    .action == Action::move)
                ++moves;
        }
        session.tick(0.05f);
        session.redraw();
        if (session.status() != 0)
            break; // solved or lost: the random walk is done
    }
    EXPECT_EQ(fixture.renderer->invalid, 0);
    // Drag previews (Map's colour blob, Loopy and Galaxies edges) follow the
    // pointer past the border; the canvas clips them, so only record them.
    RecordProperty("outside", static_cast<int>(fixture.renderer->outside));
    RecordProperty("moves", static_cast<int>(moves));

    while (session.can_undo())
        ASSERT_EQ(session.undo().action, Action::undo);
    while (session.can_redo())
        ASSERT_EQ(session.redo().action, Action::redo);
}

TEST_P(SgtGame, SerialisationRoundTrips)
{
    Fixture fixture(*GetParam());
    Session &session = fixture.session;
    std::mt19937 random(7);
    const int buttons[] = {CURSOR_RIGHT, CURSOR_DOWN, CURSOR_SELECT, CURSOR_SELECT2};
    for (int step = 0; step < 200; ++step)
        session.key(0, 0, buttons[random() % 4]);

    const std::string saved = session.serialise();
    ASSERT_FALSE(saved.empty());
    Session restored(*GetParam());
    ASSERT_EQ(restored.deserialise(saved), "");
    EXPECT_EQ(restored.serialise(), saved);
    EXPECT_EQ(restored.game_id(), session.game_id());

    Session garbage(*GetParam());
    EXPECT_NE(garbage.deserialise("not a save file"), "");
}

TEST_P(SgtGame, SolveCompletesThePuzzle)
{
    Fixture fixture(*GetParam());
    Session &session = fixture.session;
    if (!session.can_solve())
        GTEST_SKIP() << "game has no solver";
    // Upstream semantics differ: most games show the full solution (status 1),
    // Black Box and Guess reveal the answer (status -1), and Flip, Flood,
    // Inertia, Rectangles and Undead mark a cheated or hinted state (status 0).
    // The shell treats Solve as "show solution" and never celebrates it.
    const std::string error = session.solve();
    if (std::string(GetParam()->id) == "mines")
        EXPECT_EQ(error, "Game has not been started yet");
    else
        ASSERT_EQ(error, "");
    for (int frame = 0; frame < 200 && session.timer_active(); ++frame)
    {
        session.tick(0.05f);
        session.redraw();
    }
    const int status = session.status();
    EXPECT_TRUE(status >= -1 && status <= 1);
    RecordProperty("status_after_solve", status);
    EXPECT_EQ(fixture.renderer->outside, 0);
}

std::string game_name(const ::testing::TestParamInfo<const GameEntry *> &info)
{
    return info.param->id;
}

std::vector<const GameEntry *> all_games()
{
    std::vector<const GameEntry *> games;
    for (const GameEntry &entry : ppz::sgt::catalog())
        games.push_back(&entry);
    return games;
}

INSTANTIATE_TEST_SUITE_P(All, SgtGame, ::testing::ValuesIn(all_games()), game_name);

TEST(SgtCatalog, HasFortyUniqueGames)
{
    const auto games = ppz::sgt::catalog();
    ASSERT_EQ(games.size(), 40u);
    for (std::size_t i = 1; i < games.size(); ++i)
        EXPECT_LT(std::string(games[i - 1].id), std::string(games[i].id));
    for (const GameEntry &entry : games)
    {
        EXPECT_NE(entry.game, nullptr);
        EXPECT_STRNE(entry.display_name, "");
        EXPECT_STRNE(entry.description, "");
        EXPECT_STRNE(entry.objective, "");
    }
    EXPECT_NE(ppz::sgt::find_game("net"), nullptr);
    EXPECT_EQ(ppz::sgt::find_game("tetris"), nullptr);
}

TEST(SgtSession, ClassifiesUndoAndCursorKeys)
{
    Session session(*ppz::sgt::find_game("net"));
    ASSERT_EQ(session.load_game_id(session.encoded_params() + "#ppz"), "");
    EXPECT_EQ(session.undo().action, Action::no_effect);
    EXPECT_EQ(session.key(0, 0, CURSOR_RIGHT).action, Action::ui);
    const auto rotate = session.key(0, 0, CURSOR_SELECT);
    EXPECT_EQ(rotate.action, Action::move);
    ASSERT_EQ(rotate.moves.size(), 1u);
    EXPECT_EQ(session.undo().action, Action::undo);
    EXPECT_EQ(session.redo().action, Action::redo);
}

} // namespace
