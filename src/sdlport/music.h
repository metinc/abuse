#ifndef ABUSE_MUSIC_H
#define ABUSE_MUSIC_H

#include <filesystem>
#include <SDL3_mixer/SDL_mixer.h>

// CMake selects the music implementation for the target platform.
bool music_init(MIX_Mixer *mixer);
// Called after destroying the mixer, once its callbacks have finished.
void music_uninit();
std::filesystem::path resolve_audio_path(const char *filename);

#endif
