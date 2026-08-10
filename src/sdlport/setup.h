/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *  Copyright (c) 2016 Antonio Radojkovic <antonior.software@gmail.com>
 *  Copyright (c) 2024 Andrej Pancik
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, by Sam Hocevar, or Andrej Pancik.
 */

#ifndef _SETUP_H_
#define _SETUP_H_

#include <string>

inline constexpr char DEFAULT_SOUNDFONT[] = "MuseScore.sf2";

class Settings
{
  public:
    //screen
    int fullscreen; //0 - window, 1 - borderless desktop fullscreen, 2 - exclusive fullscreen
    bool borderless; //borderless window
    short xres; //game screen resolution
    short yres;
    std::string aspect_ratio; //optional display aspect ratio; preserves the original gameplay DPI
    short scale; //windows scale
    bool linear_filter; //"antialias"
    int hires; //enable hires screens and icons

    //sound
    bool mono;
    bool no_sound;
    bool no_music;
    int volume_sound; //0-127
    int volume_music; //0-127
    std::string soundfont; // SoundFont filename or path

    //random
    bool local_save;
    bool grab_input; //lock the input to the window
    bool editor; //enable editor mode
    short physics_update; //custom pysics update time in miliseconds
    short max_fps; //max frames per seconds to avoid GPU hogging if vsync is off
    short mouse_scale; //mouse scaling in fullscreen, 0 - match desktop, 1 - match game screen
    bool big_font; //big font doesn't render properly (there are lines under letters and stuff)
    std::string language;
    //

    std::string quick_load; //quick load
    bool player_touching_console; //only allow quicksave if player is touching the console
    bool skip_intro;

    double gamma;

    //settings shared with the Lisp game layer
    std::string difficulty;

    //cheats
    bool cheat_god, cheat_bullettime;

    //player controls
    int left, right, up, down;
    int left_2, right_2, up_2, down_2;
    int b1; //special
    int b2; //fire
    int b3; //weapon prev
    int b4; //weapon next

    //controller settings
    int ctr_aim_correctx; //for some reason game adds black bars on widescreen resolutions and it messes up crosshair position
    int ctr_cd; //crosshair distance from player
    int ctr_rst_s; //right stick sensitivity
    int ctr_rst_dz, ctr_lst_dzx, ctr_lst_dzy; //dead zones
    //
    float ctr_aim_x, ctr_aim_y; //state of right stick
    float ctr_mouse_x, ctr_mouse_y; //use left stick to move mouse...gave up

    //controller buttons
    std::string ctr_a, ctr_b, ctr_x, ctr_y;
    std::string ctr_lst, ctr_rst; //stick buttons
    std::string ctr_lsr, ctr_rsr; //shoulder buttons
    std::string ctr_ltg, ctr_rtg; //trigger buttons

    int ctr_f5, ctr_f9;

    Settings();

    bool ApplyAspectRatio();
    bool Load();
    bool Save() const;
    void BeginCommandLineOverrides();
    void SetFullscreenMode(int mode);
    void SetSoundFont(const std::string &path);

  private:
    bool ReadTomlFile();
    void Validate();

    bool command_line_overrides = false;
    int file_fullscreen = 2;
    short file_xres = 320, file_yres = 200;
    std::string file_aspect_ratio;
    bool file_no_sound = false, file_linear_filter = false, file_mono = false;
    bool file_local_save = false, file_editor = false;
};

#endif // _SETUP_H_
