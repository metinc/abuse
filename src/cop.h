/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#ifndef __COP_HPP_
#define __COP_HPP_

// functions defined for the main player,  these were translated because they
// are called every tick and they were getting slow/complicated

// Keep this order in sync with the DARNEL variables in lisp/people.lsp.
enum cop_player_lvar
{
    in_climbing_area,
    disable_top_draw,
    just_hit,
    ship_pan_x,
    special_power,
    used_special_power,
    last1_x,
    last1_y,
    last2_x,
    last2_y,
    has_saved_this_level,
    r_ramp,
    g_ramp,
    b_ramp,
    is_teleporting,
    just_fired,
    has_compass,
    coop_checkpoint_active,
    coop_checkpoint_x,
    coop_checkpoint_y
};

void *top_aim(bool check_local);
void decrement_fire_delay();
void *laser_ufun(void *args);
void *top_ufun(void *args);
void *plaser_ufun(void *args);
void *player_rocket_ufun(void *args);
void *lsaber_ufun(void *args);
void *cop_mover(int xm, int ym, int but);
void *sgun_ai();
void *ladder_ai();
void *top_draw();
void *bottom_draw();
void *mover_ai();
void *respawn_ai();
void *score_draw();
void *show_kills();

#endif
