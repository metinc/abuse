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
#include "player_name.h"
#include "sdlport/setup.h"

#include <cstring>

net_configuration *main_net_cfg = nullptr;
extern Settings settings;
extern char game_name[50];

net_configuration::net_configuration()
{
    copy_player_name(name, sizeof(name), settings.player_name.c_str());
    strncpy(game_name, settings.server_name.c_str(), sizeof(game_name) - 1);
    game_name[sizeof(game_name) - 1] = '\0';
    server_host[0] = '\0';
    room_code[0] = '\0';
    online = false;
    join_failed = false;
    server_full = false;
    host_ended_server = false;
    waiting_for_host = false;
    lobby_players = 1;
    streamer_mode = settings.streamer_mode;

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
