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
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <filesystem>
#include <limits>
#include <system_error>
#include <utility>
#include <toml.hpp>
#include <SDL3/SDL.h>

#include "file_utils.h"
#include "specs.h"
#include "keys.h"
#include "setup.h"
#include "errorui.h"

//AR
#include <fstream>
extern Settings settings;
//

extern int xres, yres; //video.cpp
extern float sfx_volume, music_volume; //loader.cpp
unsigned int scale; //AR was static, removed for external

const char *settings_filename = "settings.toml";

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

Settings::Settings()
{
    //screen
    this->fullscreen = true;
    this->borderless = false;
    this->xres = 320; // default window width
    this->yres = 200; // default window height
    this->editor_xres = 640;
    this->editor_yres = 400;
    this->aspect_ratio = ""; // empty uses the desktop aspect ratio
    this->scale = 2; // default window scale
    this->linear_filter = false; // don't "anti-alias"
    this->hires = 0;

    //sound
    this->mono = false; // disable stereo sound
    this->no_sound = false; // disable sound
    this->no_music = false; // disable music
    this->volume_sound = 1.0;
    this->volume_music = 1.0;
    this->soundfont = DEFAULT_SOUNDFONT;

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
    this->menu_demos = false;
    this->gamma = 1.0;
    this->difficulty = "hard";

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
    this->ctr_lsr = "b1";
    this->ctr_rsr = "b2";
    //
    this->ctr_ltg = "b1";
    this->ctr_rtg = "b2";
    //
    this->ctr_f5 = -1;
    this->ctr_f9 = -1;
}

namespace
{
using settings_document = toml::ordered_value;

std::filesystem::path settings_path(const char *filename)
{
    const char *prefix = get_save_filename_prefix();
    return std::filesystem::path(prefix ? prefix : "") / filename;
}

std::filesystem::path settings_template_path()
{
    const char *prefix = get_filename_prefix();
    return std::filesystem::path(prefix ? prefix : "") / "user" / settings_filename;
}

const settings_document *find_table(const settings_document &parent, const char *key)
{
    if (!parent.is_table() || !parent.contains(key))
        return nullptr;
    const settings_document &value = parent.at(key);
    return value.is_table() ? &value : nullptr;
}

const settings_document *find_value(const settings_document *table, const char *key)
{
    return table && table->is_table() && table->contains(key) ? &table->at(key) : nullptr;
}

void invalid_setting(const char *section, const char *key, const char *expected)
{
    fprintf(stderr, "Config: Ignoring %s.%s; expected %s\n", section, key, expected);
}

template <typename T>
void read_integer(const settings_document *table, const char *section, const char *key, T &target)
{
    const settings_document *node = find_value(table, key);
    if (!node)
        return;
    if (node->is_integer())
    {
        const int64_t value = node->as_integer();
        if (value < static_cast<int64_t>(std::numeric_limits<T>::min()) ||
            value > static_cast<int64_t>(std::numeric_limits<T>::max()))
            invalid_setting(section, key, "an in-range integer");
        else
            target = static_cast<T>(value);
    }
    else
        invalid_setting(section, key, "an integer");
}

void read_number(const settings_document *table, const char *section, const char *key, double &target)
{
    const settings_document *node = find_value(table, key);
    if (!node)
        return;
    if (node->is_floating())
        target = node->as_floating();
    else if (node->is_integer())
        target = static_cast<double>(node->as_integer());
    else
        invalid_setting(section, key, "a number");
}

void read_boolean(const settings_document *table, const char *section, const char *key, bool &target)
{
    const settings_document *node = find_value(table, key);
    if (!node)
        return;
    if (node->is_boolean())
        target = node->as_boolean();
    else
        invalid_setting(section, key, "true or false");
}

void read_string(const settings_document *table, const char *section, const char *key, std::string &target)
{
    const settings_document *node = find_value(table, key);
    if (!node)
        return;
    if (node->is_string())
        target = node->as_string();
    else
        invalid_setting(section, key, "a string");
}

bool parse_key(const std::string &name, int &value)
{
    if (name.size() == 1)
    {
        value = static_cast<unsigned char>(name[0]);
        return true;
    }

    static const char *valid_names[] = {
        "BACKSPACE", "TAB",   "ENTER",   "ESC",     "SPACE", "UP",       "DOWN", "LEFT", "RIGHT",  "CTRL_L", "CTRL_R",
        "ALT_L",     "ALT_R", "SHIFT_L", "SHIFT_R", "CAPS",  "NUM_LOCK", "HOME", "END",  "DEL",    "F1",     "F2",
        "F3",        "F4",    "F5",      "F6",      "F7",    "F8",       "F9",   "F10",  "INSERT", "PAGEUP", "PAGEDOWN",
    };
    for (const char *valid : valid_names)
    {
        if (!SDL_strcasecmp(name.c_str(), valid))
        {
            value = key_value(valid);
            return true;
        }
    }
    return false;
}

std::string key_string(int key)
{
    switch (key)
    {
    case JK_BACKSPACE:
        return "BACKSPACE";
    case JK_TAB:
        return "TAB";
    case JK_ENTER:
        return "ENTER";
    case JK_ESC:
        return "ESC";
    case JK_SPACE:
        return "SPACE";
    case JK_UP:
        return "UP";
    case JK_DOWN:
        return "DOWN";
    case JK_LEFT:
        return "LEFT";
    case JK_RIGHT:
        return "RIGHT";
    case JK_CTRL_L:
        return "CTRL_L";
    case JK_CTRL_R:
        return "CTRL_R";
    case JK_ALT_L:
        return "ALT_L";
    case JK_ALT_R:
        return "ALT_R";
    case JK_SHIFT_L:
        return "SHIFT_L";
    case JK_SHIFT_R:
        return "SHIFT_R";
    case JK_CAPS:
        return "CAPS";
    case JK_NUM_LOCK:
        return "NUM_LOCK";
    case JK_HOME:
        return "HOME";
    case JK_END:
        return "END";
    case JK_DEL:
        return "DEL";
    case JK_F1:
        return "F1";
    case JK_F2:
        return "F2";
    case JK_F3:
        return "F3";
    case JK_F4:
        return "F4";
    case JK_F5:
        return "F5";
    case JK_F6:
        return "F6";
    case JK_F7:
        return "F7";
    case JK_F8:
        return "F8";
    case JK_F9:
        return "F9";
    case JK_F10:
        return "F10";
    case JK_INSERT:
        return "INSERT";
    case JK_PAGEUP:
        return "PAGEUP";
    case JK_PAGEDOWN:
        return "PAGEDOWN";
    default:
        if (key > 0 && key < 256)
            return std::string(1, static_cast<char>(key));
        return "";
    }
}

void read_keys(const settings_document *table, const char *key, int &primary, int *secondary = nullptr)
{
    const settings_document *node = find_value(table, key);
    if (!node || !node->is_array())
    {
        if (node)
            invalid_setting("input.keyboard", key,
                            secondary ? "an array containing one or two keys" : "a one-key array");
        return;
    }
    const auto &values = node->as_array();
    if (values.empty() || values.size() > (secondary ? 2u : 1u))
    {
        invalid_setting("input.keyboard", key, secondary ? "an array containing one or two keys" : "a one-key array");
        return;
    }

    int parsed[2];
    for (size_t i = 0; i < values.size(); ++i)
    {
        if (!values[i].is_string() || !parse_key(values[i].as_string(), parsed[i]))
        {
            invalid_setting("input.keyboard", key, "valid key names");
            return;
        }
    }
    primary = parsed[0];
    if (secondary && values.size() == 2)
        *secondary = parsed[1];
}

std::string internal_action(const std::string &action)
{
    if (action == "special")
        return "b1";
    if (action == "fire")
        return "b2";
    if (action == "weapon_prev")
        return "b3";
    if (action == "weapon_next")
        return "b4";
    if (action == "up" || action == "down")
        return action;
    return "";
}

std::string external_action(const std::string &action)
{
    if (action == "b1")
        return "special";
    if (action == "b2")
        return "fire";
    if (action == "b3")
        return "weapon_prev";
    if (action == "b4")
        return "weapon_next";
    return action;
}

void read_gamepad_action(const settings_document *table, const char *key, std::string &target)
{
    const settings_document *node = find_value(table, key);
    if (!node)
        return;
    if (!node->is_string() || internal_action(node->as_string()).empty())
    {
        invalid_setting("input.gamepad", key, "a supported action name");
        return;
    }
    target = internal_action(node->as_string());
}

int gamepad_button(const std::string &name)
{
    if (name == "south")
        return SDL_GAMEPAD_BUTTON_SOUTH;
    if (name == "east")
        return SDL_GAMEPAD_BUTTON_EAST;
    if (name == "west")
        return SDL_GAMEPAD_BUTTON_WEST;
    if (name == "north")
        return SDL_GAMEPAD_BUTTON_NORTH;
    if (name == "left_stick")
        return SDL_GAMEPAD_BUTTON_LEFT_STICK;
    if (name == "right_stick")
        return SDL_GAMEPAD_BUTTON_RIGHT_STICK;
    if (name == "left_shoulder")
        return SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
    if (name == "right_shoulder")
        return SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
    if (name == "none")
        return -1;
    return -2;
}

std::string gamepad_button_name(int button)
{
    switch (button)
    {
    case SDL_GAMEPAD_BUTTON_SOUTH:
        return "south";
    case SDL_GAMEPAD_BUTTON_EAST:
        return "east";
    case SDL_GAMEPAD_BUTTON_WEST:
        return "west";
    case SDL_GAMEPAD_BUTTON_NORTH:
        return "north";
    case SDL_GAMEPAD_BUTTON_LEFT_STICK:
        return "left_stick";
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
        return "right_stick";
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
        return "left_shoulder";
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
        return "right_shoulder";
    default:
        return "none";
    }
}

void read_gamepad_button(const settings_document *table, const char *key, int &target)
{
    const settings_document *node = find_value(table, key);
    if (!node)
        return;
    const int button = node->is_string() ? gamepad_button(node->as_string()) : -2;
    if (button == -2)
        invalid_setting("input.gamepad", key, "a gamepad button name or 'none'");
    else
        target = button;
}

void merge_comments_and_unknown_values(settings_document &canonical, const settings_document &user)
{
    for (const std::string &comment : user.comments())
    {
        const auto existing = std::find(canonical.comments().begin(), canonical.comments().end(), comment);
        if (existing == canonical.comments().end())
            canonical.comments().push_back(comment);
    }

    if (!canonical.is_table() || !user.is_table())
        return;

    for (const auto &entry : user.as_table())
    {
        if (canonical.contains(entry.first))
            merge_comments_and_unknown_values(canonical.at(entry.first), entry.second);
        else
            canonical.as_table().emplace_back(entry.first, entry.second);
    }
}

settings_document document_for_save(const std::filesystem::path &user_path)
{
    settings_document document(toml::ordered_table{});
    const std::filesystem::path template_path = settings_template_path();
    const bool has_template = std::filesystem::exists(template_path);
    const bool has_user_document = std::filesystem::exists(user_path);

    if (has_template)
        document = toml::parse<toml::ordered_type_config>(template_path);
    else if (has_user_document)
        document = toml::parse<toml::ordered_type_config>(user_path);

    if (has_template && has_user_document)
    {
        const settings_document user = toml::parse<toml::ordered_type_config>(user_path);
        merge_comments_and_unknown_values(document, user);
    }
    return document;
}

settings_document &ensure_table(settings_document &parent, const char *key)
{
    if (!parent.is_table())
        parent = toml::ordered_table{};
    if (!parent.contains(key) || !parent.at(key).is_table())
        parent[key] = toml::ordered_table{};
    return parent.at(key);
}

template <typename T> void set_value(settings_document &table, const char *key, T value)
{
    table[key] = std::move(value);
}

void set_fixed_number(settings_document &table, const char *key, double value)
{
    table[key] = value;
    toml::floating_format_info &format = table.at(key).as_floating_fmt();
    format.fmt = toml::floating_format::fixed;
    format.prec = 2;
}

}

void Settings::Validate()
{
    auto clamp = [](auto &value, auto low, auto high, const char *name) {
        if (value < low || value > high)
        {
            value = std::max(low, std::min(value, high));
            fprintf(stderr, "Config: %s is out of range; using %lld\n", name, static_cast<long long>(value));
        }
    };
    clamp(xres, static_cast<short>(1), std::numeric_limits<short>::max(), "video.framebuffer_width");
    clamp(yres, static_cast<short>(1), std::numeric_limits<short>::max(), "video.framebuffer_height");
    clamp(editor_xres, static_cast<short>(1), std::numeric_limits<short>::max(),
          "video.editor_framebuffer_width");
    clamp(editor_yres, static_cast<short>(1), std::numeric_limits<short>::max(),
          "video.editor_framebuffer_height");
    clamp(scale, static_cast<short>(1), static_cast<short>(20), "video.window_scale");
    clamp(hires, 0, 2, "video.hires");
    if (!std::isfinite(gamma))
    {
        gamma = 1.0;
        fprintf(stderr, "Config: video.gamma is not finite; using 1.0\n");
    }
    else
        clamp(gamma, 0.5, 2.0, "video.gamma");
    auto validate_gain = [&clamp](double &gain, const char *name) {
        if (!std::isfinite(gain))
        {
            gain = 1.0;
            fprintf(stderr, "Config: %s is not finite; using 1.0\n", name);
        }
        else
            clamp(gain, 0.0, 1.0, name);
    };
    validate_gain(volume_sound, "audio.sound_volume");
    validate_gain(volume_music, "audio.music_volume");
    clamp(physics_update, static_cast<short>(1), std::numeric_limits<short>::max(), "gameplay.physics_tick_ms");
    clamp(max_fps, static_cast<short>(1), std::numeric_limits<short>::max(), "gameplay.max_fps");
    clamp(mouse_scale, static_cast<short>(0), static_cast<short>(1), "input.mouse_scale");
    clamp(ctr_rst_dz, 0, 32767, "input.gamepad.aim_dead_zone");
    clamp(ctr_lst_dzx, 0, 32767, "input.gamepad.move_dead_zone_x");
    clamp(ctr_lst_dzy, 0, 32767, "input.gamepad.move_dead_zone_y");
    if (difficulty != "easy" && difficulty != "medium" && difficulty != "hard" && difficulty != "extreme")
    {
        fprintf(stderr, "Config: gameplay.difficulty is invalid; using hard\n");
        difficulty = "hard";
    }
}

bool Settings::ReadTomlFile()
{
    const std::filesystem::path path = settings_path(settings_filename);
    try
    {
        const settings_document document = toml::parse<toml::ordered_type_config>(path);
        const settings_document *version = find_value(&document, "schema_version");
        if (version && version->is_integer() && version->as_integer() > 3)
        {
            fprintf(stderr, "Config: %s uses unsupported schema version %lld\n", path.string().c_str(),
                    static_cast<long long>(version->as_integer()));
            return false;
        }

        const settings_document *video = find_table(document, "video");
        read_boolean(video, "video", "fullscreen", fullscreen);
        read_boolean(video, "video", "borderless", borderless);
        read_string(video, "video", "aspect_ratio", aspect_ratio);
        if (aspect_ratio == "desktop")
            aspect_ratio.clear();
        read_integer(video, "video", "framebuffer_width", xres);
        read_integer(video, "video", "framebuffer_height", yres);
        read_integer(video, "video", "editor_framebuffer_width", editor_xres);
        read_integer(video, "video", "editor_framebuffer_height", editor_yres);
        read_integer(video, "video", "window_scale", scale);
        read_boolean(video, "video", "linear_filter", linear_filter);
        read_integer(video, "video", "hires", hires);
        read_boolean(video, "video", "big_font", big_font);
        read_number(video, "video", "gamma", gamma);

        const settings_document *audio = find_table(document, "audio");
        bool enabled = !no_sound;
        read_boolean(audio, "audio", "sound_enabled", enabled);
        no_sound = !enabled;
        enabled = !no_music;
        read_boolean(audio, "audio", "music_enabled", enabled);
        no_music = !enabled;
        read_boolean(audio, "audio", "mono", mono);
        read_number(audio, "audio", "sound_volume", volume_sound);
        read_number(audio, "audio", "music_volume", volume_music);
        read_string(audio, "audio", "soundfont", soundfont);

        const settings_document *gameplay = find_table(document, "gameplay");
        read_string(gameplay, "gameplay", "difficulty", difficulty);
        read_integer(gameplay, "gameplay", "physics_tick_ms", physics_update);
        read_integer(gameplay, "gameplay", "max_fps", max_fps);
        read_boolean(gameplay, "gameplay", "skip_intro", skip_intro);
        read_boolean(gameplay, "gameplay", "menu_demos", menu_demos);

        const settings_document *general = find_table(document, "general");
        read_string(general, "general", "language", language);
        read_boolean(general, "general", "editor", editor);
        read_boolean(general, "general", "grab_input", grab_input);
        read_boolean(general, "general", "local_save", local_save);

        const settings_document *input = find_table(document, "input");
        read_integer(input, "input", "mouse_scale", mouse_scale);
        const settings_document *keyboard = input ? find_table(*input, "keyboard") : nullptr;
        read_keys(keyboard, "left", left, &left_2);
        read_keys(keyboard, "right", right, &right_2);
        read_keys(keyboard, "up", up, &up_2);
        read_keys(keyboard, "down", down, &down_2);
        read_keys(keyboard, "special", b1);
        read_keys(keyboard, "fire", b2);
        read_keys(keyboard, "weapon_prev", b3);
        read_keys(keyboard, "weapon_next", b4);

        const settings_document *gamepad = input ? find_table(*input, "gamepad") : nullptr;
        read_integer(gamepad, "input.gamepad", "aim_correction_x", ctr_aim_correctx);
        read_integer(gamepad, "input.gamepad", "crosshair_distance", ctr_cd);
        read_integer(gamepad, "input.gamepad", "aim_sensitivity", ctr_rst_s);
        read_integer(gamepad, "input.gamepad", "aim_dead_zone", ctr_rst_dz);
        read_integer(gamepad, "input.gamepad", "move_dead_zone_x", ctr_lst_dzx);
        read_integer(gamepad, "input.gamepad", "move_dead_zone_y", ctr_lst_dzy);
        read_gamepad_action(gamepad, "south", ctr_a);
        read_gamepad_action(gamepad, "east", ctr_b);
        read_gamepad_action(gamepad, "west", ctr_x);
        read_gamepad_action(gamepad, "north", ctr_y);
        read_gamepad_action(gamepad, "left_stick", ctr_lst);
        read_gamepad_action(gamepad, "right_stick", ctr_rst);
        read_gamepad_action(gamepad, "left_shoulder", ctr_lsr);
        read_gamepad_action(gamepad, "right_shoulder", ctr_rsr);
        read_gamepad_action(gamepad, "left_trigger", ctr_ltg);
        read_gamepad_action(gamepad, "right_trigger", ctr_rtg);
        read_gamepad_button(gamepad, "quick_save", ctr_f5);
        read_gamepad_button(gamepad, "quick_load", ctr_f9);

        Validate();
        return true;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Config: Unable to parse " << path << ":\n" << error.what() << '\n';
        return false;
    }
}

bool Settings::Load()
{
    const std::filesystem::path path = settings_path(settings_filename);
    if (std::filesystem::exists(path))
        return ReadTomlFile();

    Validate();
    if (!Save())
        return false;
    printf("Default \"settings.toml\" created\n");
    return true;
}

void Settings::BeginCommandLineOverrides()
{
    command_line_overrides = true;
    file_fullscreen = fullscreen;
    file_xres = xres;
    file_yres = yres;
    file_editor_xres = editor_xres;
    file_editor_yres = editor_yres;
    file_aspect_ratio = aspect_ratio;
    file_no_sound = no_sound;
    file_linear_filter = linear_filter;
    file_mono = mono;
    file_local_save = local_save;
    file_editor = editor;
}

void Settings::SetFullscreen(bool enabled)
{
    fullscreen = enabled;
    file_fullscreen = enabled;
}

void Settings::SetSoundFont(const std::string &path)
{
    soundfont = path;
}

bool Settings::Save() const
{
    const std::filesystem::path path = settings_path(settings_filename);
    std::string serialized;
    try
    {
        settings_document document = document_for_save(path);
        set_value(document, "schema_version", 3);

        settings_document &video = ensure_table(document, "video");
        const bool saved_fullscreen = command_line_overrides ? file_fullscreen : fullscreen;
        const short saved_xres = command_line_overrides ? file_xres : xres;
        const short saved_yres = command_line_overrides ? file_yres : yres;
        const short saved_editor_xres = command_line_overrides ? file_editor_xres : editor_xres;
        const short saved_editor_yres = command_line_overrides ? file_editor_yres : editor_yres;
        const std::string &saved_aspect_ratio = command_line_overrides ? file_aspect_ratio : aspect_ratio;
        set_value(video, "fullscreen", saved_fullscreen);
        set_value(video, "borderless", borderless);
        set_value(video, "aspect_ratio", saved_aspect_ratio.empty() ? std::string("desktop") : saved_aspect_ratio);
        set_value(video, "framebuffer_width", saved_xres);
        set_value(video, "framebuffer_height", saved_yres);
        set_value(video, "editor_framebuffer_width", saved_editor_xres);
        set_value(video, "editor_framebuffer_height", saved_editor_yres);
        set_value(video, "window_scale", scale);
        set_value(video, "linear_filter", command_line_overrides ? file_linear_filter : linear_filter);
        set_value(video, "hires", hires);
        set_value(video, "big_font", big_font);
        set_fixed_number(video, "gamma", gamma);

        settings_document &audio = ensure_table(document, "audio");
        set_value(audio, "sound_enabled", !(command_line_overrides ? file_no_sound : no_sound));
        set_value(audio, "music_enabled", !no_music);
        set_fixed_number(audio, "sound_volume", volume_sound);
        set_fixed_number(audio, "music_volume", volume_music);
        set_value(audio, "mono", command_line_overrides ? file_mono : mono);
        set_value(audio, "soundfont", soundfont);

        settings_document &gameplay = ensure_table(document, "gameplay");
        set_value(gameplay, "difficulty", difficulty);
        set_value(gameplay, "physics_tick_ms", physics_update);
        set_value(gameplay, "max_fps", max_fps);
        set_value(gameplay, "skip_intro", skip_intro);
        set_value(gameplay, "menu_demos", menu_demos);

        settings_document &general = ensure_table(document, "general");
        set_value(general, "language", language);
        set_value(general, "editor", command_line_overrides ? file_editor : editor);
        set_value(general, "grab_input", grab_input);
        set_value(general, "local_save", command_line_overrides ? file_local_save : local_save);

        settings_document &input = ensure_table(document, "input");
        set_value(input, "mouse_scale", mouse_scale);
        settings_document &keyboard = ensure_table(input, "keyboard");
        set_value(keyboard, "left", toml::ordered_array{key_string(left), key_string(left_2)});
        set_value(keyboard, "right", toml::ordered_array{key_string(right), key_string(right_2)});
        set_value(keyboard, "up", toml::ordered_array{key_string(up), key_string(up_2)});
        set_value(keyboard, "down", toml::ordered_array{key_string(down), key_string(down_2)});
        set_value(keyboard, "special", toml::ordered_array{key_string(b1)});
        set_value(keyboard, "fire", toml::ordered_array{key_string(b2)});
        set_value(keyboard, "weapon_prev", toml::ordered_array{key_string(b3)});
        set_value(keyboard, "weapon_next", toml::ordered_array{key_string(b4)});

        settings_document &gamepad = ensure_table(input, "gamepad");
        set_value(gamepad, "aim_correction_x", ctr_aim_correctx);
        set_value(gamepad, "crosshair_distance", ctr_cd);
        set_value(gamepad, "aim_sensitivity", ctr_rst_s);
        set_value(gamepad, "aim_dead_zone", ctr_rst_dz);
        set_value(gamepad, "move_dead_zone_x", ctr_lst_dzx);
        set_value(gamepad, "move_dead_zone_y", ctr_lst_dzy);
        set_value(gamepad, "south", external_action(ctr_a));
        set_value(gamepad, "east", external_action(ctr_b));
        set_value(gamepad, "west", external_action(ctr_x));
        set_value(gamepad, "north", external_action(ctr_y));
        set_value(gamepad, "left_stick", external_action(ctr_lst));
        set_value(gamepad, "right_stick", external_action(ctr_rst));
        set_value(gamepad, "left_shoulder", external_action(ctr_lsr));
        set_value(gamepad, "right_shoulder", external_action(ctr_rsr));
        set_value(gamepad, "left_trigger", external_action(ctr_ltg));
        set_value(gamepad, "right_trigger", external_action(ctr_rtg));
        set_value(gamepad, "quick_save", gamepad_button_name(ctr_f5));
        set_value(gamepad, "quick_load", gamepad_button_name(ctr_f9));

        serialized = toml::format(document);
        while (!serialized.empty() && (serialized.back() == '\n' || serialized.back() == '\r'))
            serialized.pop_back();
        serialized += '\n';
    }
    catch (const std::exception &error)
    {
        std::cerr << "Config: Unable to prepare " << path << ":\n" << error.what() << '\n';
        return false;
    }

    const std::filesystem::path temporary = path.string() + ".tmp";
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        std::cerr << "Config: Unable to write " << temporary << '\n';
        return false;
    }
    output << serialized;
    output.close();
    if (!output)
    {
        std::cerr << "Config: Unable to finish writing " << temporary << '\n';
        return false;
    }

    std::filesystem::rename(temporary, path, error);
    if (error)
    {
        std::error_code remove_error;
        std::filesystem::remove(path, remove_error);
        error.clear();
        std::filesystem::rename(temporary, path, error);
    }
    if (error)
    {
        std::cerr << "Config: Unable to replace " << path << ": " << error.message() << '\n';
        return false;
    }
    return true;
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
    // printf( "  -x <arg>          Set the width to <arg>\n" );
    // printf( "  -y <arg>          Set the height to <arg>\n" );
}

//
// Parse the command-line parameters
//
void parseCommandLine(int argc, char **argv)
{
    // Command-line arguments override settings from config file
    settings.BeginCommandLineOverrides();

    for (int i = 1; i < argc; i++)
    {
        if (!SDL_strcasecmp(argv[i], "-remote_save"))
        {
            settings.local_save = false;
        }
        if (!SDL_strcasecmp(argv[i], "-fullscreen"))
        {
            settings.fullscreen = true;
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
            settings.editor_xres = width;
            settings.editor_yres = height;
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
        else if (!SDL_strcasecmp(argv[i], "-h") || !SDL_strcasecmp(argv[i], "--help"))
        {
            showHelp(argv[0]);
            exit(EXIT_SUCCESS);
        }
    }
}

namespace
{
bool calculate_aspect_framebuffer(const std::string &aspect_ratio, short base_width, short base_height, short &width,
                                  short &height)
{
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

    // VGA pixels are 5:6. Grow one framebuffer dimension to reach the
    // requested display aspect ratio without changing the baseline zoom.
    int64_t framebuffer_width;
    int64_t framebuffer_height;
    if (static_cast<int64_t>(aspect_width) * 6 * base_height >=
        static_cast<int64_t>(aspect_height) * 5 * base_width)
    {
        const int64_t scaled_width = static_cast<int64_t>(base_height) * 6 * aspect_width;
        const int64_t divisor = static_cast<int64_t>(5) * aspect_height;
        framebuffer_width = (scaled_width + divisor / 2) / divisor;
        framebuffer_height = base_height;
    }
    else
    {
        const int64_t scaled_height = static_cast<int64_t>(base_width) * 5 * aspect_height;
        const int64_t divisor = static_cast<int64_t>(6) * aspect_width;
        framebuffer_width = base_width;
        framebuffer_height = (scaled_height + divisor / 2) / divisor;
    }

    if (framebuffer_width < base_width || framebuffer_height < base_height ||
        framebuffer_width > std::numeric_limits<short>::max() ||
        framebuffer_height > std::numeric_limits<short>::max())
        return false;

    width = static_cast<short>(framebuffer_width);
    height = static_cast<short>(framebuffer_height);
    return true;
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

    return calculate_aspect_framebuffer(aspect_ratio, 320, 200, xres, yres);
}

bool Settings::GetEditorFramebufferSize(short &width, short &height) const
{
    width = editor_xres;
    height = editor_yres;
    if (!SDL_strcasecmp(aspect_ratio.c_str(), "custom"))
        return true;
    return calculate_aspect_framebuffer(aspect_ratio, editor_xres, editor_yres, width, height);
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
    settings.Load();

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
    sfx_volume = static_cast<float>(settings.volume_sound);
    music_volume = static_cast<float>(settings.volume_music);
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
