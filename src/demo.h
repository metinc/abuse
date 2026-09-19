/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#ifndef __DEMO_HPP_
#define __DEMO_HPP_

#include "lisp.h"
#include "jwindow.h"

#include <string>

class demo_manager
{
    LObject *initial_difficulty;
    int initial_game_mode;
    bFILE *record_file;
    int skip_next;
    bool automatic_recording;
    bool game_mode_overridden;
    bool reload_snapshots;
    bool network_reloaded;
    int recorded_player_number;
    std::string playback_checkpoint_path;

    void clear_playback_checkpoint();
    bool write_reload_snapshot();
    bool read_reload_snapshot(bool &loaded);

  public:
    enum demo_state
    {
        NORMAL,
        RECORDING,
        PLAYING
    } state;
    int set_state(demo_state new_state, char const *filename = NULL);
    demo_state current_state()
    {
        return state;
    }
    int save_packet(void *packet, int packet_size); // returns non 0 if actually saved
    int get_packet(void *packet, int &packet_size); // returns non 0 if actually loaded
    void notify_network_reload()
    {
        if (state == RECORDING)
            network_reloaded = true;
    }
    int playback_player_number() const
    {
        return recorded_player_number;
    }

    int start_playing(char const *filename);
    int start_recording(char const *filename);
    int start_automatic_recording();
    bool save_playback_checkpoint();
    bool load_playback_checkpoint();
    bool is_automatic_recording() const
    {
        return automatic_recording;
    }
    void reset_game();
    int demo_skip()
    {
        if (skip_next)
        {
            skip_next--;
            return 1;
        }
        else
            return 0;
    }
    demo_manager()
    {
        state = NORMAL;
        skip_next = 0;
        record_file = NULL;
        initial_difficulty = NULL;
        initial_game_mode = 0;
        automatic_recording = false;
        game_mode_overridden = false;
        reload_snapshots = false;
        network_reloaded = false;
        recorded_player_number = 0;
    }
    void do_inputs();
};

extern demo_manager demo_man;

extern void get_event(Event &ev);
extern bool event_waiting();

extern ivec2 last_demo_mpos;
extern int last_demo_mbut;

//extern ulong demo_tick_on;
#endif
