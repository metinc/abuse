/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#ifndef __JOYSTICK_HPP_
#define __JOYSTICK_HPP_

#include <SDL3/SDL_gamepad.h>

bool joy_init();
void joy_handle_added(SDL_JoystickID id);
void joy_handle_removed(SDL_JoystickID id);

#endif
