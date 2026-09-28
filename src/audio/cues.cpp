// ProsperoPuzzles - Sound cues: names, placeholders and the loaded sound bank.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio/cues.hpp"

#include "audio/wav.hpp"
#include "core/save_file.hpp"

#include <algorithm>
#include <cctype>

namespace ppz::audio
{

namespace
{

struct CueInfo
{
    const char *name;
    Bus bus;
    Wave wave;
    float start_hz;
    float end_hz;
    float seconds;
    float gain;
};

// Placeholder voices echo the WIP 2048 and Tenfold synths where they had one.
constexpr CueInfo kCues[kCueCount] = {
    {"ui_focus", Bus::ui, Wave::sine, 660, 700, 0.035f, 0.25f},
    {"ui_select", Bus::ui, Wave::triangle, 740, 880, 0.08f, 0.4f},
    {"ui_back", Bus::ui, Wave::triangle, 520, 420, 0.08f, 0.35f},
    {"ui_tab", Bus::ui, Wave::sine, 560, 640, 0.06f, 0.3f},
    {"ui_favorite_on", Bus::ui, Wave::sine, 880, 1320, 0.22f, 0.35f},
    {"ui_favorite_off", Bus::ui, Wave::sine, 660, 440, 0.12f, 0.3f},
    {"ui_launch", Bus::ui, Wave::sine, 300, 900, 0.35f, 0.35f},
    {"ui_pause_open", Bus::ui, Wave::triangle, 440, 560, 0.12f, 0.3f},
    {"ui_pause_close", Bus::ui, Wave::triangle, 560, 440, 0.12f, 0.3f},
    {"ui_toggle", Bus::ui, Wave::triangle, 700, 700, 0.05f, 0.3f},
    {"ui_slider", Bus::ui, Wave::sine, 900, 900, 0.03f, 0.2f},
    {"ui_error", Bus::ui, Wave::soft_square, 200, 160, 0.14f, 0.3f},
    {"ui_notify", Bus::ui, Wave::sine, 660, 990, 0.3f, 0.35f},
    {"cursor", Bus::sfx, Wave::sine, 360, 380, 0.03f, 0.18f},
    {"place", Bus::sfx, Wave::triangle, 590, 620, 0.07f, 0.4f},
    {"mark", Bus::sfx, Wave::triangle, 420, 400, 0.07f, 0.35f},
    {"erase", Bus::sfx, Wave::sine, 500, 300, 0.07f, 0.3f},
    {"digit", Bus::sfx, Wave::triangle, 640, 660, 0.06f, 0.35f},
    {"rotate", Bus::sfx, Wave::triangle, 330, 400, 0.09f, 0.35f},
    {"slide", Bus::sfx, Wave::triangle, 360, 300, 0.08f, 0.35f},
    {"flip", Bus::sfx, Wave::sine, 520, 780, 0.07f, 0.35f},
    {"pickup", Bus::sfx, Wave::sine, 480, 620, 0.06f, 0.3f},
    {"drop", Bus::sfx, Wave::sine, 620, 440, 0.07f, 0.3f},
    {"connect", Bus::sfx, Wave::sine, 660, 990, 0.16f, 0.35f},
    {"reveal", Bus::sfx, Wave::triangle, 700, 760, 0.05f, 0.3f},
    {"cascade", Bus::sfx, Wave::sine, 500, 1100, 0.3f, 0.35f},
    {"merge", Bus::sfx, Wave::triangle, 460, 520, 0.115f, 0.45f},
    {"spawn", Bus::sfx, Wave::sine, 900, 1000, 0.06f, 0.25f},
    {"invalid", Bus::sfx, Wave::soft_square, 220, 200, 0.12f, 0.3f},
    {"undo", Bus::sfx, Wave::sine, 300, 260, 0.08f, 0.3f},
    {"redo", Bus::sfx, Wave::sine, 260, 300, 0.08f, 0.3f},
    {"new_game", Bus::sfx, Wave::sine, 440, 880, 0.3f, 0.35f},
    {"restart", Bus::sfx, Wave::sine, 600, 400, 0.25f, 0.3f},
    {"solve_reveal", Bus::sfx, Wave::sine, 523, 1046, 0.6f, 0.3f},
    {"complete", Bus::sfx, Wave::triangle, 523, 1046, 0.9f, 0.45f},
    {"new_record", Bus::sfx, Wave::sine, 784, 1568, 0.7f, 0.4f},
    {"explode", Bus::sfx, Wave::soft_square, 120, 40, 0.9f, 0.5f},
    {"game_over", Bus::sfx, Wave::triangle, 300, 150, 0.8f, 0.4f},
};

const CueInfo &info(Cue cue)
{
    return kCues[static_cast<std::size_t>(cue)];
}

// Splits "<game>.<cue>_NN.wav" or "<cue>_NN.wav" (the _NN suffix is optional).
bool parse_file_name(std::string_view file, std::string *game, Cue *cue)
{
    constexpr std::string_view kSuffix = ".wav";
    if (file.size() <= kSuffix.size() || file.substr(file.size() - kSuffix.size()) != kSuffix)
        return false;
    std::string_view stem = file.substr(0, file.size() - kSuffix.size());
    game->clear();
    if (const auto dot = stem.find('.'); dot != std::string_view::npos)
    {
        game->assign(stem.substr(0, dot));
        stem = stem.substr(dot + 1);
    }
    if (const auto underscore = stem.rfind('_'); underscore != std::string_view::npos)
    {
        const std::string_view digits = stem.substr(underscore + 1);
        const bool numeric =
            !digits.empty() &&
            std::all_of(digits.begin(), digits.end(),
                        [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; });
        if (numeric)
            stem = stem.substr(0, underscore);
    }
    return cue_from_name(stem, cue);
}

} // namespace

const char *cue_name(Cue cue)
{
    return cue < Cue::count ? info(cue).name : "";
}

bool cue_from_name(std::string_view name, Cue *cue)
{
    for (std::size_t index = 0; index < kCueCount; ++index)
    {
        if (name == kCues[index].name)
        {
            *cue = static_cast<Cue>(index);
            return true;
        }
    }
    return false;
}

Bus cue_bus(Cue cue)
{
    return info(cue).bus;
}

Tone placeholder_tone(Cue cue)
{
    const CueInfo &cue_info = info(cue);
    Tone tone;
    tone.wave = cue_info.wave;
    tone.start_hz = cue_info.start_hz;
    tone.end_hz = cue_info.end_hz;
    tone.seconds = cue_info.seconds;
    tone.attack = std::min(0.006f, cue_info.seconds * 0.2f);
    tone.release = std::min(0.12f, cue_info.seconds * 0.5f);
    return tone;
}

void SoundBank::add(const std::string &game, Cue cue, std::vector<float> samples,
                    std::size_t frames)
{
    Variations *set = find(cue, game);
    if (set == nullptr || set->game != game)
    {
        sets_.push_back(Variations{});
        set = &sets_.back();
        set->game = game;
        set->cue = cue;
    }
    auto sound = std::make_unique<Sound>();
    sound->samples = std::move(samples);
    sound->clip.samples = sound->samples.data();
    sound->clip.frames = frames;
    set->sounds.push_back(std::move(sound));
}

SoundBank::Stats SoundBank::load(const std::string &directory)
{
    Stats stats;
    std::vector<std::string> names = save::list_files(directory);
    // Sorted so variation order (_01, _02, ...) is deterministic.
    std::sort(names.begin(), names.end());
    for (const std::string &name : names)
    {
        std::string game;
        Cue cue = Cue::count;
        if (!parse_file_name(name, &game, &cue))
            continue;
        std::string data;
        if (!save::read_file(directory + "/" + name, &data, 16u << 20))
        {
            ++stats.rejected;
            stats.errors.push_back(name + ": unreadable");
            continue;
        }
        DecodedWav wav = decode_wav(data);
        if (!wav.ok())
        {
            ++stats.rejected;
            stats.errors.push_back(name + ": " + wav.error);
            continue;
        }
        add(game, cue, std::move(wav.samples), wav.frames);
        ++stats.files;
    }
    return stats;
}

SoundBank::Variations *SoundBank::find(Cue cue, std::string_view game)
{
    Variations *shared = nullptr;
    for (Variations &set : sets_)
    {
        if (set.cue != cue)
            continue;
        if (!game.empty() && set.game == game)
            return &set;
        if (set.game.empty())
            shared = &set;
    }
    return shared;
}

bool SoundBank::has_recording(Cue cue, std::string_view game) const
{
    for (const Variations &set : sets_)
    {
        if (set.cue == cue && (set.game.empty() || set.game == game) && !set.sounds.empty())
            return true;
    }
    return false;
}

void SoundBank::play(Mixer &mixer, Cue cue, std::string_view game, float pitch, float gain)
{
    if (cue >= Cue::count)
        return;
    PlayParams params;
    params.bus = cue_bus(cue);
    params.pitch = pitch;
    Variations *set = find(cue, game);
    if (set != nullptr && !set->sounds.empty())
    {
        // +-3 % pitch jitter keeps repeated sounds from feeling mechanical.
        jitter_state_ = jitter_state_ * 1664525u + 1013904223u;
        const float jitter = 0.97f + 0.06f * static_cast<float>(jitter_state_ >> 8) / 16777216.0f;
        params.pitch = pitch * jitter;
        params.gain = gain;
        const Sound &sound = *set->sounds[set->next % set->sounds.size()];
        set->next = (set->next + 1) % set->sounds.size();
        mixer.play_clip(&sound.clip, params);
        return;
    }
    params.gain = gain * info(cue).gain;
    mixer.play_tone(placeholder_tone(cue), params);
}

} // namespace ppz::audio
