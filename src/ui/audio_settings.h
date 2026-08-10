/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#include "dev.h"

#include <string>
#include <vector>

class AudioSettingsWindow
{
  private:
    Jwindow *m_window;
    std::vector<std::string> soundfont_values;
    std::vector<std::string> soundfont_labels;
    std::vector<char *> soundfont_label_ptrs;

  public:
    AudioSettingsWindow();

    void draw_music_vol();
    void draw_sfx_vol();
    std::string selected_soundfont();
    void select_soundfont(const std::string &soundfont);
    Jwindow *window() const
    {
        return m_window;
    }
};
