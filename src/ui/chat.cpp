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

#include "chat.h"
#include "dev.h"
#include "id.h"
#include "scroller.h"

#include <algorithm>
#include <string_view>

namespace
{
constexpr int CHAT_WINDOW_WIDTH = 320;
}

class chat_history_scroller : public scroller
{
    chat_console *owner;

  protected:
    int max_scroll_position() const override
    {
        return std::max(0, t - owner->visible_history_rows());
    }

    int visible_scroll_items() const override
    {
        return owner->visible_history_rows();
    }

  public:
    chat_history_scroller(chat_console *Owner, int X, int Y, int Width, int Height, int Positions, int Position)
        : scroller(X, Y, ID_NULL, Width, Height, 1, Positions, nullptr), owner(Owner)
    {
        sx = Position;
    }

    void draw(int active, image *target) override
    {
        (void)active;
        (void)target;
    }

    void scroll_event(int position, image *target) override
    {
        owner->scroll_history(position, target);
    }
};

chat_console *chat = NULL;

chat_console::chat_console(JCFont *font, int width, int height)
    : console(font, width, height < 4 ? 4 : height, symbol_str("CHAT"))
{
    hide();
    clear();
    cx = 0;
    cy = h - 1;
    lastx = (xres - CHAT_WINDOW_WIDTH) / 2;
    lasty = yres / 2 - h;
}

void chat_console::clear()
{
    history.clear();
    history_offset = 0;
    memset(screen, ' ', w * h);
    redraw();
}

void chat_console::put_all(char *st)
{
    const std::string encoded = JCFont::EncodeForFont(st ? std::string_view(st) : std::string_view());
    std::string_view remaining = encoded;
    do
    {
        const size_t newline = remaining.find('\n');
        const std::string_view line = remaining.substr(0, newline);

        if (line.empty())
            history.emplace_back();
        else
            for (size_t offset = 0; offset < line.size(); offset += w)
                history.emplace_back(line.substr(offset, w));

        if (newline == std::string_view::npos)
            break;
        remaining.remove_prefix(newline + 1);
    } while (true);

    history_offset = max_history_offset();
    if (history_scroller)
    {
        history_scroller->t = std::max(1, static_cast<int>(history.size()));
        history_scroller->sx = history_offset;
    }
    redraw();
}

void chat_console::draw_user(char *st)
{
    memset(screen + w * (h - 1), ' ', w);
    memcpy(screen + w * (h - 1), st, strlen(st));
    cx = strlen(st);
    cy = h - 1;
    redraw();
}

int chat_console::visible_history_rows() const
{
    return h - 1;
}

int chat_console::max_history_offset() const
{
    return std::max(0, static_cast<int>(history.size()) - visible_history_rows());
}

void chat_console::update_history_screen()
{
    const int rows = visible_history_rows();
    memset(screen, ' ', w * rows);

    for (int row = 0; row < rows && history_offset + row < static_cast<int>(history.size()); row++)
    {
        const std::string &line = history[history_offset + row];
        memcpy(screen + row * w, line.data(), std::min(static_cast<int>(line.size()), w));
    }
}

void chat_console::draw_history(image *target)
{
    if (!target)
        return;

    update_history_screen();
    const ivec2 origin(Jwindow::left_border(), Jwindow::top_border());
    const ivec2 font_size = fnt->Size();
    target->Bar(origin, origin + ivec2(screen_w() - 1, visible_history_rows() * font_size.y - 1), wm->black());

    char *text = screen;
    for (int row = 0; row < visible_history_rows(); row++)
        for (int column = 0; column < w; column++, text++)
            if (*text != ' ')
                fnt->PutChar(target, origin + ivec2(column * font_size.x, row * font_size.y), *text);
}

void chat_console::scroll_history(int offset, image *target)
{
    history_offset = std::clamp(offset, 0, max_history_offset());
    draw_history(target);
}

void chat_console::show()
{
    if (con_win)
        return;

    history_offset = max_history_offset();
    history_scroller = new chat_history_scroller(this, 2, 0, screen_w(), screen_h(),
                                                 std::max(1, static_cast<int>(history.size())), history_offset);
    const int client_width = CHAT_WINDOW_WIDTH - Jwindow::left_border() - Jwindow::right_border();
    con_win = wm->CreateWindow(ivec2(lastx, lasty), ivec2(client_width, screen_h()), history_scroller, name);
    redraw();
    con_win->m_surf->SetClip(ivec2(con_win->x1(), con_win->y1()), ivec2(con_win->x2() + 1, con_win->y2() + 1));
}

void chat_console::hide()
{
    history_scroller = nullptr;
    console::hide();
}

void chat_console::redraw()
{
    update_history_screen();
    console::redraw();
    if (history_scroller && con_win)
        history_scroller->draw_first(con_win->m_surf);
}
