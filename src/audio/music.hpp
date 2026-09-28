// ProsperoPuzzles - Streamed OGG Vorbis music with crossfades and ducking.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "audio/mixer.hpp"
#include "audio/stream_ring.hpp"

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct stb_vorbis;

namespace ppz::audio
{

// One Vorbis file decoded from memory, looping forever. Loop points come from
// the LOOPSTART / LOOPLENGTH comments (in samples); without them the whole
// file loops.
class MusicTrack
{
  public:
    ~MusicTrack();

    // Takes the whole file; returns an error, or "" on success. 48 kHz only.
    std::string open(std::string data);
    // Decodes stereo frames into out (mono is duplicated). Always fills frames
    // unless the stream is broken, in which case it returns fewer.
    int decode(float *out, int frames);

    unsigned loop_start() const
    {
        return loop_start_;
    }
    unsigned loop_end() const
    {
        return loop_end_;
    }

  private:
    std::string data_;
    stb_vorbis *vorbis_ = nullptr;
    int channels_ = 0;
    unsigned position_ = 0; // sample frames decoded so far
    unsigned loop_start_ = 0;
    unsigned loop_end_ = 0; // exclusive
    std::vector<float> scratch_;
};

// Picks and plays the soundtrack (PLAN.md Appendix A): menu_main in the
// library, game_<id> when a game has its own track, otherwise the upbeat or
// calm playlist in rotation. Two decks crossfade over 1.5 s; completion stings
// duck the music by 6 dB. Decoding runs on the caller's thread (pump, once
// per frame) and keeps each deck about 0.7 s ahead of the audio thread.
class MusicPlayer
{
  public:
    MusicPlayer();

    // Finds the tracks present in directory and attaches both decks to the
    // mixer (call before the audio thread starts). Returns how many were found.
    int init(Mixer &mixer, const std::string &directory);

    // "" for the library; otherwise the game being played.
    void set_context(std::string_view game_id);
    // Lowers the music under a sting for a moment.
    void duck();
    void pump(float dt);

    const std::string &current() const
    {
        return current_;
    }
    int tracks() const
    {
        return static_cast<int>(available_.size());
    }

    // Track names (without .ogg) that belong to a context; public for tests.
    static bool upbeat(std::string_view game_id);

  private:
    struct Deck
    {
        std::unique_ptr<StreamRing> ring = std::make_unique<StreamRing>(1u << 16);
        std::unique_ptr<StreamRing> retired; // swapped out, freed after a moment
        float retire = -1.0f;
        std::unique_ptr<MusicTrack> track;
        std::string name;
        float release = -1.0f; // seconds until the faded-out track is freed
    };

    bool has(const std::string &name) const;
    std::string choose(std::string_view game_id);
    void play(const std::string &name);
    void apply_gains(float seconds);

    Mixer *mixer_ = nullptr;
    std::string directory_;
    std::vector<std::string> available_;
    std::array<Deck, Mixer::kStreams> decks_;
    int active_ = -1;
    std::string context_ = "\x01"; // nothing chosen yet
    std::string current_;
    int calm_turn_ = 0;
    int upbeat_turn_ = 0;
    float duck_ = 0.0f; // seconds of ducking left
    std::vector<float> buffer_;
};

} // namespace ppz::audio
