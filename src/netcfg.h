/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#ifndef __NETCFG_HPP_
#define __NETCFG_HPP_

class net_configuration
{
  public:
    static constexpr int DEFAULT_ANT_MULTIPLIER = 2;
    static constexpr int MAX_ANT_MULTIPLIER = 99;
    int ant_multiplier = DEFAULT_ANT_MULTIPLIER;

    enum
    {
        SINGLE_PLAYER,
        SERVER,
        CLIENT,
        RESTART_SERVER,
        RESTART_CLIENT,
        RESTART_SINGLE
    } state;

    int restart_state();
    int notify_reset();

    unsigned short port,
        server_port; // if we are a server, use our_port
    char name[100];
    char server_host[100];
    char room_code[7];
    bool online;
    bool join_failed;
    bool host_failed = false;
    bool server_full;
    bool host_ended_server;
    bool waiting_for_host;
    bool returning_to_menu;
    int lobby_players;
    bool streamer_mode;
    bool resume_coop = false;

    char min_players, max_players;
    short kills;

    enum
    {
        DEATHMATCH,
        COOP
    } game_mode;

    net_configuration();
};

extern net_configuration *main_net_cfg;

#endif
