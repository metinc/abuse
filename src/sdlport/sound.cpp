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
#include <cstring>
#include <filesystem>
#include <mutex>
#include <new>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include "sound.h"
#include "specs.h"
#include "setup.h"

// Global settings object (defined setup.cpp)
extern Settings settings;

namespace
{
constexpr int SFX_TRACK_COUNT = 50;
constexpr int MUSIC_TRACK_POOL_SIZE = 2;
constexpr int MUSIC_FADE_MILLISECONDS = 100;
constexpr char FLUIDSYNTH_DECODER[] = "FLUIDSYNTH";
constexpr char FLUIDSYNTH_SOUNDFONT_PROPERTY[] = "SDL_mixer.decoder.fluidsynth.soundfont_path";

MIX_Mixer *mixer = nullptr;
std::vector<MIX_Track *> sfx_tracks;
std::vector<MIX_Track *> music_tracks;
std::string soundfont_path;
bool fluidsynth_available = false;

struct RetiredMusic
{
    MIX_Track *track;
    MIX_Audio *audio;
};

std::mutex music_mutex;
std::vector<RetiredMusic *> retired_music;

MIX_Track *acquire_music_track()
{
    std::lock_guard<std::mutex> lock(music_mutex);
    if (!music_tracks.empty())
    {
        MIX_Track *track = music_tracks.back();
        music_tracks.pop_back();
        return track;
    }
    return mixer ? MIX_CreateTrack(mixer) : nullptr;
}

void release_music_track(MIX_Track *track)
{
    if (!track)
        return;

    MIX_SetTrackStoppedCallback(track, nullptr, nullptr);
    MIX_SetTrackAudio(track, nullptr);

    bool keep = false;
    {
        std::lock_guard<std::mutex> lock(music_mutex);
        if (mixer && music_tracks.size() < MUSIC_TRACK_POOL_SIZE)
        {
            music_tracks.push_back(track);
            keep = true;
        }
    }
    if (!keep)
        MIX_DestroyTrack(track);
}

void SDLCALL retired_music_stopped(void *userdata, MIX_Track *track)
{
    auto *retired = static_cast<RetiredMusic *>(userdata);
    MIX_SetTrackStoppedCallback(track, nullptr, nullptr);
    MIX_SetTrackAudio(track, nullptr);
    MIX_DestroyAudio(retired->audio);

    bool keep_track = false;
    {
        std::lock_guard<std::mutex> lock(music_mutex);
        const auto it = std::find(retired_music.begin(), retired_music.end(), retired);
        if (it != retired_music.end())
            retired_music.erase(it);
        if (mixer && music_tracks.size() < MUSIC_TRACK_POOL_SIZE)
        {
            music_tracks.push_back(track);
            keep_track = true;
        }
    }
    if (!keep_track)
        MIX_DestroyTrack(track);
    delete retired;
}

void dispose_music(MIX_Track *&track, MIX_Audio *&audio, int fadeout_milliseconds)
{
    if (!track)
    {
        MIX_DestroyAudio(audio);
        audio = nullptr;
        return;
    }

    if (!MIX_TrackPlaying(track) || MIX_TrackPaused(track))
    {
        release_music_track(track);
        MIX_DestroyAudio(audio);
        track = nullptr;
        audio = nullptr;
        return;
    }

    auto *retired = new (std::nothrow) RetiredMusic{track, audio};
    if (!retired)
    {
        MIX_StopTrack(track, 0);
        release_music_track(track);
        MIX_DestroyAudio(audio);
        track = nullptr;
        audio = nullptr;
        return;
    }

    const bool already_fading_out = MIX_GetTrackFadeFrames(track) < 0;
    {
        std::lock_guard<std::mutex> lock(music_mutex);
        retired_music.push_back(retired);
    }
    track = nullptr;
    audio = nullptr;

    if (!MIX_SetTrackStoppedCallback(retired->track, retired_music_stopped, retired))
    {
        {
            std::lock_guard<std::mutex> lock(music_mutex);
            const auto it = std::find(retired_music.begin(), retired_music.end(), retired);
            if (it != retired_music.end())
                retired_music.erase(it);
        }
        MIX_StopTrack(retired->track, 0);
        release_music_track(retired->track);
        MIX_DestroyAudio(retired->audio);
        delete retired;
        return;
    }

    if (!already_fading_out &&
        !MIX_StopTrack(retired->track, MIX_TrackMSToFrames(retired->track, fadeout_milliseconds)))
    {
        MIX_SetTrackStoppedCallback(retired->track, nullptr, nullptr);
        {
            std::lock_guard<std::mutex> lock(music_mutex);
            const auto it = std::find(retired_music.begin(), retired_music.end(), retired);
            if (it != retired_music.end())
                retired_music.erase(it);
        }
        release_music_track(retired->track);
        MIX_DestroyAudio(retired->audio);
        delete retired;
    }
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

MIX_Audio *load_prefixed_audio(const char *filename)
{
    if (!filename)
        return nullptr;

    const std::filesystem::path audio_path = resolve_audio_path(filename);
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

bool sound_is_initialized()
{
    return mixer != nullptr;
}

bool sound_set_soundfont(const std::string &configured_soundfont)
{
    std::string resolved_path;
    if (!resolve_soundfont(configured_soundfont, resolved_path))
        return false;

    if (sound_is_initialized() && !resolved_path.empty() && !fluidsynth_available)
    {
        printf("Sound: FluidSynth MIDI decoder is unavailable; cannot use SoundFont: %s\n",
               resolved_path.c_str());
        return false;
    }

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

    const int num_decoders = MIX_GetNumAudioDecoders();
    fluidsynth_available = false;
    if (num_decoders > 0)
    {
        printf("Sound: Audio decoders:");
        for (int i = 0; i < num_decoders; i++)
        {
            const char *decoder = MIX_GetAudioDecoder(i);
            printf("%s%s", (i == 0) ? " " : ", ", decoder);
            if (decoder && std::strcmp(decoder, FLUIDSYNTH_DECODER) == 0)
                fluidsynth_available = true;
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
        return false;
    }

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

    music_tracks.reserve(MUSIC_TRACK_POOL_SIZE);
    for (int i = 0; i < MUSIC_TRACK_POOL_SIZE; ++i)
    {
        MIX_Track *track = MIX_CreateTrack(mixer);
        if (!track)
        {
            printf("Sound: Could only create %zu reusable music tracks: %s\n", music_tracks.size(), SDL_GetError());
            break;
        }
        music_tracks.push_back(track);
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
    {
        std::lock_guard<std::mutex> lock(music_mutex);
        mixer = nullptr;
        music_tracks.clear();
        for (RetiredMusic *retired : retired_music)
        {
            MIX_DestroyAudio(retired->audio);
            delete retired;
        }
        retired_music.clear();
    }
    sfx_tracks.clear();
    soundfont_path.clear();
    fluidsynth_available = false;
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

/**
  * @brief Constructor for music/song objects
  *
  * Loads a Standard MIDI music file and prepares it for playback.
  *
  * @param filename Path to the music file
  */
song::song(char const *filename)
    : m_filename(filename ? filename : ""), m_audio(nullptr), m_track(nullptr), m_gain(1.0f)
{
    load();
}

bool song::load()
{
    if (!sound_is_initialized() || m_filename.empty())
        return false;

    try
    {
        const std::filesystem::path audio_path = resolve_audio_path(m_filename.c_str());
        SDL_IOStream *io = SDL_IOFromFile(audio_path.string().c_str(), "rb");
        if (!io)
        {
            printf("Sound: ERROR - could not create IO stream for %s: %s\n", m_filename.c_str(), SDL_GetError());
            return false;
        }

        SDL_PropertiesID props = SDL_CreateProperties();
        bool properties_ok = props != 0;
        properties_ok = properties_ok && SDL_SetPointerProperty(props, MIX_PROP_AUDIO_LOAD_IOSTREAM_POINTER, io);
        properties_ok = properties_ok && SDL_SetBooleanProperty(props, MIX_PROP_AUDIO_LOAD_CLOSEIO_BOOLEAN, true);
        // Keep the small MIDI file in memory and let FluidSynth generate PCM
        // incrementally while the track plays. Predecoding a whole song can
        // otherwise consume tens of megabytes and delays every transition.
        properties_ok = properties_ok && SDL_SetBooleanProperty(props, MIX_PROP_AUDIO_LOAD_PREDECODE_BOOLEAN, false);
        properties_ok =
            properties_ok && SDL_SetBooleanProperty(props, MIX_PROP_AUDIO_LOAD_SKIP_METADATA_TAGS_BOOLEAN, true);
        properties_ok =
            properties_ok && SDL_SetPointerProperty(props, MIX_PROP_AUDIO_LOAD_PREFERRED_MIXER_POINTER, mixer);
        if (properties_ok && !soundfont_path.empty())
        {
            if (!fluidsynth_available)
            {
                printf("Sound: ERROR - FluidSynth is required to play %s with the selected SoundFont\n",
                       m_filename.c_str());
                properties_ok = false;
            }
            else
            {
                properties_ok = SDL_SetStringProperty(props, MIX_PROP_AUDIO_DECODER_STRING, FLUIDSYNTH_DECODER);
                properties_ok = properties_ok &&
                                SDL_SetStringProperty(props, FLUIDSYNTH_SOUNDFONT_PROPERTY, soundfont_path.c_str());
            }
        }

        if (properties_ok)
            m_audio = MIX_LoadAudioWithProperties(props);
        else
            SDL_CloseIO(io);
        SDL_DestroyProperties(props);

        if (!m_audio)
        {
            printf("Sound: ERROR - %s while loading %s\n", SDL_GetError(), m_filename.c_str());
            return false;
        }

        m_track = acquire_music_track();
        if (!m_track)
            printf("Sound: ERROR - could not create music track for %s: %s\n", m_filename.c_str(), SDL_GetError());
        else if (!MIX_SetTrackAudio(m_track, m_audio))
        {
            printf("Sound: ERROR - could not configure music track for %s: %s\n", m_filename.c_str(), SDL_GetError());
            release_music_track(m_track);
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
    if (!sound_is_initialized())
        return;
    dispose_music(m_track, m_audio, MUSIC_FADE_MILLISECONDS);
}

/**
  * @brief Starts playing the music
  *
  * @param gain Volume gain (0.0-1.0)
  */
void song::play(float gain)
{
    if (!sound_is_initialized() || settings.no_music || !m_track)
        return;

    set_gain(gain);
    start_playback();
}

bool song::start_playback(Sint64 start_milliseconds)
{
    SDL_PropertiesID options = SDL_CreateProperties();
    if (options)
    {
        SDL_SetNumberProperty(options, MIX_PROP_PLAY_LOOPS_NUMBER, -1);
        SDL_SetNumberProperty(options, MIX_PROP_PLAY_FADE_IN_MILLISECONDS_NUMBER, MUSIC_FADE_MILLISECONDS);
        SDL_SetFloatProperty(options, MIX_PROP_PLAY_FADE_IN_START_GAIN_FLOAT, 0.0f);
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
void song::stop(int fadeout_time)
{
    if (sound_is_initialized() && m_track)
    {
        const int duration = fadeout_time > 0 ? fadeout_time : MUSIC_FADE_MILLISECONDS;
        dispose_music(m_track, m_audio, duration);
    }
}

/**
  * @brief Checks if music is currently playing
  *
  * @return true if music is playing
  */
bool song::playing() const
{
    return sound_is_initialized() && m_track && MIX_TrackPlaying(m_track);
}

/**
  * @brief Sets the music gain
  *
  * @param gain New volume gain (0.0-1.0)
  */
void song::set_gain(float gain)
{
    m_gain = std::clamp(gain, 0.0f, 1.0f);
    if (sound_is_initialized() && m_track)
        MIX_SetTrackGain(m_track, m_gain);
}

bool song::reload()
{
    if (!sound_is_initialized())
        return false;

    MIX_Audio *old_audio = m_audio;
    MIX_Track *old_track = m_track;
    m_track = nullptr;
    m_audio = nullptr;

    if (!load())
    {
        release_music_track(m_track);
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
    }

    if (resume)
    {
        set_gain(m_gain);
        if (!start_playback(position_milliseconds))
        {
            release_music_track(m_track);
            MIX_DestroyAudio(m_audio);
            m_audio = old_audio;
            m_track = old_track;
            return false;
        }
        dispose_music(old_track, old_audio, MUSIC_FADE_MILLISECONDS);
    }
    else
    {
        release_music_track(old_track);
        MIX_DestroyAudio(old_audio);
    }
    return true;
}
