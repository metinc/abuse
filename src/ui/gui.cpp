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

#include <algorithm>
#include <utility>

#include "common.h"

#include "cache.h"
#include "gui.h"
#include "dev.h"
#include "loader2.h"

static ToastMessage help_toast;

namespace
{
image *rotate_clockwise(image *source)
{
    const ivec2 source_size = source->Size();
    image *result = new image(ivec2(source_size.y, source_size.x));
    source->Lock();
    result->Lock();
    for (int y = 0; y < source_size.y; ++y)
        for (int x = 0; x < source_size.x; ++x)
            result->scan_line(x)[source_size.y - y - 1] = source->scan_line(y)[x];
    result->Unlock();
    source->Unlock();
    return result;
}
}

ToastMessage::ToastMessage() : m_screen(NULL), m_pos(0), m_background_pos(0), m_background_size(0) {}

void ToastMessage::Show(image *screen, std::string_view text)
{
    Hide();

    if (text.empty())
        return;

    constexpr int border = 1;
    constexpr int padding_x = 3;
    constexpr int padding_y = 2;
    constexpr int screen_margin = 2;

    JCFont *font = wm->font();
    const ivec2 font_size = font->Size();
    const int text_width = JCFont::EncodeForFont(text).size() * font_size.x;
    const ivec2 size(text_width + 2 * (border + padding_x), font_size.y + 2 * (border + padding_y));
    m_pos = ivec2((screen->Size().x - size.x) / 2, screen_margin);

    m_screen = screen;
    m_background_pos = Max(m_pos, ivec2(0));
    const ivec2 background_end = Min(m_pos + size, screen->Size());
    m_background_size = Max(background_end - m_background_pos, ivec2(0));
    m_background.resize(m_background_size.x * m_background_size.y);

    screen->Lock();
    for (int y = 0; y < m_background_size.y; y++)
    {
        memcpy(m_background.data() + y * m_background_size.x,
               screen->scan_line(m_background_pos.y + y) + m_background_pos.x, m_background_size.x);
    }
    screen->Unlock();

    const ivec2 bottom_right = m_pos + size - ivec2(1);
    screen->Bar(m_pos, bottom_right, wm->dark_color());
    screen->Bar(m_pos + ivec2(border), bottom_right - ivec2(border), wm->medium_color());
    font->PutString(screen, m_pos + ivec2(border + padding_x, border + padding_y), text, wm->bright_color());
}

void ToastMessage::Hide()
{
    if (!m_screen)
        return;

    ivec2 clip_aa, clip_bb;
    m_screen->GetClip(clip_aa, clip_bb);
    const ivec2 restore_aa = Max(m_background_pos, clip_aa);
    const ivec2 restore_bb = Min(m_background_pos + m_background_size, clip_bb);
    if (restore_aa < restore_bb)
    {
        const ivec2 offset = restore_aa - m_background_pos;
        const ivec2 span = restore_bb - restore_aa;

        m_screen->Lock();
        for (int y = 0; y < span.y; y++)
        {
            memcpy(m_screen->scan_line(restore_aa.y + y) + restore_aa.x,
                   m_background.data() + (offset.y + y) * m_background_size.x + offset.x, span.x);
        }
        m_screen->Unlock();
        m_screen->AddDirty(restore_aa, restore_bb);
    }

    m_background.clear();
    m_screen = NULL;
}

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

void ico_switch_button::Move(ivec2 pos)
{
    m_pos = pos;
    for (ifield *button = blist; button; button = button->next)
        button->Move(pos);
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
        wm->PushMessage(activate_id);

    if (!hover)
        up = 1;

    int image_id = (up && !enabled)    ? up_inactive
                   : (up && enabled)   ? up_active
                   : (!up && !enabled) ? down_inactive
                                       : down_active;

    screen->PutImage(cache.img(image_id), ivec2(x1, y1));

    if (hover && key[0])
    {
        help_toast.Show(screen, symbol_str(key));
    }
    else if (!hover && key[0])
    {
        help_toast.Hide();
    }
}

extern int32_t S_BUTTON_PRESS_SND;
extern float sfx_volume;

void ico_button::handle_event(Event &ev, image *screen, InputManager *im)
{
    if (!enabled)
        return;

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
        wm->PushMessage(id, this);
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

ico_button::~ico_button()
{
    help_toast.Hide();
}

choice_picker::choice_picker(int x, int y, int ID, std::vector<std::string> Options, int selected, ifield *Next)
    : options(std::move(Options)), selection(0), text_area_width(0), control_width(0), control_height(0),
      pressed_direction(0)
{
    m_pos = ivec2(x, y);
    id = ID;
    next = Next;

    char const *frame_file = "art/frame.spe";
    left_arrow = {rotate_clockwise(cache.img(cache.reg(frame_file, "d_ua", SPEC_IMAGE, 1))),
                  rotate_clockwise(cache.img(cache.reg(frame_file, "d_da", SPEC_IMAGE, 1)))};
    right_arrow = {rotate_clockwise(cache.img(cache.reg(frame_file, "u_ua", SPEC_IMAGE, 1))),
                   rotate_clockwise(cache.img(cache.reg(frame_file, "u_da", SPEC_IMAGE, 1)))};

    if (!options.empty())
        selection = std::clamp(selected, 0, static_cast<int>(options.size()) - 1);

    const int font_width = wm->font()->Size().x;
    for (const std::string &option : options)
        text_area_width = std::max(
            text_area_width, static_cast<int>(JCFont::EncodeForFont(option).size()) * font_width);
    text_area_width += 8;

    constexpr int arrow_gap = 3;
    control_width = left_arrow[0]->Size().x + arrow_gap + text_area_width + arrow_gap + right_arrow[0]->Size().x;
    control_height = std::max({left_arrow[0]->Size().y, right_arrow[0]->Size().y, wm->font()->Size().y});
}

choice_picker::~choice_picker()
{
    for (image *arrow : left_arrow)
        delete arrow;
    for (image *arrow : right_arrow)
        delete arrow;
}

void choice_picker::area(int &x1, int &y1, int &x2, int &y2)
{
    x1 = m_pos.x;
    y1 = m_pos.y;
    x2 = m_pos.x + control_width - 1;
    y2 = m_pos.y + control_height - 1;
}

void choice_picker::draw_first(image *screen)
{
    draw(0, screen);
}

void choice_picker::draw(int active, image *screen)
{
    if (!active)
        pressed_direction = 0;

    constexpr int arrow_gap = 3;
    const ivec2 left_pos(m_pos.x, m_pos.y + (control_height - left_arrow[0]->Size().y) / 2);
    const int text_left = m_pos.x + left_arrow[0]->Size().x + arrow_gap;
    const ivec2 right_pos(text_left + text_area_width + arrow_gap,
                          m_pos.y + (control_height - right_arrow[0]->Size().y) / 2);
    screen->PutImage(left_arrow[pressed_direction < 0 ? 1 : 0], left_pos);
    screen->PutImage(right_arrow[pressed_direction > 0 ? 1 : 0], right_pos);

    screen->Bar(ivec2(text_left, m_pos.y), ivec2(text_left + text_area_width - 1, m_pos.y + control_height - 1),
                wm->medium_color());
    if (!options.empty())
    {
        const std::string &label = options[selection];
        const int label_width = static_cast<int>(JCFont::EncodeForFont(label).size()) * wm->font()->Size().x;
        const ivec2 label_pos(text_left + (text_area_width - label_width) / 2,
                              m_pos.y + (control_height - wm->font()->Size().y) / 2);
        wm->font()->PutString(screen, label_pos + ivec2(1), label, wm->black());
        wm->font()->PutString(screen, label_pos, label, wm->bright_color());
    }
}

void choice_picker::change_selection(int direction, image *screen)
{
    if (options.empty() || direction == 0)
        return;
    selection = (selection + direction + static_cast<int>(options.size())) % static_cast<int>(options.size());
    draw(1, screen);
    wm->PushMessage(id, this);
    if (S_BUTTON_PRESS_SND)
        cache.sfx(S_BUTTON_PRESS_SND)->play(sfx_volume);
}

void choice_picker::handle_event(Event &event, image *screen, InputManager *input)
{
    (void)input;
    int direction = 0;
    const int left_end = m_pos.x + left_arrow[0]->Size().x;
    const int right_start = m_pos.x + control_width - right_arrow[0]->Size().x;
    if (event.mouse_move.x < left_end)
        direction = -1;
    else if (event.mouse_move.x >= right_start)
        direction = 1;

    if (event.type == EV_MOUSE_BUTTON)
    {
        if (event.mouse_button & LEFT_BUTTON)
        {
            pressed_direction = direction;
            draw(1, screen);
        }
        else
        {
            const int released_direction = pressed_direction;
            pressed_direction = 0;
            if (released_direction != 0 && released_direction == direction)
                change_selection(direction, screen);
            else
                draw(1, screen);
        }
    }
    else if (event.type == EV_KEY && (event.key == JK_LEFT || event.key == JK_RIGHT))
        change_selection(event.key == JK_LEFT ? -1 : 1, screen);
}

char *choice_picker::read()
{
    return options.empty() ? nullptr : options[selection].data();
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
