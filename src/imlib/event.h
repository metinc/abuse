/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#ifndef __EVENT_HPP_
#define __EVENT_HPP_

/* Q: Why are these powers of 2? They're never ORed together... */
#define EV_MOUSE_MOVE 1
#define EV_MOUSE_BUTTON 2
#define EV_KEY 4
#define EV_TEXT_INPUT 8
/*#define EV_REDRAW        16 UNUSED */
#define EV_SPURIOUS 32
/* RESIZE is effectively unused (it can never be generated) */
#define EV_RESIZE 64
#define EV_KEYRELEASE 128
#define EV_CLOSE_WINDOW 256
/* DRAG_WINDOW is effectively unused (it CAN be generated, but is never processed) */
#define EV_DRAG_WINDOW 512
#define EV_MESSAGE 1024

#define LEFT_BUTTON 1
#define RIGHT_BUTTON 2
#define MIDDLE_BUTTON 4

#include "keys.h"
#include "sprite.h"

#include <string>

class Jwindow;

class Event : public linked_node
{
  public:
    Event()
    {
        type = EV_SPURIOUS;
        mouse_move = ivec2(0, 0);
        mouse_button = 0;
        key = 0;
        window = NULL;
        message.id = 0;
        message.data = NULL;
    }

    Event(int id, char *data)
    {
        type = EV_MESSAGE;
        mouse_move = ivec2(0, 0);
        mouse_button = 0;
        key = 0;
        window = NULL;
        message.id = id;
        message.data = data;
    }

    int type;
    ivec2 mouse_move;
    int mouse_button, key;
    std::string text;

    Jwindow *window; // NULL is root
    struct
    {
        int id;
        char *data;
    } message;
};

class EventHandler
{
  public:
    EventHandler(image *screen, palette *pal);
    ~EventHandler();

    void Push(Event *ev)
    {
        m_events.add_end(ev);
    }

    void SysInit();
    void SysUninit();
    void SysWarpMouse(ivec2 pos);
    void SysEvent(Event &ev);

    int IsPending();
    void Get(Event &ev);
    void flush_screen();

    void SetMouseShape(image *im, ivec2 center)
    {
        m_sprite->SetVisual(im, 1);
        m_center = center;
    }
    void SetMousePos(ivec2 pos)
    {
        m_pos = ivec2(std::min(std::max(pos.x, 0), m_screen->Size().x - 1),
                      std::min(std::max(pos.y, 0), m_screen->Size().y - 1));
        SysWarpMouse(m_pos);
    }
    //AR
    ivec2 GetMousePos()
    {
        return this->m_pos;
    }
    void SetIgnoreWheelEvents(bool ignore)
    {
        m_ignore_wheel_events = ignore;
    }
  private:
    linked_list m_events;
    int m_pending;
    bool m_ignore_wheel_events = false;
    // "Dead zone" before motion of a stick "counts".
    // Maximum stick values are 0x7FFF, currently I've
    // arbitrarily set this to 1/4th.
    int m_dead_zone; //AR (int m_dead_zone = 0x2000;)

    image *m_screen;

  protected:
    /* Mouse information */
    Sprite *m_sprite;
    ivec2 m_pos, m_center;
    int m_button;
};

#endif
