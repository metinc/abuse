/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#ifndef __GU_STAT_HPP
#define __GU_STAT_HPP
#include "status.h"
#include "jwindow.h"

class gui_status_node;
class gui_status_manager : public status_manager
{
    gui_status_node *first;
    void draw_bar(gui_status_node *whom, int perc);

  public:
    gui_status_manager();
    ~gui_status_manager() override;
    void push(char const *name, visual_object *show) override;
    void update(int percentage) override;
    void pop() override;
};

#endif
