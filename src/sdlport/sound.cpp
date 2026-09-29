/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 2001 Anthony Kruize <trandor@labyrinth.net.au>
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *  Copyright (c) 2016 Antonio Radojkovic <antonior.software@gmail.com>
 *  Copyright (c) 2024 Andrej Pancik
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 */

#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include "sound.h"
#include "music.h"
#include "specs.h"
#include "setup.h"

// Global settings object (defined setup.cpp)
extern Settings settings;

thread_local bool ScopedSoundMute::muted = false;

namespace
{
constexpr int SFX_TRACK_COUNT = 50;
MIX_Mixer *mixer = nullptr;
std::vector<MIX_Track *> sfx_tracks;
}

std::filesystem::path resolve_audio_path(const char *filename)
{
    const std::filesystem::path filename_path(filename);
    if (filename_path.is_absolute())
        return filename_path;

    const std::filesystem::path data_path = std::filesystem::path(get_filename_prefix()) / filename_path;
    if (std::filesystem::is_regular_file(data_path))
        return data_path;

    const std::filesystem::path generated_path = std::filesystem::path(ABUSE_GENERATED_ASSETDIR) / filename_path;
    if (std::filesystem::is_regular_file(generated_path))
        return generated_path;

    return data_path;
}

namespace
{
MIX_Audio *load_prefixed_audio(const char *filename)
{
    if (!filename)
        return nullptr;

#ifdef __DJGPP__
    // DJGPP stat/fstat calls are expensive under DOSBox. Avoid both the path
    // probe and SDL_IOFromFile's directory check: open the effect directly and
    // decode it from memory. MIX_LoadAudio_IO predecodes its own copy before
    // the temporary file buffer goes away.
    const std::filesystem::path requested(filename);
    std::filesystem::path path = requested.is_absolute()
                                     ? requested
                                     : std::filesystem::path(get_filename_prefix()) / requested;
    std::unique_ptr<FILE, decltype(&std::fclose)> file(std::fopen(path.string().c_str(), "rb"), &std::fclose);
    if (!file && !requested.is_absolute())
    {
        path = std::filesystem::path(ABUSE_GENERATED_ASSETDIR) / requested;
        file.reset(std::fopen(path.string().c_str(), "rb"));
    }
    if (!file)
    {
        SDL_SetError("Unable to open sound %s: %s", filename, std::strerror(errno));
        return nullptr;
    }
    if (std::fseek(file.get(), 0, SEEK_END) != 0)
    {
        SDL_SetError("Unable to seek sound %s", filename);
        return nullptr;
    }
    const long length = std::ftell(file.get());
    if (length <= 0 || std::fseek(file.get(), 0, SEEK_SET) != 0)
    {
        SDL_SetError("Invalid sound size: %s", filename);
        return nullptr;
    }
    const size_t size = static_cast<size_t>(length);
    std::unique_ptr<void, decltype(&SDL_free)> data(SDL_malloc(size), &SDL_free);
    if (!data)
    {
        SDL_OutOfMemory();
        return nullptr;
    }
    if (std::fread(data.get(), 1, size, file.get()) != size)
    {
        SDL_SetError("Unable to read sound %s", filename);
        return nullptr;
    }
    file.reset();
    return MIX_LoadAudio_IO(mixer, SDL_IOFromConstMem(data.get(), size), true, true);
#else
    const std::filesystem::path audio_path = resolve_audio_path(filename);
    return MIX_LoadAudio(mixer, audio_path.string().c_str(), true);
#endif
}

}

bool sound_is_initialized()
{
    return mixer != nullptr;
}

/**
  * @brief Initializes the sound system
  *
  * This function performs the following steps:
  * 1. Verifies the existence of the sfx directory
  * 2. Initializes SDL_mixer with standard audio parameters
  * 3. Loads custom soundfonts if specified in settings
  * 4. Allocates sound-effect tracks
  *
  * @return true when the mixer is ready
  */
bool sound_init()
{
    if (sound_is_initialized())
        return true;

    // Get the path to the game's data directory and sfx subdirectory
    const std::filesystem::path datadir = get_filename_prefix();
    const std::filesystem::path sfx_path = datadir / "sfx";

    // Verify sfx directory exists
    if (!std::filesystem::exists(sfx_path))
    {
        printf("Sound: Disabled (couldn't find the sfx directory %s)\n", sfx_path.string().c_str());
        return false;
    }

    if (!MIX_Init())
    {
        printf("Sound: Unable to initialize SDL_mixer - %s\nSound: Disabled (error)\n", SDL_GetError());
        return false;
    }

    SDL_AudioSpec requested_spec{};
#ifdef __DJGPP__
    requested_spec.freq = 22050;
#else
    requested_spec.freq = 44100;
#endif
    requested_spec.format = SDL_AUDIO_S16;
    requested_spec.channels = settings.mono ? 1 : 2;
    mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &requested_spec);
    if (!mixer)
    {
        MIX_Quit();
        printf("Sound: Unable to open audio - %s\nSound: Disabled (error)\n", SDL_GetError());
        return false;
    }

    if (!music_init(mixer))
        printf("Sound: Music initialization failed: %s\n", SDL_GetError());

    sfx_tracks.reserve(SFX_TRACK_COUNT);
    for (int i = 0; i < SFX_TRACK_COUNT; ++i)
    {
        MIX_Track *track = MIX_CreateTrack(mixer);
        if (!track)
        {
            printf("Sound: Could only create %zu sound-effect tracks: %s\n", sfx_tracks.size(), SDL_GetError());
            break;
        }
        sfx_tracks.push_back(track);
    }

    return true;
}

/**
  * @brief Shuts down the sound system
  *
  * Closes the audio device and marks the system as disabled.
  * Safe to call even if sound system wasn't initialized.
  */
void sound_uninit()
{
    if (!sound_is_initialized())
        return;

    MIX_DestroyMixer(mixer);
    music_uninit();
    mixer = nullptr;
    sfx_tracks.clear();
    MIX_Quit();
}

/**
  * @brief Constructor for sound effect objects
  *
  * Loads a sound effect from a file and prepares it for playback.
  * Uses SDL_mixer's file loader and predecodes the sound for repeated playback.
  *
  * @param filename Path to the sound effect file
  */
sound_effect::sound_effect(char const *filename) : m_audio(nullptr)
{
    if (!sound_is_initialized())
        return;

    m_audio = load_prefixed_audio(filename);
    if (!m_audio)
    {
        printf("Failed to load sound from file %s: %s\n", filename, SDL_GetError());
    }
}

/**
  * @brief Destructor for sound effect objects
  *
  * Ensures all instances of this sound effect stop playing
  * before freeing resources.
  */
sound_effect::~sound_effect()
{
    if (!sound_is_initialized())
        return;

    if (m_audio)
    {
        for (MIX_Track *track : sfx_tracks)
        {
            if (MIX_GetTrackAudio(track) == m_audio)
            {
                MIX_StopTrack(track, 0);
                MIX_SetTrackAudio(track, nullptr);
            }
        }
        MIX_DestroyAudio(m_audio);
        m_audio = nullptr;
    }
}

/**
  * @brief Plays a sound effect with specified parameters
  *
  * @param gain Volume gain (0.0-1.0)
  * @param frequency_ratio Playback pitch/rate, where 1.0 is normal speed
  * @param panpot Stereo panning (0=right, 128=center, 255=left)
  */
void sound_effect::play(float gain, float frequency_ratio, int panpot)
{
    if (ScopedSoundMute::active())
        return;
    if (!sound_is_initialized() || settings.no_sound || !m_audio)
        return;

    // Clamp values to valid ranges
    gain = std::clamp(gain, 0.0f, 1.0f);
    panpot = std::clamp(panpot, 0, 255);
    frequency_ratio = std::clamp(frequency_ratio, 0.01f, 100.0f);

    for (MIX_Track *track : sfx_tracks)
    {
        if (MIX_TrackPlaying(track) || MIX_TrackPaused(track))
            continue;

        const MIX_StereoGains gains = {
            static_cast<float>(panpot) / 255.0f,
            static_cast<float>(255 - panpot) / 255.0f,
        };
        MIX_SetTrackAudio(track, m_audio);
        MIX_SetTrackGain(track, gain);
        MIX_SetTrackStereo(track, &gains);
        MIX_SetTrackFrequencyRatio(track, frequency_ratio);
        if (!MIX_PlayTrack(track, 0))
            printf("Failed to play sound: %s\n", SDL_GetError());
        return;
    }
}
