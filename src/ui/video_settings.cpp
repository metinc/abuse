/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *  Copyright (c) 2016 Antonio Radojkovic <antonior.software@gmail.com>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>

#include "common.h"

#include "game.h"

#include "jwindow.h"
#include "lisp.h"
#include "scroller.h"
#include "id.h"
#include "cache.h"
#include "video_settings.h"

//AR
#include "sdlport/setup.h"
#include "sdlport/video_mode.h"
extern Settings settings;
//

class gamma_slider : public scroller
{
    static constexpr double min_gamma = 0.5;
    static constexpr double max_gamma = 2.0;
    static constexpr double gamma_step = 0.05;
    static constexpr int gamma_steps = 31;

    palette *display_palette;
    const char *darker_label;
    const char *brighter_label;
    ivec2 value_position;

    static int step_for_gamma(double gamma)
    {
        return static_cast<int>(std::lround((std::clamp(gamma, min_gamma, max_gamma) - min_gamma) / gamma_step));
    }

  public:
    gamma_slider(int X, int Y, int ID, int width, double current, palette *pal, const char *darker,
                 const char *brighter, ivec2 ValuePosition, ifield *Next)
        : scroller(X, Y, ID, width, wm->font()->Size().y + 4, 0, gamma_steps, Next), display_palette(pal),
          darker_label(darker), brighter_label(brighter), value_position(ValuePosition)
    {
        sx = step_for_gamma(current);
    }

    double value() const
    {
        return min_gamma + sx * gamma_step;
    }

    void reset(image *screen)
    {
        set_x(step_for_gamma(1.0), screen);
    }

    void scroll_event(int newx, image *screen) override
    {
        (void)newx;
        settings.gamma = value();

        screen->Bar(m_pos, m_pos + ivec2(l - 1, h - 1), wm->black());

        const int font_width = wm->font()->Size().x;
        const int text_y = m_pos.y + 2;
        wm->font()->PutString(screen, ivec2(m_pos.x + 2, text_y), darker_label, wm->bright_color());

        const int brighter_width = static_cast<int>(strlen(brighter_label)) * font_width;
        wm->font()->PutString(screen, ivec2(m_pos.x + l - brighter_width - 2, text_y), brighter_label,
                              wm->bright_color());

        char value_label[16];
        snprintf(value_label, sizeof(value_label), "%.2f", value());
        const int value_width = static_cast<int>(strlen(value_label)) * font_width;
        screen->Bar(value_position, value_position + ivec2(value_width - 1, wm->font()->Size().y - 1),
                    wm->medium_color());
        wm->font()->PutString(screen, value_position, value_label, wm->bright_color());

        display_palette->load();
    }
};

class video_mode_picker : public pick_list
{
    int minimum_item_width;

  public:
    video_mode_picker(int X, int Y, int ID, bool fullscreen, int minimum_width, char **labels, ifield *Next)
        : pick_list(X, Y, ID, 2, labels, 2, 0, Next, nullptr, false), minimum_item_width(minimum_width)
    {
        const int requested = fullscreen ? 1 : 0;
        for (cur_sel = 0; cur_sel < total() && get_selection() != requested; ++cur_sel)
            ;
        if (cur_sel == total())
            cur_sel = 0;
        sx = std::min(cur_sel, max_scroll_position());
    }

    int item_width() override
    {
        return std::max(minimum_item_width, pick_list::item_width());
    }
};

// Helper for retrieving language strings
static char const *lang_string(char const *symbol)
{
    LSymbol *v = LSymbol::Find(symbol);
    if (!v || !DEFINEDP(v->GetValue()))
        return "Language symbol missing!";
    return lstring_value(v->GetValue());
}

void show_video_settings(palette *pal)
{
    const double original_gamma = settings.gamma;
    bool abort_menu = false;

    const char *windowed_label = lang_string("video_windowed");
    const char *fullscreen_label = lang_string("video_fullscreen");
    const char *default_label = lang_string("gamma_default");

    const int font_width = wm->font()->Size().x;
    const int font_height = wm->font()->Size().y;
    const int mode_width =
        std::max(static_cast<int>(strlen(windowed_label)), static_cast<int>(strlen(fullscreen_label))) * font_width +
        12;

    const int client_left = Jwindow::left_border();
    const int client_top = Jwindow::top_border();
    const int padding = 8;
    const int section_gap = 10;
    const int client_width = std::max(settings.big_font ? 260 : 210, mode_width + padding * 2 + 4);
    const int content_x = client_left + padding - 2;
    const int gamma_label_y = padding + 3;
    const int gamma_value_y = client_top + gamma_label_y;
    const int default_y = gamma_label_y - 4;
    const int gamma_y = padding + font_height + 10;
    const int mode_label_y = gamma_y + font_height + 4 + 12 + section_gap;
    const int mode_y = mode_label_y + font_height + 5;
    const int ok_y = mode_y + 2 * (font_height + 1) + 12;
    const int client_bottom = ok_y + cache.img(ok_button)->Size().y + 10 + padding;
    const int client_height = client_bottom - client_top;
    const int slider_width = client_width - padding * 2 - 2;

    const int default_width = static_cast<int>(strlen(default_label)) * font_width + 7;
    const int ok_width = cache.img(ok_button)->Size().x + 7;
    const int default_x = client_left + client_width - padding - default_width - 3;
    const int gamma_value_x = default_x - padding - 4 * font_width;
    const int ok_x = client_left + (client_width - ok_width) / 2;

    info_field *labels =
        new info_field(client_left + 2, mode_label_y, ID_NULL, lang_string("video_mode_msg"),
                       new info_field(client_left + 2, gamma_label_y, ID_NULL, lang_string("gamma_msg"), nullptr));
    button *ok = new button(ok_x, ok_y, ID_GAMMA_OK, cache.img(ok_button), labels);
    button *reset = new button(default_x, default_y, ID_GAMMA_DEFAULT, default_label, ok);
    reset->set_momentary();
    char *mode_labels[] = {const_cast<char *>(windowed_label), const_cast<char *>(fullscreen_label)};
    video_mode_picker *mode_picker = new video_mode_picker(content_x, mode_y, ID_VIDEO_MODE_PICKER,
                                                           settings.fullscreen, client_width - padding * 2 - 6,
                                                           mode_labels, reset);
    gamma_slider *slider = new gamma_slider(content_x, gamma_y, ID_GAMMA_SLIDER, slider_width, settings.gamma, pal,
                                            lang_string("gamma_darker"), lang_string("gamma_brighter"),
                                            ivec2(gamma_value_x, gamma_value_y), mode_picker);

    const ivec2 total_size(client_width + Jwindow::left_border() + Jwindow::right_border(),
                           client_height + Jwindow::top_border() + Jwindow::bottom_border());
    Jwindow *window = wm->CreateWindow(ivec2((xres - total_size.x) / 2, (yres - total_size.y) / 2),
                                       ivec2(client_width, client_height), slider, lang_string("ic_gamma"));

    Event event;
    wm->flush_screen();
    while (!abort_menu)
    {
        do
        {
            wm->get_event(event);
        } while (event.type == EV_MOUSE_MOVE && wm->IsPending());

        if (event.type == EV_CLOSE_WINDOW || (event.type == EV_KEY && event.key == JK_ESC))
        {
            abort_menu = true;
        }
        else if (event.type == EV_MESSAGE && event.message.id == ID_GAMMA_DEFAULT)
        {
            slider->reset(window->m_surf);
        }
        else if (event.type == EV_MESSAGE && event.message.id == ID_GAMMA_OK)
        {
            break;
        }

        wm->flush_screen();
    }

    const double selected_gamma = slider->value();
    const bool fullscreen = mode_picker->get_selection() != 0;
    wm->close_window(window);

    if (abort_menu)
    {
        settings.gamma = original_gamma;
        pal->load();
    }
    else
    {
        settings.gamma = selected_gamma;
        if (video_set_fullscreen(fullscreen))
            settings.SetFullscreen(fullscreen);
        else
            std::cerr << "Failed to apply fullscreen mode\n";
        if (!settings.Save())
            std::cerr << "Failed to save video settings\n";
    }

    wm->flush_screen();
}
