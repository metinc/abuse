/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack Dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#ifndef ABUSE_PLAYER_NAME_H
#define ABUSE_PLAYER_NAME_H

#include <cstddef>

constexpr int MAX_PLAYER_NAME_LENGTH = 18;
constexpr int MAX_SERVER_NAME_LENGTH = 18;

char const *get_login();
void set_login(char const *name);
void copy_player_name(char *destination, std::size_t destination_size, char const *source);

#endif
