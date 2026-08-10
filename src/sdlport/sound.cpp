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
#include <cctype>
#include <filesystem>
#include <string>
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
    if (!filename)
        return nullptr;

    const std::filesystem::path filename_path(filename);
    const std::filesystem::path audio_path =
        filename_path.is_absolute() ? filename_path : std::filesystem::path(get_filename_prefix()) / filename_path;
    return MIX_LoadAudio(mixer, audio_path.string().c_str(), true);
}

bool resolve_soundfont(const std::string &configured_soundfont, std::string &resolved_path)
{
    resolved_path.clear();
    if (configured_soundfont.empty())
        return true;

    std::filesystem::path path = configured_soundfont;
    if (path.is_relative())
        path = std::filesystem::path(get_filename_prefix()) / "soundfonts" / path;

    if (!std::filesystem::is_regular_file(path))
    {
        printf("Sound: Soundfont not found: %s\n", path.string().c_str());
        return false;
    }

    resolved_path = path.string();
    return true;
}

std::string fallback_soundfont(const std::filesystem::path &directory)
{
    if (std::filesystem::is_regular_file(directory / DEFAULT_SOUNDFONT))
        return DEFAULT_SOUNDFONT;

    std::vector<std::string> soundfonts;
    std::error_code error;
    for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end; it.increment(error))
    {
        if (!it->is_regular_file(error))
            continue;

        std::string extension = it->path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (extension == ".sf2" || extension == ".sf3")
            soundfonts.push_back(it->path().filename().string());
    }

    if (soundfonts.empty())
        return {};

    std::sort(soundfonts.begin(), soundfonts.end());
    return soundfonts.front();
}
}

bool sound_set_soundfont(const std::string &configured_soundfont)
{
    std::string resolved_path;
    if (!resolve_soundfont(configured_soundfont, resolved_path))
        return false;

    soundfont_path = std::move(resolved_path);
    if (soundfont_path.empty())
        printf("Sound: Using the default SoundFont.\n");
    else
        printf("Sound: Using SoundFont: %s\n", soundfont_path.c_str());
    return true;
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

    // FluidSynth needs an explicit SoundFont on systems without a configured
    // system-wide default. Fall back to the bundled default when the configured
    // font is unavailable.
    const std::string fallback = fallback_soundfont(datadir / "soundfonts");
    if (settings.soundfont.empty() || !sound_set_soundfont(settings.soundfont))
    {
        if (sound_set_soundfont(fallback))
            settings.SetSoundFont(fallback);
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
  * Uses SDL_mixer's file loader and predecodes the sound for repeated playback.
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
  * @param frequency_ratio Playback pitch/rate, where 1.0 is normal speed
  * @param panpot Stereo panning (0=right, 128=center, 255=left)
  */
void sound_effect::play(int volume, float frequency_ratio, int panpot)
{
    if (!enabled || settings.no_sound || !m_audio)
        return;

    // Clamp values to valid ranges
    volume = std::clamp(volume, 0, 127);
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
        MIX_SetTrackGain(track, static_cast<float>(volume) / 127.0f);
        MIX_SetTrackStereo(track, &gains);
        MIX_SetTrackFrequencyRatio(track, frequency_ratio);
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
    : m_filename(filename ? filename : ""), m_audio(nullptr), m_track(nullptr), m_volume(127)
{
    load();
}

bool song::load()
{
    if (!enabled || m_filename.empty())
        return false;

    try
    {
        // Load HMI format music file into memory
        uint32_t data_size;
        unsigned char *data = load_hmi(m_filename.c_str(), data_size);

        if (!data)
        {
            printf("Sound: ERROR - could not load %s\n", m_filename.c_str());
            return false;
        }

        SDL_IOStream *io = SDL_IOFromConstMem(data, data_size);
        if (!io)
        {
            printf("Sound: ERROR - could not create IO stream for %s: %s\n", m_filename.c_str(), SDL_GetError());
            free(data);
            return false;
        }

        SDL_PropertiesID props = SDL_CreateProperties();
        bool properties_ok = props != 0;
        properties_ok = properties_ok && SDL_SetPointerProperty(props, MIX_PROP_AUDIO_LOAD_IOSTREAM_POINTER, io);
        properties_ok = properties_ok && SDL_SetBooleanProperty(props, MIX_PROP_AUDIO_LOAD_CLOSEIO_BOOLEAN, true);
        properties_ok =
            properties_ok && SDL_SetPointerProperty(props, MIX_PROP_AUDIO_LOAD_PREFERRED_MIXER_POINTER, mixer);
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
            printf("Sound: ERROR - %s while loading %s\n", SDL_GetError(), m_filename.c_str());
            return false;
        }

        m_track = MIX_CreateTrack(mixer);
        if (!m_track)
            printf("Sound: ERROR - could not create music track for %s: %s\n", m_filename.c_str(), SDL_GetError());
        else if (!MIX_SetTrackAudio(m_track, m_audio))
        {
            printf("Sound: ERROR - could not configure music track for %s: %s\n", m_filename.c_str(), SDL_GetError());
            MIX_DestroyTrack(m_track);
            m_track = nullptr;
        }
        return m_track != nullptr;
    }
    catch (const std::exception &e)
    {
        printf("Sound: ERROR - Exception while loading %s: %s\n", m_filename.c_str(), e.what());
        return false;
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
    start_playback();
}

bool song::start_playback(Sint64 start_milliseconds)
{
    SDL_PropertiesID options = SDL_CreateProperties();
    if (options)
    {
        SDL_SetNumberProperty(options, MIX_PROP_PLAY_LOOPS_NUMBER, -1);
        if (start_milliseconds > 0)
            SDL_SetNumberProperty(options, MIX_PROP_PLAY_START_MILLISECOND_NUMBER, start_milliseconds);
    }
    const bool started = MIX_PlayTrack(m_track, options);
    if (!started)
        printf("Sound: ERROR - could not play music: %s\n", SDL_GetError());
    SDL_DestroyProperties(options);
    return started;
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
    m_volume = std::clamp(volume, 0, 127);
    if (enabled && m_track)
        MIX_SetTrackGain(m_track, static_cast<float>(m_volume) / 127.0f);
}

bool song::reload()
{
    if (!enabled)
        return false;

    MIX_Audio *old_audio = m_audio;
    MIX_Track *old_track = m_track;
    m_track = nullptr;
    m_audio = nullptr;

    if (!load())
    {
        MIX_DestroyTrack(m_track);
        MIX_DestroyAudio(m_audio);
        m_audio = old_audio;
        m_track = old_track;
        return false;
    }

    const bool resume = old_track && MIX_TrackPlaying(old_track);
    Sint64 position_milliseconds = 0;
    if (resume)
    {
        const Sint64 position_frames = MIX_GetTrackPlaybackPosition(old_track);
        if (position_frames >= 0)
            position_milliseconds = MIX_TrackFramesToMS(old_track, position_frames);
        MIX_StopTrack(old_track, 0);
    }
    MIX_DestroyTrack(old_track);
    MIX_DestroyAudio(old_audio);

    if (resume)
    {
        set_volume(m_volume);
        return start_playback(position_milliseconds);
    }
    return true;
}
