/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 2001 Anthony Kruize <trandor@labyrinth.net.au>
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
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

#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>

#include <SDL3/SDL.h>
#include "joy.h"

namespace
{
std::unordered_map<SDL_JoystickID, SDL_Gamepad *> gamepads;
bool shutdown_registered = false;

const char *gamepad_name(SDL_JoystickID id)
{
    const char *name = SDL_GetGamepadNameForID(id);
    return name ? name : "Unknown gamepad";
}

bool open_gamepad(SDL_JoystickID id)
{
    if (gamepads.find(id) != gamepads.end())
        return true;

    SDL_Gamepad *gamepad = SDL_OpenGamepad(id);
    if (!gamepad)
    {
        std::fprintf(stderr, "Warning: Unable to open gamepad %s: %s\n", gamepad_name(id), SDL_GetError());
        return false;
    }

    gamepads.emplace(id, gamepad);
    const char *name = SDL_GetGamepadName(gamepad);
    std::printf("Gamepad connected: %d (%s)\n", id, name ? name : "Unknown gamepad");
    return true;
}
}

bool joy_init()
{
    if (!shutdown_registered)
    {
        std::atexit(joy_shutdown);
        shutdown_registered = true;
    }

    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    std::printf("%d gamepads on system\n", count);
    for (int i = 0; i < count; ++i)
        open_gamepad(ids[i]);
    SDL_free(ids);

    return !gamepads.empty();
}

void joy_handle_added(SDL_JoystickID id)
{
    open_gamepad(id);
}

void joy_handle_removed(SDL_JoystickID id)
{
    auto found = gamepads.find(id);
    if (found != gamepads.end())
    {
        const char *raw_name = SDL_GetGamepadName(found->second);
        const std::string name = raw_name ? raw_name : "Unknown gamepad";
        SDL_CloseGamepad(found->second);
        gamepads.erase(found);
        std::printf("Gamepad disconnected: %d (%s)\n", id, name.c_str());
    }
}

void joy_shutdown()
{
    for (const auto &entry : gamepads)
        SDL_CloseGamepad(entry.second);
    gamepads.clear();
}
