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

#include <SDL3/SDL_timer.h>

#include "timing.h"

// Constructor
//
time_marker::time_marker()
{
    get_time();
}

//
// get_time()
// Get the current time
//
void time_marker::get_time()
{
    ticks = SDL_GetTicksNS();
}

//
// diff_time()
// Find the time difference
//
double time_marker::diff_time(time_marker *other)
{
    constexpr double nanoseconds_per_second = 1000000000.0;
    if (ticks >= other->ticks)
        return static_cast<double>(ticks - other->ticks) / nanoseconds_per_second;

    return -static_cast<double>(other->ticks - ticks) / nanoseconds_per_second;
}
