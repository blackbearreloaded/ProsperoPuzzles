// ProsperoPuzzles - Streamed OGG Vorbis music with crossfades and ducking.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio/music.hpp"

#include "core/save_file.hpp"

#include "third_party/stb/vorbis.h"

#include <sys/stat.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ppz::audio
{

namespace
{

constexpr float kCrossfadeSeconds = 1.5f;
constexpr float kDuckSeconds = 2.5f;
constexpr float kDuckGain = 0.5f;     // -6 dB
constexpr std::size_t kAhead = 32768; // frames kept buffered (~0.7 s)
constexpr int kChunk = 2048;          // frames decoded per step
constexpr int kPlaylist = 9;          // puzzle_calm_01 .. _09 and the like

bool comment_value(const stb_vorbis_comment &comments, const char *key, unsigned *value)
{
    const std::size_t length = std::strlen(key);
    for (int i = 0; i < comments.comment_list_length; ++i)
    {
        const char *entry = comments.comment_list[i];
        if (std::strncmp(entry, key, length) == 0 && entry[length] == '=')
        {
            *value = static_cast<unsigned>(std::strtoul(entry + length + 1, nullptr, 10));
            return true;
        }
    }
    return false;
}

bool file_exists(const std::string &path)
{
    struct stat info
    {
    };
    return ::stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
}

std::string numbered(const char *stem, int n)
{
    char name[48];
    std::snprintf(name, sizeof(name), "%s_%02d", stem, n);
    return name;
}

} // namespace

MusicTrack::~MusicTrack()
{
    if (vorbis_ != nullptr)
        stb_vorbis_close(vorbis_);
}

std::string MusicTrack::open(std::string data)
{
    data_ = std::move(data);
    int error = 0;
    vorbis_ = stb_vorbis_open_memory(reinterpret_cast<const unsigned char *>(data_.data()),
                                     static_cast<int>(data_.size()), &error, nullptr);
    if (vorbis_ == nullptr)
        return "not a Vorbis stream (error " + std::to_string(error) + ")";
    const stb_vorbis_info info = stb_vorbis_get_info(vorbis_);
    if (info.sample_rate != static_cast<unsigned>(kSampleRate))
        return "sample rate " + std::to_string(info.sample_rate) + " (need 48000)";
    if (info.channels < 1 || info.channels > 2)
        return std::to_string(info.channels) + " channels (need 1 or 2)";
    channels_ = info.channels;
    const unsigned total = stb_vorbis_stream_length_in_samples(vorbis_);
    if (total == 0)
        return "empty stream";
    loop_start_ = 0;
    loop_end_ = total;
    unsigned start = 0;
    unsigned length = 0;
    const stb_vorbis_comment comments = stb_vorbis_get_comment(vorbis_);
    if (comment_value(comments, "LOOPSTART", &start) && start < total)
    {
        loop_start_ = start;
        if (comment_value(comments, "LOOPLENGTH", &length) && length > 0)
            loop_end_ = std::min(total, start + length);
    }
    return {};
}

int MusicTrack::decode(float *out, int frames)
{
    int done = 0;
    int stalls = 0;
    while (done < frames)
    {
        if (position_ >= loop_end_)
        {
            if (stb_vorbis_seek(vorbis_, loop_start_) == 0)
                return done;
            position_ = loop_start_;
        }
        const int want = static_cast<int>(
            std::min<unsigned>(static_cast<unsigned>(frames - done), loop_end_ - position_));
        scratch_.resize(static_cast<std::size_t>(want) * 2);
        const int got = stb_vorbis_get_samples_float_interleaved(vorbis_, channels_,
                                                                 scratch_.data(), want * channels_);
        if (channels_ == 1)
        {
            // Spread mono to both sides, back to front so nothing is overwritten.
            for (int k = got - 1; k >= 0; --k)
            {
                const float s = scratch_[static_cast<std::size_t>(k)];
                scratch_[static_cast<std::size_t>(k) * 2] = s;
                scratch_[static_cast<std::size_t>(k) * 2 + 1] = s;
            }
        }
        if (got <= 0)
        {
            // Past the real end (a LOOPLENGTH beyond the data): wrap now.
            if (++stalls > 2)
                return done;
            position_ = loop_end_;
            continue;
        }
        stalls = 0;
        std::memcpy(out + static_cast<std::size_t>(done) * 2, scratch_.data(),
                    static_cast<std::size_t>(got) * 2 * sizeof(float));
        done += got;
        position_ += static_cast<unsigned>(got);
    }
    return done;
}

MusicPlayer::MusicPlayer() : buffer_(static_cast<std::size_t>(kChunk) * 2)
{
}

int MusicPlayer::init(Mixer &mixer, const std::string &directory)
{
    mixer_ = &mixer;
    directory_ = directory;
    for (std::size_t slot = 0; slot < decks_.size(); ++slot)
    {
        mixer.attach_stream(slot, decks_[slot].ring.get());
        mixer.set_stream_gain(slot, 0.0f, 0.0f);
    }
    std::vector<std::string> candidates = {"menu_main"};
    for (int n = 1; n <= kPlaylist; ++n)
    {
        candidates.push_back(numbered("puzzle_calm", n));
        candidates.push_back(numbered("puzzle_upbeat", n));
    }
    for (const std::string &name : candidates)
    {
        if (file_exists(directory_ + "/" + name + ".ogg"))
            available_.push_back(name);
    }
    return static_cast<int>(available_.size());
}

bool MusicPlayer::has(const std::string &name) const
{
    return std::find(available_.begin(), available_.end(), name) != available_.end();
}

bool MusicPlayer::upbeat(std::string_view game_id)
{
    constexpr std::string_view kUpbeat[] = {"g2048", "tenfold", "samegame",
                                            "flood", "inertia", "mines"};
    return std::find(std::begin(kUpbeat), std::end(kUpbeat), game_id) != std::end(kUpbeat);
}

std::string MusicPlayer::choose(std::string_view game_id)
{
    if (game_id.empty())
        return has("menu_main") ? "menu_main" : std::string();
    const std::string own = "game_" + std::string(game_id);
    if (has(own) || file_exists(directory_ + "/" + own + ".ogg"))
        return own;
    // Rotate through a playlist; fall back to the other one if it is empty.
    for (int pass = 0; pass < 2; ++pass)
    {
        const bool lively = upbeat(game_id) != (pass == 1);
        const char *stem = lively ? "puzzle_upbeat" : "puzzle_calm";
        int &turn = lively ? upbeat_turn_ : calm_turn_;
        for (int tries = 0; tries < kPlaylist; ++tries)
        {
            const std::string name = numbered(stem, turn % kPlaylist + 1);
            turn = (turn + 1) % kPlaylist;
            if (has(name))
                return name;
        }
    }
    return has("menu_main") ? "menu_main" : std::string();
}

void MusicPlayer::set_context(std::string_view game_id)
{
    if (mixer_ == nullptr || game_id == context_)
        return;
    context_ = std::string(game_id);
    play(choose(game_id));
}

void MusicPlayer::play(const std::string &name)
{
    if (name == current_)
        return;
    current_ = name;
    if (active_ >= 0)
        decks_[static_cast<std::size_t>(active_)].release = kCrossfadeSeconds + 0.1f;
    if (name.empty())
    {
        active_ = -1;
        apply_gains(kCrossfadeSeconds);
        return;
    }
    // Use the deck that is not playing; it may still hold a fading track.
    const int slot = active_ == 0 ? 1 : 0;
    Deck &deck = decks_[static_cast<std::size_t>(slot)];
    std::string data;
    auto track = std::make_unique<MusicTrack>();
    std::string error = "unreadable";
    if (save::read_file(directory_ + "/" + name + ".ogg", &data))
        error = track->open(std::move(data));
    if (!error.empty())
    {
        std::fprintf(stderr, "[PPZ] music %s rejected: %s\n", name.c_str(), error.c_str());
        active_ = -1;
        apply_gains(kCrossfadeSeconds);
        return;
    }
    // A fresh ring: whatever the old track left buffered never plays.
    deck.retired = std::move(deck.ring);
    deck.retire = 0.5f;
    deck.ring = std::make_unique<StreamRing>(1u << 16);
    mixer_->swap_stream(static_cast<std::size_t>(slot), deck.ring.get());
    deck.track = std::move(track);
    deck.name = name;
    deck.release = -1.0f;
    active_ = slot;
    pump(0.0f); // prefill before the fade starts
    apply_gains(kCrossfadeSeconds);
}

void MusicPlayer::duck()
{
    duck_ = kDuckSeconds;
    apply_gains(0.15f);
}

void MusicPlayer::apply_gains(float seconds)
{
    if (mixer_ == nullptr)
        return;
    for (std::size_t slot = 0; slot < decks_.size(); ++slot)
    {
        const bool playing = static_cast<int>(slot) == active_;
        const float gain = playing ? (duck_ > 0.0f ? kDuckGain : 1.0f) : 0.0f;
        mixer_->set_stream_gain(slot, gain, seconds);
    }
}

void MusicPlayer::pump(float dt)
{
    if (duck_ > 0.0f)
    {
        duck_ -= dt;
        if (duck_ <= 0.0f)
            apply_gains(0.8f); // swell back
    }
    for (Deck &deck : decks_)
    {
        if (deck.retire >= 0.0f)
        {
            deck.retire -= dt;
            if (deck.retire < 0.0f)
                deck.retired.reset();
        }
        if (deck.release >= 0.0f)
        {
            deck.release -= dt;
            if (deck.release < 0.0f)
            {
                // Faded out: stop decoding. The ring drains into silence.
                deck.track.reset();
                deck.name.clear();
                deck.release = -1.0f;
                continue;
            }
        }
        if (!deck.track)
            continue;
        while (deck.ring->available() < kAhead)
        {
            const int frames = static_cast<int>(std::min<std::size_t>(kChunk, deck.ring->space()));
            if (frames <= 0)
                break;
            const int got = deck.track->decode(buffer_.data(), frames);
            if (got <= 0)
            {
                deck.track.reset(); // broken stream: give up on it
                break;
            }
            deck.ring->write(buffer_.data(), static_cast<std::size_t>(got));
        }
    }
}

} // namespace ppz::audio
