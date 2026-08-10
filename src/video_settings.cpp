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

    static int step_for_gamma(double gamma)
    {
        return static_cast<int>(std::lround((std::clamp(gamma, min_gamma, max_gamma) - min_gamma) / gamma_step));
    }

  public:
    gamma_slider(int X, int Y, int ID, int width, double current, palette *pal, const char *darker,
                 const char *brighter, ifield *Next)
        : scroller(X, Y, ID, width, wm->font()->Size().y + 4, 0, gamma_steps, Next), display_palette(pal),
          darker_label(darker), brighter_label(brighter)
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
        wm->font()->PutString(screen, ivec2(m_pos.x + (l - value_width) / 2, text_y), value_label,
                              wm->bright_color());

        display_palette->load();
    }
};

class video_mode_picker : public spicker
{
    const char *labels[3];
    int width;

  public:
    video_mode_picker(int X, int Y, int ID, int current, const char *windowed, const char *borderless,
                      const char *exclusive, ifield *Next)
        : spicker(X, Y, ID, 3, 1, 1, 0, Next), labels{windowed, borderless, exclusive}
    {
        width = 0;
        for (const char *label : labels)
            width = std::max(width, static_cast<int>(strlen(label)) * wm->font()->Size().x + 8);
        reconfigure();
        cur_sel = std::max(0, std::min(2, current));
    }

    void draw_item(image *screen, int x, int y, int num, int active) override
    {
        screen->Bar(ivec2(x, y), ivec2(x + item_width() - 1, y + item_height() - 1),
                    active ? wm->medium_color() : wm->dark_color());
        wm->font()->PutString(screen, ivec2(x + 4, y + 2), labels[num], wm->bright_color());
    }

    int total() override
    {
        return 3;
    }

    int item_width() override
    {
        return width;
    }

    int item_height() override
    {
        return wm->font()->Size().y + 4;
    }

    int activate_on_mouse_move() override
    {
        return 0;
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
    const char *borderless_label = lang_string("video_borderless");
    const char *exclusive_label = lang_string("video_exclusive");
    const char *default_label = lang_string("gamma_default");

    const int font_width = wm->font()->Size().x;
    const int font_height = wm->font()->Size().y;
    const int mode_width =
        std::max({static_cast<int>(strlen(windowed_label)), static_cast<int>(strlen(borderless_label)),
                  static_cast<int>(strlen(exclusive_label))}) *
            font_width +
        12;

    const int window_width = std::max(settings.big_font ? 350 : 300, mode_width + 20);
    const int gamma_y = font_height * 3;
    const int default_y = gamma_y + font_height + 20;
    const int mode_label_y = default_y + font_height + 12;
    const int mode_y = mode_label_y + font_height + 3;
    const int ok_y = mode_y + 3 * (font_height + 4) + 10;
    const int window_height = ok_y + 28;
    const int slider_x = 10;
    const int slider_width = window_width - slider_x * 2;

    const int default_width = static_cast<int>(strlen(default_label)) * font_width + 7;
    const int ok_width = cache.img(ok_button)->Size().x + 7;
    const int default_x = (window_width - default_width) / 2;
    const int ok_x = (window_width - ok_width) / 2;

    info_field *labels =
        new info_field(2, mode_label_y, ID_NULL, lang_string("video_mode_msg"),
                       new info_field(2, font_height, ID_NULL, lang_string("gamma_msg"), nullptr));
    button *ok = new button(ok_x, ok_y, ID_GAMMA_OK, cache.img(ok_button), labels);
    button *reset = new button(default_x, default_y, ID_GAMMA_DEFAULT, default_label, ok);
    video_mode_picker *mode_picker =
        new video_mode_picker((window_width - mode_width) / 2, mode_y, ID_VIDEO_MODE_PICKER, settings.fullscreen,
                              windowed_label, borderless_label, exclusive_label, reset);
    gamma_slider *slider =
        new gamma_slider(slider_x, gamma_y, ID_GAMMA_SLIDER, slider_width, settings.gamma, pal,
                         lang_string("gamma_darker"), lang_string("gamma_brighter"), mode_picker);

    Jwindow *window =
        wm->CreateWindow(ivec2(xres / 2 - window_width / 2, yres / 2 - window_height / 2),
                         ivec2(window_width, window_height), slider);

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
    const int fullscreen_mode = mode_picker->first_selected();
    wm->close_window(window);

    if (abort_menu)
    {
        settings.gamma = original_gamma;
        pal->load();
    }
    else
    {
        settings.gamma = selected_gamma;
        if (video_set_fullscreen_mode(fullscreen_mode))
            settings.SetFullscreenMode(fullscreen_mode);
        else
            std::cerr << "Failed to apply fullscreen mode\n";
        if (!settings.Save())
            std::cerr << "Failed to save video settings\n";
    }

    wm->flush_screen();
}
