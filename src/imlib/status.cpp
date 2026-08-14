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

#include <string.h>

#include "common.h"

#include "status.h"

status_manager *stat_man = NULL;

class text_status_node
{
  public:
    char *name;
    text_status_node *next;
    visual_object *show;
    int last_update;
    text_status_node(char const *Name, visual_object *Show, text_status_node *Next)
    {
        name = strdup(Name);
        show = Show;
        next = Next;
        last_update = 0;
    }
    ~text_status_node()
    {
        free(name);
        if (show)
            delete show;
    }
};

text_status_manager::text_status_manager()
{
    first = NULL;
}

void text_status_manager::push(char const *name, visual_object *show)
{
    first = new text_status_node(name, show, first);
}

void text_status_manager::update(int)
{
}

void text_status_manager::pop()
{
    CONDITION(first, "No status's to pop!");
    text_status_node *p = first;
    first = first->next;
    delete p;
}
