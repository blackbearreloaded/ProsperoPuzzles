// ProsperoPuzzles - Update dialog flow tests.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/update_dialog.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

namespace
{

using ppz::UpdatePhase;
using ppz::ui::UpdateDialog;

class FakeUpdater final : public ppz::Updater
{
  public:
    ppz::UpdateProgress progress;
    bool begins = true;
    bool applies = true;
    int begun = 0;
    int cancelled = 0;
    int applied = 0;
    int finished = 0;

    bool take(ppz::UpdateOffer *) override
    {
        return false;
    }
    bool begin() override
    {
        ++begun;
        return begins;
    }
    ppz::UpdateProgress poll() override
    {
        return progress;
    }
    void cancel() override
    {
        ++cancelled;
    }
    bool apply() override
    {
        ++applied;
        return applies;
    }
    void finish() override
    {
        ++finished;
    }
};

constexpr float kFrame = 1.0f / 60.0f;

ppz::InputFrame press(ppz::Action action)
{
    ppz::InputFrame frame;
    frame.pressed = ppz::action_bit(action);
    return frame;
}

ppz::InputFrame nav(ppz::Direction direction)
{
    ppz::InputFrame frame;
    frame.nav = direction;
    return frame;
}

const ppz::UpdateOffer kOffer{true, "01.000.020", "01.000.020", 39167016};

struct Harness
{
    FakeUpdater updater;
    UpdateDialog dialog;
    std::vector<ppz::audio::Cue> cues;

    UpdateDialog::Result step(const ppz::InputFrame &input = {})
    {
        return dialog.update(input, kFrame, cues);
    }
    // Runs idle frames until a result other than none, or `frames` have passed.
    UpdateDialog::Result run(int frames)
    {
        for (int i = 0; i < frames; ++i)
        {
            const UpdateDialog::Result result = step();
            if (result != UpdateDialog::Result::none)
                return result;
        }
        return UpdateDialog::Result::none;
    }
    bool heard(ppz::audio::Cue cue) const
    {
        return std::find(cues.begin(), cues.end(), cue) != cues.end();
    }
};

TEST(UpdateDialog, LaterClosesWithoutTouchingTheUpdater)
{
    for (const bool with_circle : {true, false})
    {
        Harness h;
        h.dialog.open(h.updater, kOffer, false);
        ASSERT_TRUE(h.dialog.is_open());
        if (with_circle)
        {
            h.step(press(ppz::Action::back));
        }
        else
        {
            h.step(nav(ppz::Direction::right));
            h.step(press(ppz::Action::confirm));
        }
        EXPECT_FALSE(h.dialog.is_open());
        EXPECT_EQ(h.updater.begun, 0);
        EXPECT_EQ(h.run(600), UpdateDialog::Result::none);
    }
}

TEST(UpdateDialog, UpdateNowDownloadsStagesAndAsksTheAppToClose)
{
    Harness h;
    h.dialog.open(h.updater, kOffer, false);
    EXPECT_EQ(h.step(press(ppz::Action::confirm)), UpdateDialog::Result::none);
    EXPECT_EQ(h.updater.begun, 1);

    h.updater.progress = {UpdatePhase::downloading, 1000, 4000, "about 3 s left", {}};
    EXPECT_EQ(h.run(120), UpdateDialog::Result::none);
    h.updater.progress = {UpdatePhase::unpacking, 10, 20, {}, {}};
    EXPECT_EQ(h.run(120), UpdateDialog::Result::none);
    EXPECT_EQ(h.updater.applied, 0);

    h.updater.progress.phase = UpdatePhase::ready;
    EXPECT_EQ(h.step(), UpdateDialog::Result::staged);
    EXPECT_EQ(h.updater.applied, 1);
    EXPECT_TRUE(h.heard(ppz::audio::Cue::complete));
    // Staged is said once; nothing cancels now, and the app closes after the message.
    EXPECT_EQ(h.step(press(ppz::Action::back)), UpdateDialog::Result::none);
    EXPECT_EQ(h.updater.cancelled, 0);
    EXPECT_EQ(h.run(60), UpdateDialog::Result::none);
    EXPECT_EQ(h.run(240), UpdateDialog::Result::quit);
    EXPECT_EQ(h.updater.applied, 1);
    EXPECT_EQ(h.updater.finished, 0);
}

TEST(UpdateDialog, CircleCancelsAndTheDialogClosesOnceTheJobStopped)
{
    Harness h;
    h.dialog.open(h.updater, kOffer, false);
    h.step(press(ppz::Action::confirm));
    h.updater.progress = {UpdatePhase::downloading, 1000, 4000, {}, {}};
    h.run(10);
    h.step(press(ppz::Action::back));
    EXPECT_EQ(h.updater.cancelled, 1);
    EXPECT_TRUE(h.dialog.is_open());
    // A second press does nothing, and a job that got ready meanwhile is not applied.
    h.step(press(ppz::Action::back));
    EXPECT_EQ(h.updater.cancelled, 1);
    h.updater.progress.phase = UpdatePhase::ready;
    EXPECT_EQ(h.run(30), UpdateDialog::Result::none);
    EXPECT_EQ(h.updater.applied, 0);

    h.updater.progress.phase = UpdatePhase::cancelled;
    EXPECT_EQ(h.step(), UpdateDialog::Result::none);
    EXPECT_FALSE(h.dialog.is_open());
    EXPECT_EQ(h.updater.finished, 1);
}

TEST(UpdateDialog, AFailureOffersToTryAgain)
{
    Harness h;
    h.dialog.open(h.updater, kOffer, false);
    h.step(press(ppz::Action::confirm));
    h.updater.progress.phase = UpdatePhase::failed;
    h.updater.progress.error = "The download doesn't match the catalog's listing";
    EXPECT_EQ(h.step(), UpdateDialog::Result::none);
    EXPECT_TRUE(h.dialog.is_open());
    EXPECT_EQ(h.updater.finished, 1);
    EXPECT_TRUE(h.heard(ppz::audio::Cue::ui_error));

    // Try again begins a second job.
    h.updater.progress = {};
    h.step(press(ppz::Action::confirm));
    EXPECT_EQ(h.updater.begun, 2);
    h.updater.progress.phase = UpdatePhase::failed;
    h.step();
    // Close leaves the app as it was.
    h.step(nav(ppz::Direction::right));
    h.step(press(ppz::Action::confirm));
    EXPECT_FALSE(h.dialog.is_open());
    EXPECT_EQ(h.updater.begun, 2);
}

TEST(UpdateDialog, AHelperThatCannotStartOrDoesNotAnswerIsAFailure)
{
    Harness h;
    h.updater.begins = false;
    h.dialog.open(h.updater, kOffer, false);
    h.step(press(ppz::Action::confirm));
    EXPECT_TRUE(h.dialog.is_open());
    EXPECT_TRUE(h.heard(ppz::audio::Cue::ui_error));
    EXPECT_EQ(h.run(600), UpdateDialog::Result::none);

    Harness silent;
    silent.updater.applies = false;
    silent.dialog.open(silent.updater, kOffer, false);
    silent.step(press(ppz::Action::confirm));
    silent.updater.progress.phase = UpdatePhase::ready;
    EXPECT_EQ(silent.step(), UpdateDialog::Result::none);
    EXPECT_EQ(silent.updater.finished, 1);
    EXPECT_EQ(silent.run(600), UpdateDialog::Result::none);
}

TEST(UpdateDialog, ACopyThatCannotUpdateItselfOnlyTells)
{
    Harness h;
    h.dialog.open(h.updater, {false, "01.000.020", "01.000.020", 0}, false);
    EXPECT_TRUE(h.dialog.is_open());
    h.step(press(ppz::Action::confirm));
    EXPECT_FALSE(h.dialog.is_open());
    EXPECT_EQ(h.updater.begun, 0);
}

TEST(UpdateDialog, DrawsEveryStage)
{
    // Layout is checked by the host snapshots; here only that drawing is balanced.
    Harness h;
    ppz::gfx::DrawList list;
    ppz::ui::Fonts fonts;
    ppz::gfx::Font font;
    fonts.regular = &font;
    fonts.semibold = &font;
    h.dialog.draw(list, fonts);
    EXPECT_TRUE(list.instances().empty());
    h.dialog.open(h.updater, kOffer, true);
    h.run(30);
    h.dialog.draw(list, fonts);
    EXPECT_FALSE(list.instances().empty());
    h.step(press(ppz::Action::confirm));
    for (const UpdatePhase phase :
         {UpdatePhase::starting, UpdatePhase::downloading, UpdatePhase::unpacking})
    {
        h.updater.progress = {phase, 5, 10, "about 2 s left", {}};
        h.run(5);
        list.clear();
        h.dialog.draw(list, fonts);
        EXPECT_FALSE(list.instances().empty());
    }
}

} // namespace
