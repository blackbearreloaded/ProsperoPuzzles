// ProsperoPuzzles - Sound cues: names, placeholders and the loaded sound bank.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "audio/mixer.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace ppz::audio
{

// Every cue in PLAN.md Appendix A. File names use cue_name(): "<cue>_NN.wav"
// for the shared sound and "<game>.<cue>_NN.wav" for a per-game override.
enum class Cue : std::uint8_t
{
    ui_focus,
    ui_select,
    ui_back,
    ui_tab,
    ui_favorite_on,
    ui_favorite_off,
    ui_launch,
    ui_pause_open,
    ui_pause_close,
    ui_toggle,
    ui_slider,
    ui_error,
    ui_notify,
    cursor,
    place,
    mark,
    erase,
    digit,
    rotate,
    slide,
    flip,
    pickup,
    drop,
    connect,
    reveal,
    cascade,
    merge,
    spawn,
    invalid,
    undo,
    redo,
    new_game,
    restart,
    solve_reveal,
    complete,
    new_record,
    explode,
    game_over,
    count,
};

constexpr std::size_t kCueCount = static_cast<std::size_t>(Cue::count);

const char *cue_name(Cue cue);
bool cue_from_name(std::string_view name, Cue *cue);
Bus cue_bus(Cue cue);
// Synthesized stand-in used until a recorded sound is supplied.
Tone placeholder_tone(Cue cue);

// Recorded sounds found in a directory, with round-robin variations and a
// small pitch jitter. Missing cues fall back to placeholder tones.
class SoundBank
{
  public:
    struct Stats
    {
        int files = 0;
        int rejected = 0;
        std::vector<std::string> errors;
    };

    // Scans directory for "<cue>_NN.wav" and "<game>.<cue>_NN.wav".
    Stats load(const std::string &directory);
    // Adds one decoded sound (used by load and by tests).
    void add(const std::string &game, Cue cue, std::vector<float> samples, std::size_t frames);

    // Posts the cue to the mixer; game may be empty. pitch scales playback
    // (for example merges rising with value).
    void play(Mixer &mixer, Cue cue, std::string_view game = {}, float pitch = 1.0f,
              float gain = 1.0f);

    bool has_recording(Cue cue, std::string_view game = {}) const;

  private:
    struct Sound
    {
        std::vector<float> samples;
        Clip clip;
    };
    struct Variations
    {
        std::string game; // empty for the shared set
        Cue cue = Cue::count;
        std::vector<std::unique_ptr<Sound>> sounds;
        std::size_t next = 0;
    };

    Variations *find(Cue cue, std::string_view game);

    std::vector<Variations> sets_;
    std::uint32_t jitter_state_ = 0x2545f491u;
};

} // namespace ppz::audio
