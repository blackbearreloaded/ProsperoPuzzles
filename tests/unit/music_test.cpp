// ProsperoPuzzles - Music streaming tests: Vorbis decode, loops, decks and mixing.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio/mixer.hpp"
#include "audio/music.hpp"
#include "audio/stream_ring.hpp"
#include "core/save_file.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

#ifndef PPZ_SOURCE_DIR
#define PPZ_SOURCE_DIR "."
#endif

using namespace ppz::audio;

namespace
{

const std::string kMusic = std::string(PPZ_SOURCE_DIR) + "/tests/fixtures/music";

std::string fixture(const char *name)
{
    std::string data;
    EXPECT_TRUE(ppz::save::read_file(kMusic + "/" + name, &data)) << name;
    return data;
}

double peak(const std::vector<std::int16_t> &samples)
{
    double best = 0.0;
    for (std::int16_t s : samples)
        best = std::max(best, std::fabs(static_cast<double>(s) / 32767.0));
    return best;
}

} // namespace

TEST(StreamRing, WrapsAndReportsShortReads)
{
    StreamRing ring(8);
    float in[20];
    for (int i = 0; i < 20; ++i)
        in[i] = static_cast<float>(i);
    EXPECT_EQ(ring.write(in, 10), 8u); // only 8 frames fit
    float out[20] = {};
    EXPECT_EQ(ring.read(out, 5), 5u);
    EXPECT_EQ(out[9], 9.0f);
    EXPECT_EQ(ring.write(in, 4), 4u); // wraps around the end
    EXPECT_EQ(ring.available(), 7u);
    EXPECT_EQ(ring.read(out, 20), 7u);
    EXPECT_EQ(out[5], 15.0f); // frames 5..7 of the first write
    EXPECT_EQ(out[6], 0.0f);  // then frames 0..3 of the second
    EXPECT_EQ(out[13], 7.0f);
}

TEST(MusicTrack, ReadsLoopCommentsAndLoopsForever)
{
    MusicTrack track;
    ASSERT_EQ(track.open(fixture("puzzle_calm_02.ogg")), "");
    EXPECT_EQ(track.loop_start(), 12000u);
    EXPECT_EQ(track.loop_end(), 36000u);
    // Five seconds from a one-second file: the loop keeps it going.
    std::vector<float> out(48000 * 2);
    for (int second = 0; second < 5; ++second)
        EXPECT_EQ(track.decode(out.data(), 48000), 48000);
    float loudest = 0.0f;
    for (float s : out)
        loudest = std::max(loudest, std::fabs(s));
    EXPECT_GT(loudest, 0.08f); // ffmpeg's test sine peaks at 1/8
}

TEST(MusicTrack, DuplicatesMonoAndRejectsOtherRates)
{
    MusicTrack mono;
    ASSERT_EQ(mono.open(fixture("menu_main.ogg")), "");
    std::vector<float> out(4800 * 2);
    ASSERT_EQ(mono.decode(out.data(), 4800), 4800);
    EXPECT_FLOAT_EQ(out[2000], out[2001]);
    MusicTrack wrong;
    EXPECT_NE(wrong.open(fixture("wrong_rate.ogg")).find("48000"), std::string::npos);
    MusicTrack junk;
    EXPECT_NE(junk.open("not an ogg file"), "");
}

TEST(Mixer, StreamsPlayOnTheMusicBusAndRampGain)
{
    Mixer mixer;
    StreamRing ring(4096);
    mixer.attach_stream(0, &ring);
    std::vector<float> constant(2048 * 2, 0.5f);
    ring.write(constant.data(), 2048);
    std::vector<std::int16_t> out(256 * 2);
    mixer.render(out.data(), 256);
    EXPECT_EQ(peak(out), 0.0); // gain starts at zero
    mixer.set_stream_gain(0, 1.0f, 0.0f);
    mixer.render(out.data(), 256);
    EXPECT_NEAR(peak(out), 0.5, 0.02);
    mixer.set_bus_gain(Bus::music, 0.0f);
    for (int i = 0; i < 12; ++i) // past the 2048 buffered frames
        mixer.render(out.data(), 256);
    EXPECT_LT(peak(out), 0.05);              // the music bus fader applies to streams
    EXPECT_GT(mixer.stream_underruns(), 0u); // the ring ran dry along the way
}

TEST(MusicPlayer, ChoosesTracksByContextAndCrossfades)
{
    Mixer mixer;
    MusicPlayer player;
    EXPECT_EQ(player.init(mixer, kMusic), 2); // menu_main and puzzle_calm_02
    player.set_context("");
    EXPECT_EQ(player.current(), "menu_main");
    player.set_context("net");
    EXPECT_EQ(player.current(), "game_net"); // its own track wins
    player.set_context("solo");
    EXPECT_EQ(player.current(), "puzzle_calm_02");
    player.set_context("mines"); // upbeat list is empty: falls back to calm
    EXPECT_EQ(player.current(), "puzzle_calm_02");
    EXPECT_TRUE(MusicPlayer::upbeat("g2048"));
    EXPECT_FALSE(MusicPlayer::upbeat("solo"));

    // Two seconds of audio with the decoder pumped each "frame": music plays.
    std::vector<std::int16_t> out(800 * 2);
    double loudest = 0.0;
    for (int frame = 0; frame < 120; ++frame)
    {
        player.pump(1.0f / 60.0f);
        mixer.render(out.data(), 800);
        loudest = std::max(loudest, peak(out));
    }
    EXPECT_GT(loudest, 0.05);
    player.duck();
    player.pump(0.5f);
}

TEST(MusicPlayer, SilentWithoutTracks)
{
    Mixer mixer;
    MusicPlayer player;
    EXPECT_EQ(player.init(mixer, "/nonexistent"), 0);
    player.set_context("");
    player.set_context("solo");
    EXPECT_EQ(player.current(), "");
    player.pump(0.016f);
}
