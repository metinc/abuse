/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#ifndef _VIDEO_HPP_
#define _VIDEO_HPP_

#include "common.h"
#include "image.h"

struct SDL_Window;

extern int xres, yres;
extern image *main_screen;

SDL_Window *video_window();
void set_mode();
bool resize_framebuffer(int width, int height);
bool video_set_fullscreen(bool enabled);
void video_change_settings(int scale_add, bool toggle_fullscreen);
void video_update_mouse_confinement();
ivec2 video_window_to_game(float window_x, float window_y);
void video_warp_mouse(ivec2 position);
bool video_start_text_input();
void video_stop_text_input();
bool video_save_screenshot(char const *filename);
void close_framebuffer();
void close_graphics();

void update_dirty(image *im, int xoff = 0, int yoff = 0);

#endif
