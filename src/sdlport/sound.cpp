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

#include <cstring>
#include <string>
#include <filesystem>
#include <algorithm>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include "sound.h"
#include "hmi.h"
#include "specs.h"
#include "setup.h"

// Global settings object (defined setup.cpp)
extern Settings settings;

int enabled = 0; // Indicates if sound system is operational
SDL_AudioSpec audio_spec; // Stores current audio specifications

namespace
{
constexpr int SFX_TRACK_COUNT = 50;
constexpr char FLUIDSYNTH_SOUNDFONT_PROPERTY[] = "SDL_mixer.decoder.fluidsynth.soundfont_path";

MIX_Mixer *mixer = nullptr;
std::vector<MIX_Track *> sfx_tracks;
std::string soundfont_path;

MIX_Audio *load_prefixed_audio(const char *filename)
{
    FILE *file = prefix_fopen(filename, "rb");
    if (!file)
        return nullptr;

    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return nullptr;
    }

    const long file_size = ftell(file);
    if (file_size <= 0 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return nullptr;
    }

    std::vector<unsigned char> bytes(static_cast<size_t>(file_size));
    const bool read_ok = fread(bytes.data(), 1, bytes.size(), file) == bytes.size();
    fclose(file);
    if (!read_ok)
        return nullptr;

    SDL_IOStream *io = SDL_IOFromConstMem(bytes.data(), bytes.size());
    if (!io)
        return nullptr;

    // Predecoding makes the returned object independent of the temporary buffer.
    return MIX_LoadAudio_IO(mixer, io, true, true);
}
}

/**
  * @brief Initializes the sound system
  *
  * This function performs the following steps:
  * 1. Verifies the existence of the sfx directory
  * 2. Initializes SDL_mixer with standard audio parameters
  * 3. Loads custom soundfonts if specified in settings
  * 4. Allocates mixing channels
  * 5. Queries and stores the actual audio specifications
  *
  * @param argc Command line argument count (unused)
  * @param argv Command line arguments (unused)
  * @return int 0 on failure, non-zero on success
  */
int sound_init(int argc, char **argv)
{
    // Sound and music are always enabled, they just never play if disabled in config
    // or if loading the files failed (enabled == false)

    // Get the path to the game's data directory and sfx subdirectory
    const std::filesystem::path datadir = get_filename_prefix();
    const std::filesystem::path sfx_path = datadir / "sfx";

    // Verify sfx directory exists
    if (!std::filesystem::exists(sfx_path))
    {
        printf("Sound: Disabled (couldn't find the sfx directory %s)\n", sfx_path.string().c_str());
        return 0;
    }

    if (!MIX_Init())
    {
        printf("Sound: Unable to initialize SDL_mixer - %s\nSound: Disabled (error)\n", SDL_GetError());
        return 0;
    }

    const int num_decoders = MIX_GetNumAudioDecoders();
    if (num_decoders > 0)
    {
        printf("Sound: Audio decoders:");
        for (int i = 0; i < num_decoders; i++)
        {
            printf("%s%s", (i == 0) ? " " : ", ", MIX_GetAudioDecoder(i));
        }
        printf("\n");
    }
    else
    {
        printf("Sound: No music decoders available!\n");
    }

    SDL_AudioSpec requested_spec{};
    requested_spec.freq = 44100;
    requested_spec.format = SDL_AUDIO_S16;
    requested_spec.channels = settings.mono ? 1 : 2;
    mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &requested_spec);
    if (!mixer)
    {
        MIX_Quit();
        printf("Sound: Unable to open audio - %s\nSound: Disabled (error)\n", SDL_GetError());
        return 0;
    }

    MIX_GetMixerFormat(mixer, &audio_spec);

    // Load custom soundfont if specified in settings
    // Soundfonts provide instrument samples for MIDI playback
    if (!settings.soundfont.empty())
    {
        std::filesystem::path configured_soundfont = settings.soundfont;
        if (configured_soundfont.is_relative())
            configured_soundfont = datadir / "soundfonts" / configured_soundfont;

        if (!std::filesystem::exists(configured_soundfont))
        {
            printf("Sound: Soundfont not found: %s\n", configured_soundfont.string().c_str());
        }
        else
        {
            soundfont_path = configured_soundfont.string();
            printf("Sound: Using soundfont: %s\n", soundfont_path.c_str());
        }
    }
    else
    {
        printf("Sound: No custom soundfont specified, using default.\n");
    }
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

    // Enable both SFX and music subsystems
    enabled = SFX_INITIALIZED | MUSIC_INITIALIZED;

    return enabled;
}

/**
  * @brief Shuts down the sound system
  *
  * Closes the audio device and marks the system as disabled.
  * Safe to call even if sound system wasn't initialized.
  */
void sound_uninit()
{
    if (!enabled)
        return;

    enabled = 0;
    sfx_tracks.clear(); // MIX_DestroyMixer owns and destroys these tracks.
    MIX_DestroyMixer(mixer);
    mixer = nullptr;
    soundfont_path.clear();
    MIX_Quit();
}

/**
  * @brief Constructor for sound effect objects
  *
  * Loads a sound effect from a file and prepares it for playback.
  * Uses SDL_IOStream for memory-based loading to avoid leaving files open.
  *
  * @param filename Path to the sound effect file
  */
sound_effect::sound_effect(char const *filename) : m_audio(nullptr)
{
    if (!enabled)
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
    if (!enabled)
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
  * @param volume Volume level (0-127)
  * @param pitch Pitch adjustment (unused in current implementation)
  * @param panpot Stereo panning (0=right, 128=center, 255=left)
  */
void sound_effect::play(int volume, int pitch, int panpot)
{
    if (!enabled || settings.no_sound || !m_audio)
        return;

    // Clamp values to valid ranges
    volume = std::clamp(volume, 0, 127);
    panpot = std::clamp(panpot, 0, 255);

    for (MIX_Track *track : sfx_tracks)
    {
        if (MIX_TrackPlaying(track) || MIX_TrackPaused(track))
            continue;

        const MIX_StereoGains gains = {
            static_cast<float>(panpot) / 255.0f,
            static_cast<float>(255 - panpot) / 255.0f,
        };
        MIX_SetTrackAudio(track, m_audio);
        MIX_SetTrackGain(track, static_cast<float>(volume) / 127.0f);
        MIX_SetTrackStereo(track, &gains);
        if (!MIX_PlayTrack(track, 0))
            printf("Failed to play sound: %s\n", SDL_GetError());
        return;
    }
}

/**
  * @brief Constructor for music/song objects
  *
  * Loads a music file (HMI format) and prepares it for playback.
  * Uses SDL_IOStream for memory-based playback to avoid keeping files open.
  *
  * @param filename Path to the music file
  */
song::song(char const *filename)
    : m_audio(nullptr),
      m_track(nullptr)
{
    if (!enabled || !filename)
        return;

    try
    {
        // Load HMI format music file into memory
        uint32_t data_size;
        unsigned char *data = load_hmi(filename, data_size);

        if (!data)
        {
            printf("Sound: ERROR - could not load %s\n", filename);
            return;
        }

        SDL_IOStream *io = SDL_IOFromConstMem(data, data_size);
        if (!io)
        {
            printf("Sound: ERROR - could not create IO stream for %s: %s\n", filename, SDL_GetError());
            free(data);
            return;
        }

        SDL_PropertiesID props = SDL_CreateProperties();
        bool properties_ok = props != 0;
        properties_ok = properties_ok && SDL_SetPointerProperty(props, MIX_PROP_AUDIO_LOAD_IOSTREAM_POINTER, io);
        properties_ok = properties_ok && SDL_SetBooleanProperty(props, MIX_PROP_AUDIO_LOAD_CLOSEIO_BOOLEAN, true);
        properties_ok = properties_ok && SDL_SetPointerProperty(props, MIX_PROP_AUDIO_LOAD_PREFERRED_MIXER_POINTER, mixer);
        if (properties_ok && !soundfont_path.empty())
            properties_ok = SDL_SetStringProperty(props, FLUIDSYNTH_SOUNDFONT_PROPERTY, soundfont_path.c_str());

        if (properties_ok)
            m_audio = MIX_LoadAudioWithProperties(props);
        else
            SDL_CloseIO(io);
        SDL_DestroyProperties(props);
        free(data);

        if (!m_audio)
        {
            printf("Sound: ERROR - %s while loading %s\n", SDL_GetError(), filename);
            return;
        }

        m_track = MIX_CreateTrack(mixer);
        if (!m_track)
            printf("Sound: ERROR - could not create music track for %s: %s\n", filename, SDL_GetError());
        else if (!MIX_SetTrackAudio(m_track, m_audio))
        {
            printf("Sound: ERROR - could not configure music track for %s: %s\n", filename, SDL_GetError());
            MIX_DestroyTrack(m_track);
            m_track = nullptr;
        }
    }
    catch (const std::exception &e)
    {
        printf("Sound: ERROR - Exception while loading %s: %s\n", filename, e.what());
    }
}

/**
  * @brief Destructor for music/song objects
  *
  * Stops playback and frees all allocated resources.
  */
song::~song()
{
    if (playing())
        stop();

    if (!enabled)
        return;
    MIX_DestroyTrack(m_track);
    MIX_DestroyAudio(m_audio);
    m_track = nullptr;
    m_audio = nullptr;
}

/**
  * @brief Starts playing the music
  *
  * @param volume Volume level (0-127)
  */
void song::play(unsigned char volume)
{
    if (!enabled || settings.no_music || !m_track)
        return;

    set_volume(volume);
    SDL_PropertiesID options = SDL_CreateProperties();
    if (options)
        SDL_SetNumberProperty(options, MIX_PROP_PLAY_LOOPS_NUMBER, -1);
    if (!MIX_PlayTrack(m_track, options))
        printf("Sound: ERROR - could not play music: %s\n", SDL_GetError());
    SDL_DestroyProperties(options);
}

/**
  * @brief Stops music playback
  *
  * @param fadeout_time Fadeout duration in milliseconds (100 ms by default)
  */
void song::stop(long fadeout_time)
{
    if (enabled && m_track)
    {
        const long duration = fadeout_time > 0 ? fadeout_time : 100;
        MIX_StopTrack(m_track, MIX_TrackMSToFrames(m_track, duration));
    }
}

/**
  * @brief Checks if music is currently playing
  *
  * @return int Non-zero if music is playing, 0 otherwise
  */
int song::playing()
{
    return enabled && m_track && MIX_TrackPlaying(m_track);
}

/**
  * @brief Sets the music volume
  *
  * @param volume New volume level (0-127)
  */
void song::set_volume(int volume)
{
    if (enabled && m_track)
        MIX_SetTrackGain(m_track, static_cast<float>(std::clamp(volume, 0, 127)) / 127.0f);
}
