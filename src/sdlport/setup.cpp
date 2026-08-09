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

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <signal.h>
#include <cstring>
#include <string>
#include <filesystem>
#include <limits>
#include <SDL3/SDL.h>

#include "file_utils.h"
#include "specs.h"
#include "keys.h"
#include "setup.h"
#include "errorui.h"

//AR
#include <fstream>
#include <sstream>
extern Settings settings;
//

extern int xres, yres; //video.cpp
extern int sfx_volume, music_volume; //loader.cpp
unsigned int scale; //AR was static, removed for external

const char *config_filename = "config.txt";

namespace
{
std::string append_path(const char *base, const char *relative)
{
    std::string result = base ? base : "";
    if (!relative || !relative[0] || std::strcmp(relative, ".") == 0)
        return result;

    if (!result.empty() && result.back() != '/' && result.back() != '\\')
        result += '/';
    result += relative;
    return result;
}

bool is_abuse_data_directory(const std::string &path)
{
    const std::string marker = append_path(path.c_str(), "abuse.lsp");
    SDL_PathInfo info;
    return SDL_GetPathInfo(marker.c_str(), &info) && info.type == SDL_PATHTYPE_FILE;
}

std::string find_data_directory()
{
    if (const char *override_path = SDL_getenv("ABUSE_PATH"))
        return override_path;

    if (const char *base_path = SDL_GetBasePath())
    {
        std::string relative_path = append_path(base_path, ABUSE_ASSETDIR_FROM_BASE);
        if (is_abuse_data_directory(relative_path))
            return relative_path;
    }

    if (is_abuse_data_directory(ASSETDIR))
        return ASSETDIR;

    if (is_abuse_data_directory(ABUSE_DEVELOPMENT_ASSETDIR))
        return ABUSE_DEVELOPMENT_ASSETDIR;

    return ASSETDIR;
}
}

bool AR_GetAttr(std::string line, std::string &attr, std::string &value)
{
    attr = value = "";

    std::size_t found = line.find("=");

    // no "=" or no attribute name
    if (found == std::string::npos || found == 0)
        return false;

    attr = line.substr(0, found);
    value = line.substr(found + 1);

    // Empty values are valid for optional string settings such as soundfont.
    return !attr.empty();
}

Settings::Settings()
{
    //screen
    this->fullscreen = 2;
    this->borderless = false;
    this->xres = 320; // default window width
    this->yres = 200; // default window height
    this->aspect_ratio = ""; // empty uses the desktop aspect ratio
    this->scale = 2; // default window scale
    this->linear_filter = false; // don't "anti-alias"
    this->hires = 0;

    //sound
    this->mono = false; // disable stereo sound
    this->no_sound = false; // disable sound
    this->no_music = false; // disable music
    this->volume_sound = 127;
    this->volume_music = 127;
    this->soundfont = ""; // Empty = don't use custom soundfont

    //random
    this->local_save = false;
    this->grab_input = false; // don't grab the input
    this->editor = false; // disable editor mode
    this->physics_update = 65; // original 65ms/15 FPS
    this->max_fps = 300;
    this->mouse_scale = 0; // match desktop
    this->big_font = false;
    this->language = "english";
    //
    this->player_touching_console = false;

    this->cheat_god = false;
    this->skip_intro = false;

    //player controls
    this->up = key_value("w");
    this->down = key_value("s");
    this->left = key_value("a");
    this->right = key_value("d");
    this->up_2 = key_value("UP");
    this->down_2 = key_value("DOWN");
    this->left_2 = key_value("LEFT");
    this->right_2 = key_value("RIGHT");
    this->b1 = key_value("SHIFT_L"); //special
    this->b2 = key_value("f"); //fire
    this->b3 = key_value("q"); //weapons
    this->b4 = key_value("e");

    //controller settings
    this->ctr_aim_correctx = 0;
    this->ctr_cd = 90;
    this->ctr_rst_s = 10;
    this->ctr_rst_dz = 5000; // aiming
    this->ctr_lst_dzx = 10000; // move left right
    this->ctr_lst_dzy = 25000; // up/jump, down/use
    this->ctr_aim_x = 0;
    this->ctr_aim_y = 0;
    this->ctr_mouse_x = 0;
    this->ctr_mouse_y = 0;

    //controller buttons
    this->ctr_a = "up";
    this->ctr_b = "down";
    this->ctr_x = "b3";
    this->ctr_y = "b4";
    //
    this->ctr_lst = "b1";
    this->ctr_rst = "down";
    //
    this->ctr_lsr = "b2";
    this->ctr_rsr = "b3";
    //
    this->ctr_ltg = "b1";
    this->ctr_rtg = "b2";
    //
    this->ctr_f5 = 0;
    this->ctr_f9 = 0;
}

//////////
////////// CREATE DEFAULT "config.txt" FILE
//////////

bool Settings::CreateConfigFile()
{
    const std::filesystem::path prefix = get_save_filename_prefix();
    const std::filesystem::path file_path = prefix / config_filename;
    std::ofstream out(file_path);
    if (!out.is_open())
    {
        std::string tmp = "ERROR - CreateConfigFile() - Failed to create \"" + file_path.string() + "\"\n";
        printf(tmp.c_str());

        return false;
    }

    out << "; Abuse configuration file (v" << PACKAGE_VERSION << ")" << std::endl;
    out << std::endl;
    //
    out << "; SCREEN SETTINGS" << std::endl;
    out << std::endl;
    out << ";0 - window, 1 - borderless desktop fullscreen, 2 - exclusive fullscreen" << std::endl;
    out << "fullscreen=" << this->fullscreen << std::endl;
    out << "borderless=" << this->borderless << std::endl;
    out << std::endl;
    out << "; Display aspect ratio (for example 16:9). Empty matches the desktop." << std::endl;
    out << "; The original aspect ratio was 4:3. Use custom to select the explicit" << std::endl;
    out << "; screen_width/screen_height values below." << std::endl;
    out << "aspect_ratio=" << this->aspect_ratio << std::endl;
    out << std::endl;
    out << "; Explicit game framebuffer size (original 320x200)" << std::endl;
    out << "screen_width=" << this->xres << std::endl;
    out << "screen_height=" << this->yres << std::endl;
    out << std::endl;
    out << "; Scale window" << std::endl;
    out << "scale=" << this->scale << std::endl;
    out << std::endl;
    out << "; Enable high resolution screens, buttons and font" << std::endl;
    out << "hires=" << this->hires << std::endl;
    out << "big_font=" << this->big_font << std::endl;
    out << std::endl;
    out << "; Language selection (english, german, french)\n" << std::endl;
    out << "language=" << this->language.c_str() << std::endl;
    out << std::endl;
    out << "; Use linear texture filter (nearest is default)" << std::endl;
    out << "linear_filter=" << this->linear_filter << std::endl;
    out << std::endl;
    //
    out << "; SOUND SETTINGS" << std::endl;
    out << std::endl;
    out << "; Volume (0-127)" << std::endl;
    out << "volume_sound=" << this->volume_sound << std::endl;
    out << "volume_music=" << this->volume_music << std::endl;
    out << std::endl;
    out << "; Disable music and sound effects" << std::endl;
    out << "no_music=" << this->no_music << std::endl;
    out << "no_sound=" << this->no_sound << std::endl;
    out << std::endl;
    out << "; Use mono audio only" << std::endl;
    out << "mono=" << this->mono << std::endl;
    out << "; Path to custom soundfont file (optional)\n" << std::endl;
    out << "soundfont=" << this->soundfont.c_str() << std::endl;
    out << std::endl;
    //
    out << "; MISCELLANEOUS SETTINGS" << std::endl;
    out << std::endl;
    out << "; Enable editor mode" << std::endl;
    out << "editor=" << this->editor << std::endl;
    out << std::endl;
    out << "; Grab the mouse and keyboard to the window" << std::endl;
    out << "grab_input=" << this->grab_input << std::endl;
    out << std::endl;
    out << "; Fullscreen mouse scaling (0 - match desktop, 1 - match game screen)" << std::endl;
    out << "mouse_scale=" << this->mouse_scale << std::endl;
    out << std::endl;
    out << "; Physics update time in ms (65ms/15FPS original)" << std::endl;
    out << "physics_update=" << this->physics_update << std::endl;
    out << std::endl;
    out << "; Max frames per second" << std::endl;
    out << "max_fps=" << this->max_fps << std::endl;
    out << std::endl;
    out << "local_save=" << this->local_save << std::endl;
    out << std::endl;
    out << "skip_intro=" << this->skip_intro << std::endl;
    out << std::endl;
    //
    out << "; PLAYER CONTROLS" << std::endl;
    out << std::endl;
    out << "; Key mappings" << std::endl;
    out << "left=a" << std::endl;
    out << "right=d" << std::endl;
    out << "up=w" << std::endl;
    out << "down=s" << std::endl;
    out << "special=SHIFT_L" << std::endl;
    out << "fire=f" << std::endl;
    out << "weapon_prev=q" << std::endl;
    out << "weapon_next=e" << std::endl;
    out << std::endl;
    //
    out << "; Alternative key mappings (only the following controls can have two keyboard bindings)" << std::endl;
    out << "left_2=LEFT" << std::endl;
    out << "right_2=RIGHT" << std::endl;
    out << "up_2=UP" << std::endl;
    out << "down_2=DOWN" << std::endl;
    out << std::endl;
    //
    out << "; CONTROLLER SETTINGS" << std::endl;
    out << std::endl;
    out << "; Correct crosshair position (x)" << std::endl;
    out << "ctr_aim_x=" << this->ctr_aim_correctx << std::endl;
    out << std::endl;
    out << "; Crosshair distance from player" << std::endl;
    out << "ctr_cd=" << this->ctr_cd << std::endl;
    out << std::endl;
    out << "; Right stick/aiming sensitivity" << std::endl;
    out << "ctr_rst_s=" << this->ctr_rst_s << std::endl;
    out << std::endl;
    out << "; Right stick/aiming dead zone" << std::endl;
    out << "ctr_rst_dz=" << this->ctr_rst_dz << std::endl;
    out << std::endl;
    out << "; Left stick/movement dead zones" << std::endl;
    out << "ctr_lst_dzx=" << this->ctr_lst_dzx << std::endl;
    out << "ctr_lst_dzy=" << this->ctr_lst_dzy << std::endl;
    out << std::endl;
    //
    out << "; Button mappings (don't use buttons for left/right movement)" << std::endl;
    out << "up=ctr_a" << std::endl;
    out << "down=ctr_b" << std::endl;
    out << "special=ctr_left_shoulder" << std::endl;
    out << "special=ctr_left_trigger" << std::endl;
    out << "fire=ctr_right_shoulder" << std::endl;
    out << "fire=ctr_right_trigger" << std::endl;
    out << "weapon_prev=ctr_x" << std::endl;
    out << "weapon_next=ctr_y" << std::endl;
    out << "quick_save=none" << std::endl;
    out << "quick_load=none" << std::endl;

    out.close();

    printf("Default \"config.txt\" created\n");

    return true;

    /*
	#if !((defined __APPLE__) || (defined WIN32))
	fputs( "; Location of the datafiles\ndatadir=", fd );
	fputs( ASSETDIR "\n\n", fd );
	#endif	
	*/
}

//////////
////////// READ CONFIG FILE
//////////

bool Settings::ReadConfigFile()
{
    const std::filesystem::path prefix = get_save_filename_prefix();
    const std::filesystem::path file_path = prefix / config_filename;

    std::ifstream filein(file_path.c_str());
    if (!filein.is_open())
    {
        std::string tmp = "ERROR - ReadConfigFile() - Failed to open \"" + file_path.string() + "\"\n";
        printf(tmp.c_str());

        //try to create it
        return CreateConfigFile();
    }

    std::string line;
    while (std::getline(filein, line))
    {
        //stop reading file
        if (line == "exit")
        {
            filein.close();
            return true;
        }

        //skip empty line or ";" which marks a comment
        if (line.empty() || line[0] == ';')
            continue;

        std::string attr, value;

        //skip if invalid command
        if (!AR_GetAttr(line, attr, value))
        {
            printf("Ignoring invalid command \"%s\" in %s\n", line.c_str(), file_path.c_str());
            continue;
        }

        try
        {
            //screen
            if (attr == "fullscreen")
                this->fullscreen = std::stoi(value);
            else if (attr == "borderless")
                this->borderless = (value == "1");
            else if (attr == "aspect_ratio")
                this->aspect_ratio = value;
            else if (attr == "screen_width")
                this->xres = std::stoi(value);
            else if (attr == "screen_height")
                this->yres = std::stoi(value);
            else if (attr == "scale")
                this->scale = std::stoi(value);
            else if (attr == "linear_filter")
                this->linear_filter = (value == "1");
            else if (attr == "hires")
                this->hires = std::stoi(value);
            else if (attr == "max_fps")
                this->max_fps = std::stoi(value);
            //sound
            else if (attr == "mono")
                this->mono = (value == "1");
            else if (attr == "no_sound")
                this->no_sound = (value == "1");
            else if (attr == "no_music")
                this->no_music = (value == "1");
            else if (attr == "volume_sound")
                this->volume_sound = std::stoi(value);
            else if (attr == "volume_music")
                this->volume_music = std::stoi(value);
            else if (attr == "soundfont")
                this->soundfont = value;

            //random
            else if (attr == "local_save")
                this->local_save = (value == "1");
            else if (attr == "grab_input")
                this->grab_input = (value == "1");
            else if (attr == "editor")
                this->editor = (value == "1");
            else if (attr == "physics_update")
                this->physics_update = std::stoi(value);
            else if (attr == "mouse_scale")
                this->mouse_scale = std::stoi(value);
            else if (attr == "big_font")
                this->big_font = (value == "1");
            else if (attr == "language")
                this->language = value;
            else if (attr == "skip_intro")
                this->skip_intro = (value == "1");

            //player controls
            else if (attr == "up")
            {
                if (!ControllerButton(attr, value))
                    this->up = key_value(value.c_str());
            }
            else if (attr == "down")
            {
                if (!ControllerButton(attr, value))
                    this->down = key_value(value.c_str());
            }
            else if (attr == "left")
            {
                if (!ControllerButton(attr, value))
                    this->left = key_value(value.c_str());
            }
            else if (attr == "right")
            {
                if (!ControllerButton(attr, value))
                    this->right = key_value(value.c_str());
            }
            //
            else if (attr == "special")
            {
                if (!ControllerButton(attr, value))
                    this->b1 = key_value(value.c_str());
            }
            else if (attr == "fire")
            {
                if (!ControllerButton(attr, value))
                    this->b2 = key_value(value.c_str());
            }
            else if (attr == "weapon_prev")
            {
                if (!ControllerButton(attr, value))
                    this->b3 = key_value(value.c_str());
            }
            else if (attr == "weapon_next")
            {
                if (!ControllerButton(attr, value))
                    this->b4 = key_value(value.c_str());
            }
            //
            else if (attr == "up_2")
                this->up_2 = key_value(value.c_str());
            else if (attr == "down_2")
                this->down_2 = key_value(value.c_str());
            else if (attr == "left_2")
                this->left_2 = key_value(value.c_str());
            else if (attr == "right_2")
                this->right_2 = key_value(value.c_str());

            //controller settings
            else if (attr == "ctr_aim_x")
                this->ctr_aim_correctx = std::stoi(value);
            else if (attr == "ctr_cd")
                this->ctr_cd = std::stoi(value);
            else if (attr == "ctr_rst_s")
                this->ctr_rst_s = std::stoi(value);
            else if (attr == "ctr_rst_dz")
                this->ctr_rst_dz = std::stoi(value);
            else if (attr == "ctr_lst_dzx")
                this->ctr_lst_dzx = std::stoi(value);
            else if (attr == "ctr_lst_dzy")
                this->ctr_lst_dzy = std::stoi(value);
            else if (attr == "quick_save" || attr == "quick_load")
            {
                int b = 0;

                if (value == "ctr_a")
                    b = SDL_GAMEPAD_BUTTON_SOUTH;
                else if (value == "ctr_b")
                    b = SDL_GAMEPAD_BUTTON_EAST;
                else if (value == "ctr_x")
                    b = SDL_GAMEPAD_BUTTON_WEST;
                else if (value == "ctr_y")
                    b = SDL_GAMEPAD_BUTTON_NORTH;
                else if (value == "ctr_left_stick")
                    b = SDL_GAMEPAD_BUTTON_LEFT_STICK;
                else if (value == "ctr_right_stick")
                    b = SDL_GAMEPAD_BUTTON_RIGHT_STICK;
                else if (value == "ctr_left_shoulder")
                    b = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
                else if (value == "ctr_right_shoulder")
                    b = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;

                if (attr == "quick_save")
                    this->ctr_f5 = b;
                else
                    this->ctr_f9 = b;
            }
            else
            {
                printf("Ignoring unknown command \"%s\" in %s\n", line.c_str(), file_path.c_str());
                continue;
            }
        }
        catch (const std::exception &e)
        {
            printf("Ignoring invalid value \"%s\" in %s\n", line.c_str(), file_path.c_str());
            continue;
        }
    }

    filein.close();

    return true;

    /*
	if( strcasecmp( result, "datadir" ) == 0 )
	{
	result = strtok( NULL, "\n" );
	set_filename_prefix( result );
	}
	*/
}

bool Settings::ControllerButton(std::string c, std::string b)
{
    std::string control = c;

    if (c == "special")
        control = "b1";
    else if (c == "fire")
        control = "b2";
    else if (c == "weapon_prev")
        control = "b3";
    else if (c == "weapon_next")
        control = "b4";

    if (b == "ctr_a")
    {
        this->ctr_a = control;
        return true;
    };
    if (b == "ctr_b")
    {
        this->ctr_b = control;
        return true;
    };
    if (b == "ctr_x")
    {
        this->ctr_x = control;
        return true;
    };
    if (b == "ctr_y")
    {
        this->ctr_y = control;
        return true;
    };
    //
    if (b == "ctr_left_stick")
    {
        this->ctr_lst = control;
        return true;
    };
    if (b == "ctr_right_stick")
    {
        this->ctr_rst = control;
        return true;
    };
    //
    if (b == "ctr_left_shoulder")
    {
        this->ctr_lsr = control;
        return true;
    };
    if (b == "ctr_right_shoulder")
    {
        this->ctr_rsr = control;
        return true;
    };
    //
    if (b == "ctr_left_trigger")
    {
        this->ctr_ltg = control;
        return true;
    };
    if (b == "ctr_right_trigger")
    {
        this->ctr_rtg = control;
        return true;
    };

    return false;
}

//
// Display help
//
void showHelp(const char *executableName)
{
    printf("\n");
    printf("Usage: %s [options]\n", executableName);
    printf("Options:\n\n");
    printf("** Abuse Options **\n");
    printf("  -size <arg>       Set the size of the screen\n");
    printf("  -edit             Startup in editor mode\n");
    printf("  -a <arg>          Use addon named <arg>\n");
    printf("  -f <arg>          Load map file named <arg>\n");
    printf("  -lisp             Startup in lisp interpreter mode\n");
    printf("  -nodelay          Run at maximum speed\n");
    printf("\n");
    printf("** Abuse-SDL Options **\n");
    printf("  -datadir <arg>    Set the location of the game data to <arg>\n");
    printf("  -fullscreen       Enable borderless desktop fullscreen mode\n");
    printf("  -aspect <w:h>     Set display aspect ratio without changing gameplay DPI\n");
    printf("  -antialias        Enable anti-aliasing\n");
    printf("  -h, --help        Display this text\n");
    printf("  -mono             Disable stereo sound\n");
    printf("  -nosound          Disable sound\n");
    printf("  -scale <arg>      Scale to <arg>\n");
    printf("  -soundfont <arg>  Use soundfont file <arg> for MIDI playback\n");
    // printf( "  -x <arg>          Set the width to <arg>\n" );
    // printf( "  -y <arg>          Set the height to <arg>\n" );
}

//
// Parse the command-line parameters
//
void parseCommandLine(int argc, char **argv)
{
    // Command-line arguments override settings from config file

    for (int i = 1; i < argc; i++)
    {
        if (!SDL_strcasecmp(argv[i], "-remote_save"))
        {
            settings.local_save = false;
        }
        if (!SDL_strcasecmp(argv[i], "-fullscreen"))
        {
            settings.fullscreen = 1;
        }
        else if (!SDL_strcasecmp(argv[i], "-size"))
        {
            int width = 320;
            int height = 200;
            if (i + 1 < argc && !sscanf(argv[++i], "%d", &width))
            {
                width = 320;
            }
            if (i + 1 < argc && !sscanf(argv[++i], "%d", &height))
            {
                height = 200;
            }
            settings.xres = width;
            settings.yres = height;
            settings.aspect_ratio = "custom";
        }
        else if (!SDL_strcasecmp(argv[i], "-aspect"))
        {
            if (i + 1 < argc)
                settings.aspect_ratio = argv[++i];
        }
        else if (!SDL_strcasecmp(argv[i], "-nosound"))
        {
            settings.no_sound = 1;
        }
        else if (!SDL_strcasecmp(argv[i], "-antialias"))
        {
            settings.linear_filter = 1;
        }
        else if (!SDL_strcasecmp(argv[i], "-mono"))
        {
            settings.mono = 1;
        }
        else if (!SDL_strcasecmp(argv[i], "-datadir"))
        {
            char datadir[255];
            if (i + 1 < argc && sscanf(argv[++i], "%s", datadir))
            {
                set_filename_prefix(datadir);
            }
        }
        else if (!SDL_strcasecmp(argv[i], "-soundfont"))
        {
            if (i + 1 < argc)
            {
                settings.soundfont = argv[++i];
            }
        }
        else if (!SDL_strcasecmp(argv[i], "-h") || !SDL_strcasecmp(argv[i], "--help"))
        {
            showHelp(argv[0]);
            exit(EXIT_SUCCESS);
        }
    }
}

bool Settings::ApplyAspectRatio()
{
    if (!SDL_strcasecmp(aspect_ratio.c_str(), "custom"))
        return true;

    if (aspect_ratio.empty())
    {
        const SDL_DisplayID display = SDL_GetPrimaryDisplay();
        const SDL_DisplayMode *mode = display ? SDL_GetDesktopDisplayMode(display) : nullptr;
        if (mode && mode->w > 0 && mode->h > 0)
            aspect_ratio = std::to_string(mode->w) + ":" + std::to_string(mode->h);
        else
        {
            fprintf(stderr, "Video: Unable to determine desktop aspect ratio; using original 4:3\n");
            aspect_ratio = "4:3";
        }
    }

    const std::size_t separator = aspect_ratio.find(':');
    if (separator == std::string::npos || separator == 0 || separator + 1 == aspect_ratio.size() ||
        aspect_ratio.find(':', separator + 1) != std::string::npos)
        return false;

    int aspect_width;
    int aspect_height;
    try
    {
        std::size_t width_end;
        std::size_t height_end;
        aspect_width = std::stoi(aspect_ratio.substr(0, separator), &width_end);
        aspect_height = std::stoi(aspect_ratio.substr(separator + 1), &height_end);
        if (width_end != separator || height_end != aspect_ratio.size() - separator - 1)
            return false;
    }
    catch (const std::exception &)
    {
        return false;
    }

    if (aspect_width <= 0 || aspect_height <= 0)
        return false;

    constexpr int original_width = 320;
    constexpr int original_height = 200;
    constexpr int corrected_original_height = 240;

    // Compare against 4:3 without using floating point. Wider displays grow
    // the framebuffer horizontally; narrower displays grow it vertically.
    int64_t framebuffer_width;
    int64_t framebuffer_height;
    if (static_cast<int64_t>(aspect_width) * 3 >= static_cast<int64_t>(aspect_height) * 4)
    {
        const int64_t width = static_cast<int64_t>(corrected_original_height) * aspect_width;
        framebuffer_width = (width + aspect_height / 2) / aspect_height;
        framebuffer_height = original_height;
    }
    else
    {
        // Original VGA pixels are 5:6, so convert the required displayed
        // height back to framebuffer pixels after holding the width at 320.
        const int64_t height = static_cast<int64_t>(original_width) * 5 * aspect_height;
        framebuffer_width = original_width;
        framebuffer_height = (height + 3 * aspect_width) / (6 * aspect_width);
    }

    if (framebuffer_width < original_width || framebuffer_height < original_height ||
        framebuffer_width > std::numeric_limits<short>::max() || framebuffer_height > std::numeric_limits<short>::max())
        return false;

    xres = static_cast<short>(framebuffer_width);
    yres = static_cast<short>(framebuffer_height);
    return true;
}

//
// Setup SDL and configuration
//
void setup(int argc, char **argv)
{
    // Initialize SDL with video and audio support
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD))
    {
        show_startup_error("Unable to initialize SDL: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    atexit(SDL_Quit);

    char *prefPath = SDL_GetPrefPath("abuse", ".");

    if (prefPath == NULL)
    {
        printf("WARNING: Unable to get save directory path: %s\n", SDL_GetError());
        printf("         Savegames will use current directory.\n");
        set_save_filename_prefix("");
    }
    else
    {
        set_save_filename_prefix(prefPath);
        SDL_free(prefPath);
    }

    if (const char *save_override = SDL_getenv("ABUSE_SAVE_PATH"))
        set_save_filename_prefix(save_override);

    const std::string data_path = find_data_directory();
    set_filename_prefix(data_path.c_str());

    printf("Save path %s\n", get_save_filename_prefix());
    printf("Data path %s\n", get_filename_prefix());

    // Load the user's configuration file from the save directory
    settings.ReadConfigFile();

    // Process any command-line arguments that might override settings
    parseCommandLine(argc, argv);

    if (!settings.ApplyAspectRatio())
    {
        show_startup_error("Invalid aspect_ratio '%s'; expected positive values in w:h form",
                           settings.aspect_ratio.c_str());
        exit(EXIT_FAILURE);
    }
    if (!SDL_strcasecmp(settings.aspect_ratio.c_str(), "custom"))
        printf("Video: Using custom %dx%d framebuffer\n", settings.xres, settings.yres);
    else
        printf("Video: Aspect ratio %s uses a %dx%d framebuffer\n", settings.aspect_ratio.c_str(), settings.xres,
               settings.yres);

    // Initialize audio volumes from settings
    // These variables are defined externally in loader.cpp
    scale = settings.scale;
    xres = settings.xres;
    yres = settings.yres;
    sfx_volume = settings.volume_sound;
    music_volume = settings.volume_music;
}

int get_key_binding(char const *dir, int i)
{
    if (SDL_strcasecmp(dir, "left") == 0)
        return settings.left;
    else if (SDL_strcasecmp(dir, "right") == 0)
        return settings.right;
    else if (SDL_strcasecmp(dir, "up") == 0)
        return settings.up;
    else if (SDL_strcasecmp(dir, "down") == 0)
        return settings.down;
    else if (SDL_strcasecmp(dir, "left2") == 0)
        return settings.left_2;
    else if (SDL_strcasecmp(dir, "right2") == 0)
        return settings.right_2;
    else if (SDL_strcasecmp(dir, "up2") == 0)
        return settings.up_2;
    else if (SDL_strcasecmp(dir, "down2") == 0)
        return settings.down_2;
    else if (SDL_strcasecmp(dir, "b1") == 0)
        return settings.b1;
    else if (SDL_strcasecmp(dir, "b2") == 0)
        return settings.b2;
    else if (SDL_strcasecmp(dir, "b3") == 0)
        return settings.b3;
    else if (SDL_strcasecmp(dir, "b4") == 0)
        return settings.b4;

    return 0;
}

//AR controller
std::string get_ctr_binding(std::string c)
{
    if (c == "ctr_a")
        return settings.ctr_a;
    else if (c == "ctr_b")
        return settings.ctr_b;
    else if (c == "ctr_x")
        return settings.ctr_x;
    else if (c == "ctr_y")
        return settings.ctr_y;
    //
    else if (c == "ctr_lst")
        return settings.ctr_lst;
    else if (c == "ctr_rst")
        return settings.ctr_rst;
    //
    else if (c == "ctr_lsr")
        return settings.ctr_lsr;
    else if (c == "ctr_rsh")
        return settings.ctr_rsr;
    //
    else if (c == "ctr_ltg")
        return settings.ctr_ltg;
    else if (c == "ctr_rtg")
        return settings.ctr_rtg;

    return "";
}
