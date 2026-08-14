/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#ifndef __KEYS_HPP_
#define __KEYS_HPP_

constexpr int JK_NONE = -1;
constexpr int JK_BACKSPACE = 8;
constexpr int JK_TAB = 9;
constexpr int JK_ENTER = 13;
constexpr int JK_ESC = 27;
constexpr int JK_SPACE = 32;

constexpr int JK_UP = 256;
constexpr int JK_DOWN = 257;
constexpr int JK_LEFT = 258;
constexpr int JK_RIGHT = 259;
constexpr int JK_CTRL_L = 260;
constexpr int JK_CTRL_R = 261;
constexpr int JK_ALT_L = 262;
constexpr int JK_ALT_R = 263;
constexpr int JK_SHIFT_L = 264;
constexpr int JK_SHIFT_R = 265;
constexpr int JK_CAPS = 266;
constexpr int JK_NUM_LOCK = 267;
constexpr int JK_HOME = 268;
constexpr int JK_END = 269;
constexpr int JK_DEL = 270;
constexpr int JK_F1 = 271;
constexpr int JK_F2 = 272;
constexpr int JK_F3 = 273;
constexpr int JK_F4 = 274;
constexpr int JK_F5 = 275;
constexpr int JK_F6 = 276;
constexpr int JK_F7 = 277;
constexpr int JK_F8 = 278;
constexpr int JK_F9 = 279;
constexpr int JK_F10 = 280;
constexpr int JK_INSERT = 281;
constexpr int JK_PAGEUP = 282;
constexpr int JK_PAGEDOWN = 283;
constexpr int JK_COMMAND = 284;
constexpr int JK_MAX_KEY = JK_COMMAND;
constexpr int JK_KEY_COUNT = 512;

constexpr bool key_is_valid(int key)
{
    return key >= 0 && key < JK_KEY_COUNT;
}

// returns a ASCII string describing a key, i.e. "Up Arrow"
void key_name(int key, char *buffer);

// returns a value describing a key name
int key_value(char const *buffer);

#endif
