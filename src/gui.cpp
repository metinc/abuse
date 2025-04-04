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

#include "common.h"

#include "cache.h"
#include "gui.h"
#include "dev.h"
#include "loader2.h"

void ico_button::set_act_id(int id)
{
    activate_id = id;
}

ico_switch_button::ico_switch_button(int X, int Y, int ID, int start_on, ifield *butts, ifield *Next)
{
    m_pos = ivec2(X, Y);
    id = ID;
    next = Next;
    blist = cur_but = butts;
    act = 0;
    for (ifield *b = blist; b; b = b->next)
        b->m_pos = m_pos;
    while (cur_but && start_on--)
        cur_but = cur_but->next;
    if (!cur_but)
        cur_but = blist;
}

void ico_switch_button::area(int &x1, int &y1, int &x2, int &y2)
{
    x1 = 10000;
    y1 = 10000;
    x2 = -10000;
    y2 = -10000;
    int X1, Y1, X2, Y2;
    for (ifield *b = blist; b; b = b->next)
    {
        b->area(X1, Y1, X2, Y2);
        if (X1 < x1)
            x1 = X1;
        if (Y1 < y1)
            y1 = Y1;
        if (X2 > x2)
            x2 = X2;
        if (Y2 > y2)
            y2 = Y2;
    }
    if (!blist)
    {
        x1 = x2 = m_pos.x;
        y1 = y2 = m_pos.y;
    }
}

ifield *ico_switch_button::unlink(int id)
{
    ifield *last = NULL;
    for (ifield *b = blist; b; b = b->next)
    {
        if (b->id == id)
        {
            if (last)
                last->next = b->next;
            else
                blist = b->next;
            if (cur_but == b)
                cur_but = blist;
            return b;
        }
        ifield *x = b->unlink(id);
        if (x)
            return x;
        last = b;
    }
    return NULL;
}

void ico_switch_button::handle_event(Event &ev, image *screen, InputManager *im)
{
    bool validEvent =
        (ev.type == EV_KEYRELEASE && ev.key == 13) || (ev.type == EV_MOUSE_BUTTON && ev.mouse_button == 0);
    if (validEvent)
    {
        cur_but = cur_but->next;
        if (!cur_but)
            cur_but = blist;
        cur_but->draw(act, screen);
    }
    cur_but->handle_event(ev, screen, im);
}

void ico_button::draw(int hover, image *screen)
{
    int x1, y1, x2, y2;
    area(x1, y1, x2, y2);

    // Event is needed to show save game preview on hover
    if (hover && activate_id != -1 && enabled)
        wm->Push(new Event(activate_id, NULL));

    if (!hover)
        up = 1;

    int image_id = (up && !enabled)    ? up_inactive
                   : (up && enabled)   ? up_active
                   : (!up && !enabled) ? down_inactive
                                       : down_active;

    screen->PutImage(cache.img(image_id), ivec2(x1, y1));

    if (hover && key[0])
    {
        int g = 127;
        screen->Bar(ivec2(0, 0), ivec2(144, 20), 0);
        wm->font()->PutString(screen, ivec2(3), symbol_str(key), color_table->Lookup(g >> 3, g >> 3, g >> 3));
    }
    else if (!hover && key[0])
    {
        screen->Bar(ivec2(0, 0), ivec2(144, 20), 0);
    }
}

extern int32_t S_BUTTON_PRESS_SND;
extern int sfx_volume;

void ico_button::handle_event(Event &ev, image *screen, InputManager *im)
{
    bool validEvent = ((ev.type == EV_KEY || ev.type == EV_KEYRELEASE) && ev.key == 13) || ev.type == EV_MOUSE_BUTTON ||
                      (ev.type == EV_MOUSE_MOVE && ev.mouse_button > 0);
    if (!validEvent)
        return;

    int x1, y1, x2, y2;
    area(x1, y1, x2, y2);
    int new_up = (ev.type == EV_KEYRELEASE && ev.key == 13) || (ev.type == EV_MOUSE_BUTTON && ev.mouse_button == 0);
    if (new_up != up)
    {
        up = new_up;
        draw(enabled, screen);
    }
    if (up)
    {
        wm->Push(new Event(id, (char *)this));
        if (S_BUTTON_PRESS_SND)
            cache.sfx(S_BUTTON_PRESS_SND)->play(sfx_volume);
    }
}

void ico_button::area(int &x1, int &y1, int &x2, int &y2)
{
    x1 = m_pos.x;
    y1 = m_pos.y;
    x2 = m_pos.x + cache.img(up_inactive)->Size().x - 1;
    y2 = m_pos.y + cache.img(up_inactive)->Size().y - 1;
}

ico_button::ico_button(int x, int y, int id, int up_inactive, int down_inactive, int up_active, int down_active,
                       ifield *next, int activate_id, char const *help_key)
{
    if (help_key)
    {
        strncpy(key, help_key, 15);
        key[15] = 0;
    }
    else
        key[0] = 0;

    up = 1;
    m_pos = ivec2(x, y);
    this->id = id;
    this->up_inactive = up_inactive;
    this->down_inactive = down_inactive;
    this->up_active = up_active;
    this->down_active = down_active;
    this->next = next;
    this->activate_id = activate_id;
    enabled = 1;
}

ico_switch_button::~ico_switch_button()
{
    while (blist)
    {
        ifield *i = blist;
        blist = blist->next;
        delete i;
    }
}
