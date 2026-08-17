/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#ifndef __SOUND_H__
#define __SOUND_H__

#include <string>

#include <SDL3_mixer/SDL_mixer.h>

#include "common.h"

bool sound_init();
void sound_uninit();
bool sound_is_initialized();
bool sound_set_soundfont(const std::string &configured_soundfont);

class sound_effect
{
  public:
    explicit sound_effect(char const *filename);
    ~sound_effect();

    sound_effect(const sound_effect &) = delete;
    sound_effect &operator=(const sound_effect &) = delete;

    void play(float gain = 1.0f, float frequency_ratio = 1.0f, int panpot = 128);

  private:
    MIX_Audio *m_audio;
};

class song
{
  public:
    explicit song(char const *filename);
    void play(float gain = 1.0f);
    void stop(int fadeout_time = 0); // milliseconds; zero uses a short default fade
    bool playing() const;
    void set_gain(float gain);
    bool reload();
    ~song();

    song(const song &) = delete;
    song &operator=(const song &) = delete;

  private:
    bool load();
    bool start_playback(Sint64 start_milliseconds = 0);

    std::string m_filename;
    MIX_Audio *m_audio;
    MIX_Track *m_track;
    float m_gain;
};

#endif
