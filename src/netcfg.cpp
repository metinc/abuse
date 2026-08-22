/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@zoy.org>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#include "netcfg.h"

#include <cstring>

extern char const *get_login();

net_configuration *main_net_cfg = nullptr;

net_configuration::net_configuration()
{
    strcpy(name, get_login());
    server_host[0] = '\0';
    room_code[0] = '\0';
    online = false;
    join_failed = false;

    min_players = 2;
    max_players = 8;
    kills = 25;
    port = 20202;
    server_port = 20202;
    state = SINGLE_PLAYER;
    game_mode = DEATHMATCH;
}

int net_configuration::restart_state()
{
    switch (state)
    {
    case RESTART_SERVER:
    case RESTART_CLIENT:
    case RESTART_SINGLE:
        return 1;
    default:
        return 0;
    }
}

int net_configuration::notify_reset()
{
    switch (state)
    {
    case RESTART_SERVER:
        state = SERVER;
        break;
    case RESTART_CLIENT:
        state = CLIENT;
        break;
    case RESTART_SINGLE:
        state = SINGLE_PLAYER;
        break;
    default:
        break;
    }

    return 1;
}
