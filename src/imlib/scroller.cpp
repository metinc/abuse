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

#include "scroller.h"
#define HS_ICON_W 10
#define HS_ICON_H 8

uint8_t hs_left_arrow[10 * 8] = {0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0,
                                 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0, 1, 1, 2,
                                 2, 2, 2, 2, 2, 2, 0, 0, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 0, 0, 0, 0};

uint8_t hs_right_arrow[10 * 8] = {0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                  1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2,
                                  2, 2, 2, 1, 1, 0, 0, 0, 0, 0, 0, 2, 1, 1, 0, 0, 0, 0, 0, 0, 2, 1, 1, 0, 0, 0};

uint8_t vs_up_arrow[8 * 10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 0, 0, 0, 0, 1, 1, 1, 1, 2, 0, 0, 1, 2,
                               1, 1, 2, 1, 2, 0, 0, 0, 1, 1, 2, 0, 0, 0, 0, 0, 1, 1, 2, 0, 0, 0, 0, 0, 1, 1, 2,
                               0, 0, 0, 0, 0, 1, 1, 2, 0, 0, 0, 0, 0, 1, 1, 2, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0};

uint8_t vs_down_arrow[8 * 10] = {0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 1, 1, 2, 0, 0, 0, 0, 0, 1, 1, 2, 0, 0, 0, 0, 0,
                                 1, 1, 2, 0, 0, 0, 0, 0, 1, 1, 2, 0, 0, 0, 0, 0, 1, 1, 2, 0, 0, 0, 1, 2, 1, 1, 2,
                                 1, 2, 0, 0, 1, 1, 1, 1, 2, 0, 0, 0, 0, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

void show_icon(image *screen, int x, int y, int icw, int ich, uint8_t *buf)
{
    ivec2 caa, cbb;
    screen->GetClip(caa, cbb);

    uint8_t remap[3];
    remap[0] = wm->medium_color();
    remap[1] = wm->bright_color();
    remap[2] = wm->dark_color();

    screen->Lock();
    for (int yc = ich; yc; yc--, y++)
    {
        if (y >= caa.y && y < cbb.y)
        {
            uint8_t *sl = screen->scan_line(y) + x;
            for (int xc = icw, xo = x; xc; xc--, xo++, sl++, buf++)
            {
                if (xo >= caa.x && xo < cbb.x)
                    *sl = remap[*buf];
            }
        }
    }
    screen->AddDirty(ivec2(x, y), ivec2(x + icw, y + ich));
    screen->Unlock();
}

scroller::scroller(int X, int Y, int ID, int L, int H, int Vert, int Total_items, ifield *Next)
{
    m_pos = ivec2(X, Y);
    id = ID;
    next = Next;
    l = L;
    h = H;
    sx = 0;
    t = Total_items;
    drag = -1;
    vert = Vert;
}

void scroller::area(int &x1, int &y1, int &x2, int &y2)
{
    area_config();
    x1 = m_pos.x - 1;
    y1 = m_pos.y - 1;
    if (vert)
    {
        x2 = m_pos.x + l + (scrollbar_visible() ? bw() : 0);
        y2 = m_pos.y + h;
    }
    else
    {
        x2 = m_pos.x + l;
        y2 = m_pos.y + h + (scrollbar_visible() ? bh() : 0);
    }
}

void scroller::dragger_area(int &x1, int &y1, int &x2, int &y2)
{
    if (vert)
    {
        x1 = m_pos.x + l;
        y1 = m_pos.y + bh();
        x2 = m_pos.x + l + bw() - 1;
        y2 = m_pos.y + h - bh() - 1;
    }
    else
    {
        x1 = m_pos.x + bw();
        y1 = m_pos.y + h;
        x2 = m_pos.x + l - bw();
        y2 = m_pos.y + h + bh() - 1;
    }
}

int scroller::bh()
{
    // TODO 4,4 or 5,5? Swap _W and _H?
    if (vert)
        return HS_ICON_W + 4;
    else
        return HS_ICON_H + 4;
}

int scroller::bw()
{
    if (vert)
        return HS_ICON_H + 4;
    else
        return HS_ICON_W + 4;
}

uint8_t *scroller::b1()
{
    if (vert)
        return vs_up_arrow;
    else
        return hs_left_arrow;
}

uint8_t *scroller::b2()
{
    if (vert)
        return vs_down_arrow;
    else
        return hs_right_arrow;
}

void scroller::draw_first(image *screen)
{
    if (sx > max_scroll_position())
        sx = max_scroll_position();
    draw(0, screen);
    if (scrollbar_visible())
    {
        screen->WidgetBar(ivec2(b1x(), b1y()), ivec2(b1x() + bw() - 1, b1y() + bh() - 1), wm->bright_color(),
                          wm->medium_color(), wm->dark_color());
        screen->WidgetBar(ivec2(b2x(), b2y()), ivec2(b2x() + bw() - 1, b2y() + bh() - 1), wm->bright_color(),
                          wm->medium_color(), wm->dark_color());
        show_icon(screen, b1x() + 2, b1y() + 2, bw() - 4, bh() - 4, b1());
        show_icon(screen, b2x() + 2, b2y() + 2, bw() - 4, bh() - 4, b2());

        int x1, y1, x2, y2;
        dragger_area(x1, y1, x2, y2);
        screen->Bar(ivec2(x1, y1), ivec2(x2, y2), wm->black());
        screen->Bar(ivec2(x1 + 1, y1 + 1), ivec2(x2 - 1, y2 - 1), wm->medium_color());
        draw_widget(screen, 0);
    }
    scroll_event(sx, screen);
}

void scroller::wig_area(int &x1, int &y1, int &x2, int &y2)
{
    int sx1, sy1, sx2, sy2;
    dragger_area(sx1, sy1, sx2, sy2);
    if (vert)
    {
        x1 = m_pos.x + l + 1;
        if (max_scroll_position() == 0)
            y1 = m_pos.y + bh() + 1;
        else
            y1 = m_pos.y + bh() + 1 + sx * (sy2 - sy1 + 1 - bh()) / max_scroll_position();
    }
    else
    {
        if (max_scroll_position() == 0)
            x1 = m_pos.x + bw() + 1;
        else
            x1 = m_pos.x + bw() + 1 + sx * (sx2 - sx1 + 1 - bw()) / max_scroll_position();
        y1 = m_pos.y + h + 1;
    }
    x2 = x1 + bw() - 3;
    y2 = y1 + bh() - 3;
}

void scroller::draw_widget(image *screen, int erase)
{
    int x1, y1, x2, y2;
    wig_area(x1, y1, x2, y2);
    if (erase)
        screen->Bar(ivec2(x1, y1), ivec2(x2, y2), wm->medium_color());
    else
        screen->WidgetBar(ivec2(x1, y1), ivec2(x2, y2), wm->bright_color(), wm->medium_color(), wm->dark_color());
}

void scroller::draw(int active, image *screen)
{
    int x1, y1, x2, y2;
    area(x1, y1, x2, y2);
    screen->Rectangle(ivec2(x1, y1), ivec2(x2, y2), active ? wm->bright_color() : wm->dark_color());
}

void scroller::handle_event(Event &ev, image *screen, InputManager *inm)
{
    int mx = ev.mouse_move.x, my = ev.mouse_move.y;
    switch (ev.type)
    {
    case EV_MOUSE_BUTTON: {
        if (ev.mouse_button && drag == -1)
        {
            if (!scrollbar_visible())
                handle_inside_event(ev, screen, inm);
            else if (mx >= b1x() && mx < b1x() + bw() && my >= b1y() - 2 && my < b1y() + bh())
            {
                if (sx > 0)
                {
                    draw_widget(screen, 1);
                    sx--;
                    draw_widget(screen, 0);
                    scroll_event(sx, screen);
                }
            }
            else if (mx >= b2x() && mx < b2x() + bw() && my >= b2y() && my <= b2y() + bh())
            {
                if (sx < max_scroll_position())
                {
                    draw_widget(screen, 1);
                    sx++;
                    draw_widget(screen, 0);
                    scroll_event(sx, screen);
                }
            }
            else
            {
                int dx1, dy1, dx2, dy2;
                dragger_area(dx1, dy1, dx2, dy2);
                if (mx >= dx1 && mx <= dx2 && my >= dy1 && my <= dy2)
                {
                    int x1, y1, x2, y2;
                    wig_area(x1, y1, x2, y2);
                    if (mx >= x1 && mx <= x2 && my >= y1 && my <= y2)
                    {
                        drag = sx;
                        inm->grab_focus(this);
                    }
                    else if (t > 1)
                    {
                        int nx = mouse_to_drag(mx, my);
                        if (nx != sx && nx >= 0 && nx <= max_scroll_position())
                        {
                            draw_widget(screen, 1);
                            sx = nx;
                            draw_widget(screen, 0);
                            scroll_event(sx, screen);
                        }
                    }
                }
                else
                    handle_inside_event(ev, screen, inm);
            }
        }
        else if (!ev.mouse_button && drag != -1)
        {
            inm->release_focus();
            drag = -1;
        }
    }
    break;

    case EV_MOUSE_MOVE: {
        if (drag != -1)
        {
            int nx = mouse_to_drag(mx, my);
            if (nx < 0)
                nx = 0;
            else if (nx > max_scroll_position())
                nx = max_scroll_position();
            if (nx != sx)
            {
                draw_widget(screen, 1);
                sx = nx;
                draw_widget(screen, 0);
                scroll_event(sx, screen);
            }
        }
        else if (activate_on_mouse_move())
        {
            int x1, y1, x2, y2;
            wig_area(x1, y1, x2, y2);
            if (mx >= m_pos.x && mx <= m_pos.x + l - 1 && my >= m_pos.y && my <= m_pos.y + h - 1)
                handle_inside_event(ev, screen, inm);
        }
    }
    break;
    case EV_KEY: {
        if (ev.mouse_wheel)
        {
            int new_position = sx - ev.mouse_wheel;
            if (new_position < 0)
                new_position = 0;
            else if (new_position > max_scroll_position())
                new_position = max_scroll_position();

            if (new_position != sx)
            {
                if (scrollbar_visible())
                    draw_widget(screen, 1);
                sx = new_position;
                if (scrollbar_visible())
                    draw_widget(screen, 0);
                scroll_event(sx, screen);
            }
            break;
        }

        switch (ev.key)
        {
        case JK_LEFT: {
            handle_left(screen, inm);
        }
        break;
        case JK_RIGHT: {
            handle_right(screen, inm);
        }
        break;
        case JK_UP: {
            handle_up(screen, inm);
        }
        break;
        case JK_DOWN: {
            handle_down(screen, inm);
        }
        break;

        default:
            handle_inside_event(ev, screen, inm);
        }
    }
    break;
    }
}

void scroller::handle_right(image *screen, InputManager *inm)
{
    if (!vert && sx < max_scroll_position())
    {
        draw_widget(screen, 1);
        sx++;
        draw_widget(screen, 0);
        scroll_event(sx, screen);
    }
}

void scroller::handle_left(image *screen, InputManager *inm)
{
    if (!vert && sx > 0)
    {
        draw_widget(screen, 1);
        sx--;
        draw_widget(screen, 0);
        scroll_event(sx, screen);
    }
}

void scroller::handle_up(image *screen, InputManager *inm)
{
    if (vert && sx > 0)
    {
        draw_widget(screen, 1);
        sx--;
        draw_widget(screen, 0);
        scroll_event(sx, screen);
    }
}

void scroller::handle_down(image *screen, InputManager *inm)
{
    if (vert && sx < max_scroll_position())
    {
        draw_widget(screen, 1);
        sx++;
        draw_widget(screen, 0);
        scroll_event(sx, screen);
    }
}

void scroller::set_x(int x, image *screen)
{
    if (x < 0)
        x = 0;
    if (x > max_scroll_position())
        x = max_scroll_position();
    if (x != sx)
    {
        draw_widget(screen, 1);
        sx = x;
        draw_widget(screen, 0);
        scroll_event(sx, screen);
    }
}

int scroller::mouse_to_drag(int mx, int my)
{
    int x1, y1, x2, y2;
    dragger_area(x1, y1, x2, y2);

    if (vert)
    {
        int h = (y2 - y1 + 1 - bh());
        if (h)
            return (my - m_pos.y - bh() - bh() / 2) * max_scroll_position() / h;
        else
            return 0;
    }
    else
    {
        int w = (x2 - x1 + 1 - bw());
        if (w)
            return (mx - m_pos.x - bw() - bw() / 2) * max_scroll_position() / w;
        else
            return 0;
    }
}

void scroller::scroll_event(int newx, image *screen)
{
    screen->Bar(m_pos, m_pos + ivec2(l - 1, h - 1), wm->black());
    int xa, ya, xo = 0, yo;
    if (vert)
    {
        xa = 0;
        ya = 30;
        yo = m_pos.x + 5;
        yo = m_pos.y + 5;
    }
    else
    {
        xa = 30;
        ya = 0;
        xo = m_pos.x + 5;
        yo = m_pos.y + 5;
    }
    for (int i = newx, c = 0; c < 30 && i < 100; i++, c++)
    {
        char st[10];
        sprintf(st, "%d", i);
        wm->font()->PutString(screen, ivec2(xo, yo), st, wm->bright_color());
        xo += xa;
        yo += ya;
    }
}

int lis_sort(void const *a, void const *b)
{
    pick_list_item *a1 = (pick_list_item *)a;
    pick_list_item *a2 = (pick_list_item *)b;
    return strcmp(a1->name, a2->name);
}

pick_list::pick_list(int X, int Y, int ID, int height, char **List, int num_entries, int start_yoffset, ifield *Next,
                     image *texture, bool sort_entries)
    : spicker(X, Y, ID, height, 1, 1, 0, Next), entries(num_entries), wid(0), lis(nullptr), key_hist_total(0),
      tex(texture)
{
    lis = (pick_list_item *)malloc(sizeof(pick_list_item) * num_entries);
    int i = 0;
    for (; i < num_entries; i++)
    {
        lis[i].name = List[i];
        lis[i].number = i;
    }
    if (sort_entries)
        qsort((void *)lis, num_entries, sizeof(pick_list_item), lis_sort);

    for (i = 0; i < entries; i++)
        if ((int)strlen(List[i]) > wid)
            wid = strlen(List[i]);

    reconfigure();
    if (entries > 0)
    {
        cur_sel = start_yoffset < 0 ? 0 : start_yoffset >= entries ? entries - 1 : start_yoffset;
        sx = cur_sel > max_scroll_position() ? max_scroll_position() : cur_sel;
    }
}

void pick_list::draw_background(image *screen)
{
    if (!tex)
    {
        screen->Bar(m_pos, m_pos + ivec2(l - 1, h - 1), wm->black());
        return;
    }

    ivec2 caa, cbb;
    screen->GetClip(caa, cbb);
    screen->SetClip(m_pos, m_pos + ivec2(l, h));
    int tile_columns = (l + tex->Size().x - 1) / tex->Size().x;
    int tile_rows = (h + tex->Size().y - 1) / tex->Size().y;
    int dy = m_pos.y;
    for (int row = 0; row < tile_rows; row++, dy += tex->Size().y)
        for (int column = 0, dx = m_pos.x; column < tile_columns; column++, dx += tex->Size().x)
            screen->PutImage(tex, ivec2(dx, dy));
    screen->SetClip(caa, cbb);
}

void pick_list::draw_item(image *screen, int x, int y, int num, int active)
{
    if (active)
        screen->Bar(ivec2(x, y), ivec2(x + item_width() - 1, y + item_height() - 1), wm->dark_color());
    wm->font()->PutString(screen, ivec2(x, y), lis[num].name, wm->bright_color());
}

void pick_list::note_selection(image *screen, InputManager *inm, int x)
{
    wm->PushMessage(id, this);
}

void pick_list::handle_inside_event(Event &ev, image *screen, InputManager *inm)
{
    if (ev.type == EV_KEY && ev.key == JK_ENTER)
    {
        wm->PushMessage(id, this);
    }
    else if (ev.type == EV_TEXT_INPUT)
    {
        int found = -1;
        for (unsigned char ch : ev.text)
            if (ch >= ' ' && ch <= '~' && key_hist_total < 20)
                key_hist[(int)(key_hist_total++)] = static_cast<char>(ch);

        for (int i = 0; i < t && found == -1; i++)
        {
            if ((int)strlen(lis[i].name) >= key_hist_total && memcmp(lis[i].name, key_hist, key_hist_total) == 0)
                found = i;
        }
        if (found != -1)
            set_x(found, screen);
        else
            key_hist_total = 0;
    }
    else
    {
        spicker::handle_inside_event(ev, screen, inm);
    }
}

spicker::spicker(int X, int Y, int ID, int Rows, int Cols, int Vert, int MultiSelect, ifield *Next)
    : scroller(X, Y, ID, 2, 2, Vert, 0, Next)
{
    l = -1;
    last_click = -1;
    r = Rows;
    c = Cols;
    m = MultiSelect;
    select = NULL;
}

void spicker::set_select(int x, int on)
{
    if (m)
    {
        if (on)
            select[x / 8] |= 1 << (x & 7);
        else
            select[x / 8] &= (0xff - (1 << (x & 7)));
    }
    else
        cur_sel = x;
}

int spicker::get_select(int x)
{
    if (m)
        return select[x / 8] & (1 << (x & 7));
    else
        return (x == cur_sel);
}

int spicker::first_selected()
{
    if (m)
    {
        for (int i = 0; i < t; i++)
            if (get_select(i))
                return i;
        return -1;
    }
    else
        return cur_sel;
}

void spicker::reconfigure()
{
    if (select)
        free(select);
    select = NULL;

    t = total();
    if (sx > max_scroll_position())
        sx = max_scroll_position();
    if (m)
    {
        select = (uint8_t *)malloc((t + 7) / 8);
        memset(select, 0, (t + 7) / 8);
    }
    else
        cur_sel = 0;
}

void spicker::draw_background(image *screen)
{
    screen->Bar(m_pos, m_pos + ivec2(l - 1, h - 1), wm->dark_color());
}

void spicker::area_config()
{
    l = item_width() * (vert ? 1 : c) + 4;
    h = item_height() * (vert ? r : 1) + 4;
}

void spicker::set_x(int x, image *screen)
{
    cur_sel = x;
    sx = x > max_scroll_position() ? max_scroll_position() : x;
    scroll_event(sx, screen);
}

void spicker::scroll_event(int newx, image *screen)
{
    if (newx < 0)
        newx = 0;
    else if (newx > max_scroll_position())
        newx = max_scroll_position();
    sx = newx;
    last_sel = newx;
    int xa, ya, xo, yo;
    xo = m_pos.x + 2;
    yo = m_pos.y + 2;
    if (vert)
    {
        xa = 0;
        ya = item_height();
    }
    else
    {
        xa = item_width();
        ya = 0;
    }
    draw_background(screen);

    for (int i = newx; i < newx + vis(); i++)
    {
        if (i < t)
        {
            if (m)
                draw_item(screen, xo, yo, i, get_select(i));
            else
                draw_item(screen, xo, yo, i, i == cur_sel);
        }
        xo += xa;
        yo += ya;
    }
}

void spicker::handle_inside_event(Event &ev, image *screen, InputManager *inm)
{
    switch (ev.type)
    {
    case EV_MOUSE_MOVE: {
        if (activate_on_mouse_move())
        {
            int me;
            if (vert)
                me = last_sel + (ev.mouse_move.y - m_pos.y) / item_height();
            else
                me = last_sel + (ev.mouse_move.x - m_pos.x) / item_width();
            if (me < t && me >= 0)
            {
                if (cur_sel != me)
                {
                    cur_sel = me;
                    scroll_event(last_sel, screen);
                    note_new_current(screen, inm, me);
                }
            }
        }
    }
    break;
    case EV_MOUSE_BUTTON: {
        int me;
        if (vert)
            me = last_sel + (ev.mouse_move.y - m_pos.y) / item_height();
        else
            me = last_sel + (ev.mouse_move.x - m_pos.x) / item_width();
        if (me < t && me >= 0)
        {
            if (m)
            {
                if (ev.mouse_button)
                {
                    if (ok_to_select(me))
                    {
                        set_select(me, !get_select(me));
                        scroll_event(last_sel, screen);
                        inm->grab_focus(this);
                    }
                }
                else
                    last_click = -1;
            }
            else if (ok_to_select(me))
            {
                if (cur_sel == me)
                    note_selection(screen, inm, me);
                else
                {
                    cur_sel = me;
                    scroll_event(last_sel, screen);
                    note_new_current(screen, inm, me);
                }
            }
        }
    }
    break;
    }
}

void spicker::handle_up(image *screen, InputManager *inm)
{
    if (vert && cur_sel > 0)
    {
        cur_sel--;

        if (cur_sel < sx)
        {
            draw_widget(screen, 1);
            last_sel = sx = cur_sel;
            draw_widget(screen, 0);
        }
        scroll_event(last_sel, screen);
        note_new_current(screen, inm, cur_sel);
    }
}

void spicker::handle_down(image *screen, InputManager *inm)
{
    if (vert && cur_sel < t - 1)
        cur_sel++;
    else
        return;
    if (cur_sel > sx + r - 1)
    {
        draw_widget(screen, 1);
        last_sel = sx = cur_sel - r + 1;
        draw_widget(screen, 0);
    }
    scroll_event(sx, screen);
    note_new_current(screen, inm, cur_sel);
}

void spicker::handle_left(image *screen, InputManager *inm)
{
}

void spicker::handle_right(image *screen, InputManager *inm)
{
}
