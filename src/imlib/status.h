/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#ifndef __STATUS_HPP_
#define __STATUS_HPP_

#include "visobj.h" // get visual object declaration

class status_manager
{
  public:
    virtual void push(char const *name, visual_object *show) = 0;
    virtual void update(int percentage) = 0;
    virtual void pop() = 0;
    virtual ~status_manager() = default;
};

extern status_manager *stat_man;

class stack_stat // something you can declare on the stack that is sure to get cleaned up
{
    status_manager *manager;

  public:
    stack_stat(char const *st, visual_object *show = NULL) : manager(stat_man)
    {
        if (manager)
            manager->push(st, show);
    }
    ~stack_stat()
    {
        if (manager)
            manager->pop();
    }
};

#endif
