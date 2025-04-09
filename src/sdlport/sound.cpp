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
#include <memory>
#include <system_error>
#include <algorithm>

#include <SDL3/SDL.h>
#include <SDL_mixer.h>

#include "sound.h"
#include "hmi.h"
#include "specs.h"
#include "setup.h"

// Global settings object (defined setup.cpp)
extern Settings settings;

int enabled = 0; // Indicates if sound system is operational
SDL_AudioSpec audio_spec; // Stores current audio specifications

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

    int mix_flags = MIX_INIT_MID | MIX_INIT_MP3 | MIX_INIT_OGG;
    int init_result = Mix_Init(mix_flags);
    if ((init_result & mix_flags) != mix_flags)
    {
        printf("Sound: Warning - Some codecs failed to initialize: %s\n", Mix_GetError());
        // Continue anyway as some formats might still work
    }

    // Initialize SDL_mixer with CD quality audio (44.1kHz, 16-bit stereo)
    // Buffer size of 1024 samples provides good balance of latency and stability
    if (Mix_OpenAudio(44100, SDL_AUDIO_S16, 2, 1024) < 0)
    {
        Mix_Quit(); // Clean up the codecs we initialized
        printf("Sound: Unable to open audio - %s\nSound: Disabled (error)\n", SDL_GetError());
        return 0;
    }

    // Load custom soundfont if specified in settings
    // Soundfonts provide instrument samples for MIDI playback
    if (!settings.soundfont.empty())
    {
        const auto soundfont_path = datadir / "soundfonts" / settings.soundfont;

        if (Mix_SetSoundFonts(soundfont_path.string().c_str()) == 0)
        {
            printf("Sound: Failed to load soundfont %s: %s\n", soundfont_path.string().c_str(), Mix_GetError());
        }
        else
        {
            printf("Sound: Loaded soundfont: %s\n", soundfont_path.string().c_str());
        }
    }

    // Allocate mixing channels for simultaneous sound effects
    Mix_AllocateChannels(50);

    // Query actual audio specifications that were obtained
    int temp_channels = 0;
    Mix_QuerySpec(&audio_spec.freq, &audio_spec.format, &temp_channels);
    audio_spec.channels = static_cast<uint8_t>(temp_channels & 0xFF);

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

    Mix_CloseAudio();
    Mix_Quit();
    enabled = false;
}

/**
  * @brief Constructor for sound effect objects
  *
  * Loads a sound effect from a file and prepares it for playback.
  * Uses SDL_RWops for memory-based loading to avoid leaving files open.
  *
  * @param filename Path to the sound effect file
  */
sound_effect::sound_effect(char const *filename) : m_chunk(nullptr)
{
    if (!enabled)
        return;

    // Use prefix_fopen to get the file
    FILE *file = prefix_fopen(filename, "rb");
    if (!file)
    {
        printf("Failed to open file %s\n", filename);
        return;
    }

    // Create SDL_RWops from the FILE*
    SDL_RWops *rw = SDL_RWFromFP(file, true); // true means SDL will close the file
    if (!rw)
    {
        printf("Failed to create RWops for %s: %s\n", filename, SDL_GetError());
        fclose(file);
        return;
    }

    m_chunk = Mix_LoadWAV_RW(rw, true); // 1 means SDL will free the RWops
    if (!m_chunk)
    {
        printf("Failed to load WAV from file %s: %s\n", filename, Mix_GetError());
    }
}

/**
  * @brief Destructor for sound effect objects
  *
  * Ensures all instances of this sound effect stop playing
  * before freeing resources. Uses fade out to avoid audio pops.
  */
sound_effect::~sound_effect()
{
    if (!enabled)
        return;

    // Sound effect deletion only happens on level load
    Mix_FadeOutGroup(-1, 100); // Fade out all channels over 100ms
    while (Mix_Playing(-1))
    { // Wait for all sounds to finish
        SDL_Delay(10);
    }

    if (m_chunk)
    {
        Mix_FreeChunk(m_chunk);
        m_chunk = nullptr;
    }
}

/**
  * @brief Plays a sound effect with specified parameters
  *
  * @param volume Volume level (0-127)
  * @param pitch Pitch adjustment (unused in current implementation)
  * @param panpot Stereo panning (0=left, 128=center, 255=right)
  */
void sound_effect::play(int volume, int pitch, int panpot)
{
    if (!enabled || settings.no_sound || !m_chunk)
        return;

    // Clamp values to valid ranges
    volume = std::clamp(volume, 0, 127);
    panpot = std::clamp(panpot, 0, 255);

    // Play on first available channel (-1)
    int channel = Mix_PlayChannel(-1, m_chunk, 0);
    if (channel > -1)
    {
        Mix_Volume(channel, volume);
        Mix_SetPanning(channel, static_cast<uint8_t>(panpot), static_cast<uint8_t>(255 - panpot));
    }
}

/**
  * @brief Constructor for music/song objects
  *
  * Loads a music file (HMI format) and prepares it for playback.
  * Uses SDL_RWops for memory-based playback to avoid keeping files open.
  *
  * @param filename Path to the music file
  */
song::song(char const *filename)
    : data(nullptr), // Raw music data
      song_id(0), // Playback identifier
      rw(nullptr), // SDL memory operations
      music(nullptr) // SDL mixer music object
{
    if (!filename)
        return;

    try
    {
        // Load HMI format music file into memory
        uint32_t data_size;
        data = load_hmi(filename, data_size);

        if (!data)
        {
            printf("Sound: ERROR - could not load %s\n", filename);
            return;
        }

        // Create SDL_RWops for memory-based playback
        rw = SDL_RWFromMem(data, data_size);
        if (!rw)
        {
            printf("Sound: ERROR - could not create RWops for %s\n", filename);
            return;
        }

        // Load music using SDL_mixer
        music = Mix_LoadMUS_RW(rw, false);

        if (!music)
        {
            printf("Sound: ERROR - %s while loading %s\n", Mix_GetError(), filename);
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

    free(data); // Using free because it was allocated by load_hmi

    if (music)
        Mix_FreeMusic(music);
    if (rw)
        SDL_FreeRW(rw);
}

/**
  * @brief Starts playing the music
  *
  * @param volume Volume level (0-127)
  */
void song::play(unsigned char volume)
{
    if (!enabled || settings.no_music || !music)
        return;

    song_id = 1;
    Mix_PlayMusic(music, -1);
    Mix_VolumeMusic(std::clamp(static_cast<int>(volume), 0, 127));
}

/**
  * @brief Stops music playback
  *
  * @param fadeout_time Fadeout duration in milliseconds (unused in current implementation)
  */
void song::stop(long fadeout_time)
{
    song_id = 0;
    Mix_FadeOutMusic(100); // Always fade out over 100ms to avoid audio pops
}

/**
  * @brief Checks if music is currently playing
  *
  * @return int Non-zero if music is playing, 0 otherwise
  */
int song::playing()
{
    return Mix_PlayingMusic();
}

/**
  * @brief Sets the music volume
  *
  * @param volume New volume level (0-127)
  */
void song::set_volume(int volume)
{
    if (music)
        Mix_VolumeMusic(volume);
}