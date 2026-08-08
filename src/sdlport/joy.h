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

int joy_init(int argc, char **argv); // returns 0 if no joystick is available
int joy_handle_added(SDL_JoystickID id);
int joy_handle_removed(SDL_JoystickID id);
int joy_gamepad_count();
void joy_shutdown();
void joy_status(int &b1, int &b2, int &b3, int &xv, int &yv);
void joy_calibrate();

#endif
