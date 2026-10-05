// ProsperoPuzzles - The update dialog: offer, progress, and the close for the update.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/update_dialog.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ppz::ui
{

namespace
{

constexpr float kCenterX = 960.0f;
constexpr float kPanelWidth = 820.0f;
// With two buttons, and while the update works (no buttons).
constexpr float kPanelHeight = 700.0f;
constexpr float kWorkingHeight = 610.0f;
// Offsets from the panel's top.
constexpr float kRingY = 178.0f;
constexpr float kRingRadius = 92.0f;
constexpr float kRingWidth = 10.0f;
constexpr float kHeadline = 344.0f;
constexpr float kButtonsTop = 516.0f;
constexpr float kButtonWidth = 340.0f;
constexpr float kButtonHeight = 76.0f;
constexpr float kButtonGap = 16.0f;
// How long "closes now" shows before the app closes.
constexpr float kClosingSeconds = 3.0f;
constexpr float kPi = 3.14159265f;

const gfx::Color kAccent = gfx::Color::rgb(0x159a80);
const gfx::Color kAccentPale = gfx::Color::rgb(0x8fe6d0);
const gfx::Color kWarning = gfx::Color::rgb(0xd65a4a);
const gfx::Color kTrack = gfx::Color::rgb(0x1b1d2b, 0.10f);

std::string megabytes(std::uint64_t bytes)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    return text;
}

// An arc from `start` (radians, 0 at the right, clockwise on screen) over
// `sweep`, as short round strokes.
void arc(gfx::DrawList &list, float cx, float cy, float radius, float width, float start,
         float sweep, gfx::Color color)
{
    if (sweep <= 0.001f)
        return;
    const int steps = std::max(2, static_cast<int>(sweep / (kPi / 60.0f)));
    float x = cx + radius * std::cos(start);
    float y = cy + radius * std::sin(start);
    for (int i = 1; i <= steps; ++i)
    {
        const float a = start + sweep * static_cast<float>(i) / static_cast<float>(steps);
        const float nx = cx + radius * std::cos(a);
        const float ny = cy + radius * std::sin(a);
        list.line(x, y, nx, ny, width, color);
        x = nx;
        y = ny;
    }
}

// One line centred on the panel, made smaller when it would not fit.
void centred(gfx::DrawList &list, const gfx::Font &font, std::uint32_t texture,
             const std::string &text, float baseline, float size, gfx::Color color)
{
    const float room = kPanelWidth - 96.0f;
    const float width = font.measure(text, size);
    const float fitted = width > room ? size * room / width : size;
    list.text(font, texture, text, kCenterX, baseline, fitted, color, gfx::Align::center);
}

struct PanelHint
{
    Button button;
    const char *label;
};

void hints(gfx::DrawList &list, const Fonts &fonts, const PanelHint *items, int count, float cy)
{
    constexpr float kGlyph = 30.0f;
    constexpr float kLabel = 22.0f;
    constexpr float kSpace = 10.0f;
    constexpr float kBetween = 40.0f;
    float width = kBetween * static_cast<float>(count - 1);
    for (int i = 0; i < count; ++i)
        width += button_width(items[i].button, kGlyph) + kSpace +
                 fonts.regular->measure(items[i].label, kLabel);
    float x = kCenterX - width * 0.5f;
    for (int i = 0; i < count; ++i)
    {
        draw_button(list, fonts, items[i].button, x, cy, kGlyph);
        x += button_width(items[i].button, kGlyph) + kSpace;
        x += list.text(*fonts.regular, fonts.regular_texture, items[i].label, x, cy + 8.0f, kLabel,
                       theme::kInkMuted) +
             kBetween;
    }
}

std::string capitalised(std::string text)
{
    if (!text.empty() && text[0] >= 'a' && text[0] <= 'z')
        text[0] = static_cast<char>(text[0] - 'a' + 'A');
    return text;
}

} // namespace

void UpdateDialog::open(Updater &updater, const UpdateOffer &offer, bool reduced_motion)
{
    updater_ = &updater;
    offer_ = offer;
    reduced_motion_ = reduced_motion;
    progress_ = {};
    error_.clear();
    open_ = true;
    choice_ = 0;
    choice_x_.snap(0.0f);
    fraction_.snap(0.0f);
    height_.snap(kPanelHeight);
    enter(offer.installable ? Stage::offer : Stage::notice);
}

void UpdateDialog::enter(Stage stage)
{
    stage_ = stage;
    stage_time_ = 0.0f;
}

void UpdateDialog::begin(std::vector<audio::Cue> &cues)
{
    progress_ = {};
    fraction_.snap(0.0f);
    if (updater_->begin())
    {
        enter(Stage::working);
        cues.push_back(audio::Cue::ui_select);
        return;
    }
    fail("The update helper could not start", cues);
}

void UpdateDialog::fail(const std::string &reason, std::vector<audio::Cue> &cues)
{
    error_ = reason;
    choice_ = 0;
    choice_x_.snap(0.0f);
    enter(Stage::failed);
    cues.push_back(audio::Cue::ui_error);
}

void UpdateDialog::close(std::vector<audio::Cue> &cues)
{
    open_ = false;
    cues.push_back(audio::Cue::ui_pause_close);
}

UpdateDialog::Result UpdateDialog::update(const InputFrame &input, float dt,
                                          std::vector<audio::Cue> &cues)
{
    fade_.target = open_ ? 1.0f : 0.0f;
    fade_.update(dt, 16.0f);
    choice_x_.target = static_cast<float>(choice_);
    choice_x_.update(dt, 20.0f);
    const bool buttons = stage_ == Stage::offer || stage_ == Stage::failed;
    height_.target = buttons || stage_ == Stage::notice ? kPanelHeight : kWorkingHeight;
    height_.update(dt, 13.0f);
    fraction_.update(dt, 9.0f);
    if (!open_)
        return Result::none;
    stage_time_ += dt;
    time_ += dt;

    switch (stage_)
    {
    case Stage::offer:
    case Stage::failed:
        if (input.nav == Direction::left || input.nav == Direction::right)
        {
            const int choice = input.nav == Direction::right ? 1 : 0;
            if (choice != choice_)
            {
                choice_ = choice;
                cues.push_back(audio::Cue::ui_focus);
            }
        }
        else if (input.is_pressed(Action::confirm) && choice_ == 0)
            begin(cues);
        else if (input.is_pressed(Action::confirm) || input.is_pressed(Action::back))
            close(cues); // asked again the next time the app opens
        break;
    case Stage::notice:
        if (input.is_pressed(Action::confirm) || input.is_pressed(Action::back))
            close(cues);
        break;
    case Stage::working:
    case Stage::cancelling:
    {
        progress_ = updater_->poll();
        const UpdatePhase phase = progress_.phase;
        if (phase == UpdatePhase::cancelled)
        {
            updater_->finish();
            close(cues);
            break;
        }
        if (phase == UpdatePhase::failed)
        {
            updater_->finish();
            fail(progress_.error.empty() ? "The update could not be installed" : progress_.error,
                 cues);
            break;
        }
        if (phase == UpdatePhase::ready && stage_ == Stage::working)
        {
            if (!updater_->apply())
            {
                updater_->finish();
                fail("The update helper did not answer", cues);
                break;
            }
            fraction_.target = 1.0f;
            enter(Stage::closing);
            cues.push_back(audio::Cue::complete);
            return Result::staged;
        }
        // The share done: of the download, then of the unpacking.
        if (progress_.total > 0)
            fraction_.target = static_cast<float>(static_cast<double>(progress_.done) /
                                                  static_cast<double>(progress_.total));
        else if (phase == UpdatePhase::unpacking)
            fraction_.target = 1.0f;
        if (stage_ == Stage::working && input.is_pressed(Action::back))
        {
            updater_->cancel();
            enter(Stage::cancelling);
            cues.push_back(audio::Cue::ui_back);
        }
        break;
    }
    case Stage::closing:
        // Once the message has been read the app ends; the helper finishes.
        if (stage_time_ >= kClosingSeconds)
            return Result::quit;
        break;
    }
    return Result::none;
}

void UpdateDialog::draw(gfx::DrawList &list, const Fonts &fonts) const
{
    const float fade = fade_.value;
    if (fade <= 0.01f)
        return;
    const float motion = reduced_motion_ ? 0.0f : 1.0f;
    list.push_opacity(std::min(fade, 1.0f));
    list.rounded_rect({0, 0, 1920, 1080}, 0, gfx::Color::rgb(0x05070f, 0.66f));
    const float height = std::clamp(height_.value, kWorkingHeight - 20.0f, kPanelHeight + 20.0f);
    const float top = 540.0f - height * 0.5f + 24.0f * (1.0f - fade) * motion;
    const gfx::Rect panel{kCenterX - kPanelWidth * 0.5f, top, kPanelWidth, height};
    list.shadow({panel.x, panel.y + 18, panel.w, panel.h}, 30, 44, theme::kShadow);
    list.rounded_rect(panel, 30, theme::kPaper);

    const float t = stage_time_;
    const bool failed = stage_ == Stage::failed;
    const gfx::Color accent = failed ? kWarning : kAccent;
    const float ring_y = top + kRingY;
    const float breathe = 0.5f + 0.5f * std::sin(time_ * 2.4f);
    const gfx::Font &regular = *fonts.regular;
    const gfx::Font &semibold = *fonts.semibold;

    // A soft glow behind the ring, breathing while it waits.
    list.shadow(
        {kCenterX - kRingRadius, ring_y - kRingRadius, kRingRadius * 2.0f, kRingRadius * 2.0f},
        kRingRadius, 56.0f, accent.with_alpha(0.16f + 0.12f * breathe * motion));
    list.circle(kCenterX, ring_y, kRingRadius - kRingWidth, theme::kPaperShade);
    list.ring(kCenterX, ring_y, kRingRadius, kRingWidth, kTrack);

    const auto buttons = [&](const char *first, const char *second)
    {
        const float left = kCenterX - kButtonWidth - kButtonGap * 0.5f;
        const float step = kButtonWidth + kButtonGap;
        const float y = top + kButtonsTop;
        for (int i = 0; i < 2; ++i)
            list.rounded_rect({left + step * static_cast<float>(i), y, kButtonWidth, kButtonHeight},
                              20, theme::kPaperShade);
        list.rounded_rect({left + step * choice_x_.value, y, kButtonWidth, kButtonHeight}, 20,
                          theme::kInk);
        for (int i = 0; i < 2; ++i)
            list.text(semibold, fonts.semibold_texture, i == 0 ? first : second,
                      left + step * static_cast<float>(i) + kButtonWidth * 0.5f, y + 49.0f, 28,
                      i == choice_ ? theme::kTextOnDark : theme::kInk, gfx::Align::center);
    };
    const float hint_y = top + height - 50.0f;

    switch (stage_)
    {
    case Stage::offer:
    case Stage::notice:
    {
        // The ring fills once as the dialog rises, around an arrow that
        // settles into its tray.
        const float fill = tween::cubic_out(t / 0.9f);
        arc(list, kCenterX, ring_y, kRingRadius, kRingWidth, -kPi * 0.5f, 2.0f * kPi * fill,
            accent);
        const float drop = (1.0f - tween::back_out(t / 0.7f)) * -26.0f * motion;
        const float bob = std::sin(time_ * 2.2f) * 3.0f * motion;
        const float ay = ring_y - 4.0f + drop + bob;
        list.line(kCenterX, ay - 30.0f, kCenterX, ay + 16.0f, 7.0f, accent);
        list.line(kCenterX - 18.0f, ay - 2.0f, kCenterX, ay + 16.0f, 7.0f, accent);
        list.line(kCenterX + 18.0f, ay - 2.0f, kCenterX, ay + 16.0f, 7.0f, accent);
        list.line(kCenterX - 30.0f, ring_y + 38.0f, kCenterX + 30.0f, ring_y + 38.0f, 7.0f, accent);

        centred(list, semibold, fonts.semibold_texture, "Update available", top + kHeadline, 44,
                theme::kInk);
        if (stage_ == Stage::notice)
        {
            centred(list, regular, fonts.regular_texture,
                    "Version " + offer_.version + " of ProsperoPuzzles is out.", top + 392.0f, 26,
                    theme::kInk);
            centred(list, regular, fonts.regular_texture,
                    "This copy can't update itself. Get the new version from", top + 436.0f, 22,
                    theme::kInkMuted);
            centred(list, semibold, fonts.semibold_texture, "homebrew.page/app/PPSA99006",
                    top + 470.0f, 22, kAccent);
            const float y = top + kButtonsTop;
            list.rounded_rect({kCenterX - kButtonWidth * 0.5f, y, kButtonWidth, kButtonHeight}, 20,
                              theme::kInk);
            list.text(semibold, fonts.semibold_texture, "OK", kCenterX, y + 49.0f, 28,
                      theme::kTextOnDark, gfx::Align::center);
            static constexpr PanelHint kNoticeHints[] = {{Button::cross, "Close"}};
            hints(list, fonts, kNoticeHints, 1, hint_y);
            break;
        }
        centred(list, regular, fonts.regular_texture,
                "Version " + offer_.version + " is ready to install.", top + 392.0f, 26,
                theme::kInk);
        centred(list, regular, fonts.regular_texture,
                offer_.size > 0 ? "Download size: " + megabytes(offer_.size) +
                                      "  \xC2\xB7  Your games and records are kept."
                                : std::string("Your games and records are kept."),
                top + 436.0f, 22, theme::kInkMuted);
        centred(list, regular, fonts.regular_texture,
                "ProsperoPuzzles closes to finish the update.", top + 470.0f, 22, theme::kInkMuted);
        buttons("Update now", "Later");
        static constexpr PanelHint kOfferHints[] = {
            {Button::dpad, "Choose"}, {Button::cross, "Select"}, {Button::circle, "Later"}};
        hints(list, fonts, kOfferHints, 3, hint_y);
        break;
    }
    case Stage::working:
    case Stage::cancelling:
    {
        const UpdatePhase phase = progress_.phase;
        const bool measured =
            phase == UpdatePhase::downloading && progress_.total > 0 && stage_ == Stage::working;
        const float share = tween::clamp01(fraction_.value);
        if (measured)
        {
            arc(list, kCenterX, ring_y, kRingRadius, kRingWidth, -kPi * 0.5f, 2.0f * kPi * share,
                accent);
            // A bright head at the arc's end.
            const float a = -kPi * 0.5f + 2.0f * kPi * share;
            list.circle(kCenterX + kRingRadius * std::cos(a), ring_y + kRingRadius * std::sin(a),
                        kRingWidth * 0.9f, kAccentPale);
            char percent[8];
            std::snprintf(percent, sizeof(percent), "%d%%",
                          std::min(100, static_cast<int>(share * 100.0f + 0.5f)));
            list.text(semibold, fonts.semibold_texture, percent, kCenterX, ring_y + 16.0f, 46,
                      theme::kInk, gfx::Align::center);
        }
        else
        {
            // Waiting without a share: an arc that turns and breathes, and
            // three dots.
            const float turn = time_ * 4.2f;
            const float sweep = kPi * (0.55f + 0.45f * std::sin(time_ * 2.1f));
            arc(list, kCenterX, ring_y, kRingRadius, kRingWidth, turn, sweep,
                stage_ == Stage::cancelling ? kWarning : accent);
            for (int i = 0; i < 3; ++i)
            {
                const float wave =
                    0.5f + 0.5f * std::sin(time_ * 6.0f - static_cast<float>(i) * 0.9f);
                list.circle(kCenterX - 26.0f + 26.0f * static_cast<float>(i),
                            ring_y - wave * 8.0f * motion, 7.0f,
                            accent.with_alpha(0.35f + 0.6f * wave));
            }
        }

        const char *headline = stage_ == Stage::cancelling         ? "Cancelling"
                               : phase == UpdatePhase::downloading ? "Downloading"
                               : phase == UpdatePhase::unpacking   ? "Unpacking"
                                                                   : "Preparing";
        centred(list, semibold, fonts.semibold_texture, headline, top + kHeadline, 40, theme::kInk);
        centred(list, regular, fonts.regular_texture, "Version " + offer_.version, top + 386.0f, 24,
                theme::kInkMuted);

        // The bar: the share, or a light running along it while there is none.
        const gfx::Rect bar{panel.x + 96.0f, top + 428.0f, panel.w - 192.0f, 10.0f};
        list.rounded_rect(bar, 5.0f, kTrack);
        if (measured || phase == UpdatePhase::unpacking)
        {
            list.rounded_rect({bar.x, bar.y, std::max(bar.h, bar.w * share), bar.h}, 5.0f, accent);
            // A sheen sliding over the filled part.
            const float sheen = std::fmod(time_ * 0.6f, 1.0f);
            list.push_clip({bar.x, bar.y, bar.w * share, bar.h});
            list.rounded_rect({bar.x + bar.w * share * sheen - 40.0f, bar.y, 80.0f, bar.h}, 5.0f,
                              kAccentPale.with_alpha(0.6f * motion));
            list.pop_clip();
        }
        else
        {
            const float run = std::fmod(time_ * 0.8f, 1.4f) - 0.2f;
            list.push_clip(bar);
            list.rounded_rect({bar.x + bar.w * run - 90.0f, bar.y, 180.0f, bar.h}, 5.0f,
                              accent.with_alpha(0.8f));
            list.pop_clip();
        }

        // Bytes, and the time left once the speed is known.
        if (measured)
        {
            std::string line = megabytes(progress_.done) + "  /  " + megabytes(progress_.total);
            if (!progress_.time_left.empty())
                line += "  \xC2\xB7  " + capitalised(progress_.time_left);
            centred(list, regular, fonts.regular_texture, line, top + 482.0f, 22, theme::kInkMuted);
        }
        if (stage_ == Stage::working)
        {
            static constexpr PanelHint kWorkingHints[] = {{Button::circle, "Cancel"}};
            hints(list, fonts, kWorkingHints, 1, hint_y);
        }
        break;
    }
    case Stage::closing:
    {
        // The ring closes, then a tick draws itself and the badge pops.
        arc(list, kCenterX, ring_y, kRingRadius, kRingWidth, -kPi * 0.5f, 2.0f * kPi, accent);
        const float pop = tween::back_out(t / 0.5f);
        list.push_transform(0.6f + 0.4f * pop, kCenterX, ring_y, 0.0f, 0.0f);
        list.circle(kCenterX, ring_y, kRingRadius - kRingWidth - 10.0f, accent.with_alpha(0.18f));
        const float stroke = tween::cubic_out((t - 0.15f) / 0.45f);
        const float x0 = kCenterX - 34.0f;
        const float y0 = ring_y + 2.0f;
        const float x1 = kCenterX - 10.0f;
        const float y1 = ring_y + 26.0f;
        const float x2 = kCenterX + 38.0f;
        const float y2 = ring_y - 26.0f;
        const float first = tween::clamp01(stroke / 0.4f);
        const float second = tween::clamp01((stroke - 0.4f) / 0.6f);
        if (first > 0.0f)
            list.line(x0, y0, x0 + (x1 - x0) * first, y0 + (y1 - y0) * first, 10.0f, accent);
        if (second > 0.0f)
            list.line(x1, y1, x1 + (x2 - x1) * second, y1 + (y2 - y1) * second, 10.0f, accent);
        list.pop_transform();

        centred(list, semibold, fonts.semibold_texture, "Update ready", top + kHeadline, 44,
                theme::kInk);
        centred(list, regular, fonts.regular_texture, "ProsperoPuzzles closes now.", top + 392.0f,
                26, theme::kInk);
        centred(list, regular, fonts.regular_texture,
                "Open it again to play version " + offer_.version + ".", top + 432.0f, 22,
                theme::kInkMuted);
        // The time until it closes.
        const float left = 1.0f - tween::clamp01(t / kClosingSeconds);
        const float width = (panel.w - 192.0f) * left;
        list.rounded_rect({kCenterX - width * 0.5f, top + 478.0f, width, 6.0f}, 3.0f,
                          accent.with_alpha(0.75f));
        break;
    }
    case Stage::failed:
    {
        arc(list, kCenterX, ring_y, kRingRadius, kRingWidth, -kPi * 0.5f,
            2.0f * kPi * tween::cubic_out(t / 0.6f), accent);
        // A short shake as it arrives, and an exclamation mark.
        const float shake =
            std::sin(t * 38.0f) * 10.0f * (1.0f - tween::clamp01(t / 0.45f)) * motion;
        list.line(kCenterX + shake, ring_y - 40.0f, kCenterX + shake, ring_y + 12.0f, 11.0f,
                  accent);
        list.circle(kCenterX + shake, ring_y + 38.0f, 8.0f, accent);

        centred(list, semibold, fonts.semibold_texture, "The update could not finish",
                top + kHeadline, 40, theme::kInk);
        centred(list, regular, fonts.regular_texture, "ProsperoPuzzles was not changed.",
                top + 392.0f, 26, theme::kInk);
        if (!error_.empty())
            centred(list, regular, fonts.regular_texture, error_, top + 436.0f, 22,
                    theme::kInkMuted);
        buttons("Try again", "Close");
        static constexpr PanelHint kFailedHints[] = {
            {Button::dpad, "Choose"}, {Button::cross, "Select"}, {Button::circle, "Close"}};
        hints(list, fonts, kFailedHints, 3, hint_y);
        break;
    }
    }
    list.pop_opacity();
}

} // namespace ppz::ui
