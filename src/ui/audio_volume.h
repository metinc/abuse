/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_AUDIO_VOLUME_H
#define ABUSE_AUDIO_VOLUME_H

#include <algorithm>

constexpr int AUDIO_VOLUME_MAX_LEVEL = 15;

inline int audio_volume_level(float gain)
{
    return static_cast<int>(std::clamp(gain, 0.0f, 1.0f) * AUDIO_VOLUME_MAX_LEVEL + 0.5f);
}

inline float audio_volume_gain(int level)
{
    return static_cast<float>(std::clamp(level, 0, AUDIO_VOLUME_MAX_LEVEL)) / AUDIO_VOLUME_MAX_LEVEL;
}

inline float increase_audio_volume(float gain)
{
    return audio_volume_gain(audio_volume_level(gain) + 1);
}

inline float decrease_audio_volume(float gain)
{
    return audio_volume_gain(audio_volume_level(gain) - 1);
}

#endif
