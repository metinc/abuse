/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>

#include "common.h"

#include "game.h"

#include "keys.h"
#include "lisp.h"
#include "jwindow.h"
#include "configuration.h"

//AR
#include "sdlport/setup.h"
extern Settings settings;
//

extern int get_key_binding(char const *dir, int i);

int key_players = 0;
int morph_detail = MEDIUM_DETAIL;

struct player_keys
{
    int left, right, up, down, b1, b2, b3, b4;
    // Alternate keys to allow two key bindings for the same action
    int left_2, right_2, up_2, down_2;
};

static player_keys *key_map = NULL;

// AK
void get_key_bindings()
{
    if (key_map)
    {
        free(key_map);
    }
    key_map = NULL;

    key_players = 1;
    key_map = (player_keys *)malloc(sizeof(player_keys) * key_players);
    for (int i = 0; i < key_players; i++)
    {
#if !defined __CELLOS_LV2__
        key_map[i].left = get_key_binding("left", i + 1);
        key_map[i].left_2 = get_key_binding("left2", i + 1);
        key_map[i].right = get_key_binding("right", i + 1);
        key_map[i].right_2 = get_key_binding("right2", i + 1);
        key_map[i].up = get_key_binding("up", i + 1);
        key_map[i].up_2 = get_key_binding("up2", i + 1);
        key_map[i].down = get_key_binding("down", i + 1);
        key_map[i].down_2 = get_key_binding("down2", i + 1);
        key_map[i].b4 = get_key_binding("b4", i + 1);
        key_map[i].b3 = get_key_binding("b3", i + 1);
        key_map[i].b2 = get_key_binding("b2", i + 1);
        key_map[i].b1 = get_key_binding("b1", i + 1);
#else
        key_map[i].left = 258;
        key_map[i].left_2 = 258;
        key_map[i].right = 259;
        key_map[i].right_2 = 259;
        key_map[i].up = 256;
        key_map[i].up_2 = 256;
        key_map[i].down = 257;
        key_map[i].down_2 = 257;
        key_map[i].b4 = 281;
        key_map[i].b3 = 261;
        key_map[i].b2 = 32;
        key_map[i].b1 = 0;
#endif
    }
}

#define is_pressed(x) the_game->key_down(x)

void get_movement(int player, int &x, int &y, int &b1, int &b2, int &b3, int &b4)
{
    //AR in the middle of nowhere... here is the code to control the player via input states
    if (player < key_players)
    {
        bool left = is_pressed(key_map[player].left) || is_pressed(key_map[player].left_2);
        bool right = is_pressed(key_map[player].right) || is_pressed(key_map[player].right_2);
        if (left && right)
            x = 0; // opposing directions cancel out
        else if (left)
            x = -1;
        else if (right)
            x = 1;
        else
            x = 0;

        bool up = is_pressed(key_map[player].up) || is_pressed(key_map[player].up_2);
        bool down = is_pressed(key_map[player].down) || is_pressed(key_map[player].down_2);
        if (up)
            y = -1;
        else if (down)
            y = 1;
        else
            y = 0;

        if (is_pressed(key_map[player].b1))
            b1 = 1;
        else
            b1 = 0;

        if (is_pressed(key_map[player].b2))
            b2 = 1;
        else
            b2 = 0;

        if (is_pressed(key_map[player].b3))
            b3 = 1;
        else
            b3 = 0;

        if (is_pressed(key_map[player].b4))
            b4 = 1;
        else
            b4 = 0;
    }
    else
    {
        // FIXME: Why not b4?
        x = y = b1 = b2 = b3 = 0;
    }
}

/*
This doesn't appear to be used
void key_bindings(int player, int &left, int &right, int &up, int &down, int &b1, int &b2, int &b3, int &b4)
{
    left = key_map[player].left;
    right = key_map[player].right;
    up = key_map[player].up;
    down = key_map[player].down;
    b1 = key_map[player].b1;
    b2 = key_map[player].b2;
    b3 = key_map[player].b3;
    b3 = key_map[player].b4;
}
*/

void config_cleanup()
{
    if (key_map)
    {
        free(key_map);
        key_map = NULL;
    }
}

//
// Get the keycode for the string 'str'
// Returns -1 for an invalid key code
//
int get_keycode(char const *str)
{
    if (!str[0])
    {
        return -1;
    }
    else if (!str[1])
    {
        return str[0];
    }
    else
    {
        int j;
        char buf[20];
        for (j = 256; j < JK_MAX_KEY; j++)
        {
            key_name(j, buf);
            char *c = buf;
            for (; *c; c++)
            {
                if (*c == ' ')
                {
                    *c = '_';
                }
                else
                {
                    *c = tolower(*c);
                }
            }
            if (strcmp(str, buf) == 0)
            {
                return j;
            }
        }
    }
    return -1;
}
