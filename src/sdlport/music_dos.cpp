/* Music playback through the Sound Blaster's OPL chip.
 * Distributed under the GNU General Public License, version 3 or later.
 */

#include "sound.h"
#include "music.h"
#include "setup.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <SDL3/SDL.h>
#include <adlmidi.h>

extern Settings settings;

namespace
{
constexpr int hmi_bank = 2; // libADLMIDI: HMI (Descent, Asterix), Fat Man 2-op instruments.
constexpr Uint32 tick_milliseconds = 5;
constexpr int fade_milliseconds = 100;
}

struct song::Playback
{
    ADL_MIDIPlayer *device = nullptr;
    SDL_Thread *thread = nullptr;
    SDL_Mutex *mutex = SDL_CreateMutex();
    bool quit = false;
    bool running = false;
    float gain = 1.0f;
    Uint64 last_tick = 0;
    Uint64 fade_start = 0;
    Uint64 fade_end = 0;
    bool fading_out = false;
    int applied_volume = -1;

    ~Playback()
    {
        if (thread)
        {
            SDL_LockMutex(mutex);
            quit = true;
            SDL_UnlockMutex(mutex);
            SDL_WaitThread(thread, nullptr);
        }
        if (device)
        {
            adl_panic(device);
            adl_close(device);
        }
        SDL_DestroyMutex(mutex);
    }

    void volume(float value)
    {
        const int level = static_cast<int>(::lround(std::clamp(value, 0.0f, 1.0f) * 16383.0f));
        if (level == applied_volume)
            return;
        // Universal real-time MIDI Master Volume preserves per-channel levels.
        const ADL_UInt8 message[] = {
            0xf0, 0x7f, 0x7f, 0x04, 0x01, static_cast<ADL_UInt8>(level & 127), static_cast<ADL_UInt8>(level >> 7),
            0xf7};
        adl_rt_systemExclusive(device, message, sizeof(message));
        applied_volume = level;
    }

    static int SDLCALL run(void *userdata)
    {
        auto &playback = *static_cast<Playback *>(userdata);
        for (;;)
        {
            SDL_LockMutex(playback.mutex);
            if (playback.quit)
            {
                SDL_UnlockMutex(playback.mutex);
                return 0;
            }
            if (playback.running)
            {
                const Uint64 now = SDL_GetTicks();
                const double elapsed = (now - playback.last_tick) / 1000.0;
                playback.last_tick = now;
                adl_tickEvents(playback.device, elapsed, 0.001);

                float envelope = 1.0f;
                if (playback.fade_end > playback.fade_start)
                    envelope = std::min(1.0f, static_cast<float>(now - playback.fade_start) /
                                                  (playback.fade_end - playback.fade_start));
                if (playback.fading_out)
                    envelope = 1.0f - envelope;
                playback.volume(playback.gain * envelope);
                if (playback.fading_out && now >= playback.fade_end)
                {
                    adl_panic(playback.device);
                    playback.running = false;
                }
            }
            SDL_UnlockMutex(playback.mutex);
            SDL_Delay(tick_milliseconds);
        }
    }
};

bool music_init(MIX_Mixer *)
{
    printf("Music: Sound Blaster OPL, HMI instrument bank.\n");
    return true;
}

void music_uninit()
{
    // Each song owns and stops its sequencer before the sound system shuts down.
}

bool sound_set_soundfont(const std::string &configured_soundfont)
{
    if (configured_soundfont.empty())
        return true;
    printf("Music: OPL instruments do not use SoundFonts.\n");
    return false;
}

song::song(char const *filename)
    : m_filename(filename ? filename : ""), m_playback(std::make_unique<Playback>()), m_gain(1.0f)
{
    load();
}

bool song::load()
{
    if (!sound_is_initialized() || m_filename.empty() || !m_playback->mutex)
        return false;
    m_playback->device = adl_init(22050); // Hardware output; no PCM synthesis in the DOS CPU.
    if (!m_playback->device)
    {
        printf("Music: Unable to initialize OPL: %s\n", adl_errorString());
        return false;
    }
    adl_setLoopEnabled(m_playback->device, 1);
    const auto path = resolve_audio_path(m_filename.c_str());
    if (adl_setBank(m_playback->device, hmi_bank) < 0 || adl_openFile(m_playback->device, path.string().c_str()) < 0)
    {
        printf("Music: Unable to load %s: %s\n", m_filename.c_str(), adl_errorInfo(m_playback->device));
        adl_close(m_playback->device);
        m_playback->device = nullptr;
        return false;
    }
    return true;
}

song::~song() = default;

void song::play(float gain)
{
    if (!sound_is_initialized() || settings.no_music || !m_playback->device)
        return;
    set_gain(gain);
    start_playback();
}

bool song::start_playback(Sint64 start_milliseconds)
{
    auto &playback = *m_playback;
    SDL_LockMutex(playback.mutex);
    adl_positionRewind(playback.device);
    if (start_milliseconds > 0)
        adl_positionSeek(playback.device, start_milliseconds / 1000.0);
    for (int channel = 0; channel < 16; ++channel)
        adl_setChannelEnabled(playback.device, channel, playback.gain > 0.0f);
    playback.last_tick = playback.fade_start = SDL_GetTicks();
    playback.fade_end = playback.fade_start + fade_milliseconds;
    playback.fading_out = false;
    playback.applied_volume = -1;
    playback.volume(0.0f);
    playback.running = true;
    SDL_UnlockMutex(playback.mutex);
    if (!playback.thread)
        playback.thread = SDL_CreateThread(Playback::run, "OPL music", &playback);
    if (!playback.thread)
    {
        playback.running = false;
        printf("Music: Unable to start sequencer: %s\n", SDL_GetError());
        return false;
    }
    return true;
}

void song::stop(int fadeout_time)
{
    if (!m_playback->device)
        return;
    SDL_LockMutex(m_playback->mutex);
    m_playback->fade_start = SDL_GetTicks();
    m_playback->fade_end = m_playback->fade_start + (fadeout_time > 0 ? fadeout_time : fade_milliseconds);
    m_playback->fading_out = true;
    SDL_UnlockMutex(m_playback->mutex);
}

bool song::playing() const
{
    if (!m_playback->device)
        return false;
    SDL_LockMutex(m_playback->mutex);
    const bool running = m_playback->running;
    SDL_UnlockMutex(m_playback->mutex);
    return running;
}

void song::set_gain(float gain)
{
    m_gain = std::clamp(gain, 0.0f, 1.0f);
    if (!m_playback->device)
        return;
    SDL_LockMutex(m_playback->mutex);
    if ((m_playback->gain > 0.0f) != (m_gain > 0.0f))
    {
        // Silence the chip's release tails as well as new notes at zero gain.
        for (int channel = 0; channel < 16; ++channel)
            adl_setChannelEnabled(m_playback->device, channel, m_gain > 0.0f);
        if (m_gain == 0.0f)
        {
            const double position = adl_positionTell(m_playback->device);
            adl_reset(m_playback->device);
            adl_positionSeek(m_playback->device, position);
            m_playback->applied_volume = -1;
        }
        m_playback->volume(m_gain);
    }
    m_playback->gain = m_gain;
    SDL_UnlockMutex(m_playback->mutex);
}

bool song::reload()
{
    // The fixed OPL bank has no runtime SoundFont to reload.
    return m_playback->device != nullptr;
}
