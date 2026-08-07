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

#include <SDL3_mixer/SDL_mixer.h>

#include "common.h"

/* options are passed via command line */

#define SFX_INITIALIZED 1
#define MUSIC_INITIALIZED 2

int sound_init(int argc, char **argv);
void sound_uninit();

class sound_effect
{
  public:
    sound_effect(char const *filename);
    ~sound_effect();

    void play(int volume = 127, int pitch = 128, int panpot = 128);

  private:
    MIX_Audio *m_audio;
};

class song
{
  public:
    song(char const *filename);
    void play(unsigned char volume = 127);
    void stop(long fadeout_time = 0); // time in ms
    int playing();
    void set_volume(int volume);
    ~song();

  private:
    MIX_Audio *m_audio;
    MIX_Track *m_track;
};

#endif
