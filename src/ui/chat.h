/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#ifndef CHAT_HPP_
#define CHAT_HPP_

#include "console.h"
#include <string>
#include <vector>

class chat_history_scroller;

class chat_console : public console
{
    friend class chat_history_scroller;

    std::vector<std::string> history;
    chat_history_scroller *history_scroller = nullptr;
    int history_offset = 0;

    int visible_history_rows() const;
    int max_history_offset() const;
    void update_history_screen();
    void draw_history(image *target);
    void scroll_history(int offset, image *target);

  public:
    int chat_event(Event &ev)
    {
        if (!con_win)
            return 0;
        else
            return con_win == ev.window;
    }
    void draw_user(char *st);
    void put_all(char *st);
    void clear();
    void show() override;
    void hide() override;
    void redraw() override;
    chat_console(JCFont *font, int width, int height);
};

extern chat_console *chat;

#endif
