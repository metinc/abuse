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

#include "common.h"

#include "event.h"
#include "video.h"
#include "filter.h"
#include <SDL2/SDL_timer.h>

//
// Constructor
//
EventHandler::EventHandler(image *screen, palette *pal)
{
    //AR moved from event.h, increased from 5000 to 10000 because it didn't detect the release of left stick when moving
    m_dead_zone = 10000;
    m_right_stick_scale = 0x2000;
    m_right_stick_player_scale = 0x400;
    m_right_stick_x = -1; // Initialize to indicate mouse mode
    m_right_stick_y = -1; // Initialize to indicate mouse mode
    m_pending = 0;
    last_key = 0;
    m_ignore_wheel_events = false;
    m_button = 0;
    m_center = ivec2(0, 0);
    //

    CHECK(screen && pal);

    m_screen = screen;

    // Mouse stuff
    uint8_t mouse_sprite[] = {0, 2, 0, 0, 0, 0, 0, 0, 2, 1, 2, 0, 0, 0, 0, 0, 2, 1, 1, 2, 0, 0, 0, 0, 2, 1, 1,
                              1, 2, 0, 0, 0, 2, 1, 1, 1, 1, 2, 0, 0, 2, 1, 1, 1, 1, 1, 2, 0, 0, 2, 1, 1, 2, 2,
                              0, 0, 0, 0, 2, 1, 1, 2, 0, 0, 0, 0, 2, 1, 1, 2, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0};

    Filter f;
    f.Set(1, pal->brightest(1));
    f.Set(2, pal->darkest(1));
    image *im = new image(ivec2(8, 10), mouse_sprite);
    f.Apply(im);

    m_sprite = new Sprite(screen, im, ivec2(100, 100));
    m_pos = screen->Size() / 2;
    m_center = ivec2(0, 0);
    m_button = 0;

    // Platform-specific stuff
    SysInit();
}

//
// Destructor
//
EventHandler::~EventHandler()
{
    if (m_sprite)
        delete m_sprite;
}

void EventHandler::Get(Event &ev)
{
    // Sleep until there are events available
    while (!m_pending)
    {
        IsPending();

        if (!m_pending)
            SDL_Delay(1);
    }

    // Return first queued event if applicable
    Event *ep = (Event *)m_events.first();
    if (ep)
    {
        ev = *ep;
        m_events.unlink(ep);
        delete ep;
        m_pending = m_events.first() != NULL;
        return;
    }

    // Return an event from the platform-specific system
    SysEvent(ev);
}

//
// flush_screen()
// Redraw the screen
//
void EventHandler::flush_screen()
{
    update_dirty(main_screen);
}
