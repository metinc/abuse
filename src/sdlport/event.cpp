/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 2001 Anthony Kruize <trandor@labyrinth.net.au>
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *  Copyright (c) 2016 Antonio Radojkovic <antonior.software@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 */

#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <SDL3/SDL.h>

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "common.h"

#include "image.h"
#include "palette.h"
#include "filter.h"
#include "video.h"
#include "event.h"
#include "sprite.h"
#include "game.h"
#include "joy.h"
#include "setup.h"

extern Settings settings;
extern int get_key_binding(char const *dir, int i);

namespace
{
constexpr int GAMEPAD_SOURCE_BASE = 10000;
constexpr int GAMEPAD_AXIS_SOURCE_BASE = GAMEPAD_SOURCE_BASE + SDL_GAMEPAD_BUTTON_COUNT;
constexpr int LEFT_X_NEGATIVE = GAMEPAD_AXIS_SOURCE_BASE;
constexpr int LEFT_X_POSITIVE = GAMEPAD_AXIS_SOURCE_BASE + 1;
constexpr int LEFT_Y_NEGATIVE = GAMEPAD_AXIS_SOURCE_BASE + 2;
constexpr int LEFT_Y_POSITIVE = GAMEPAD_AXIS_SOURCE_BASE + 3;
constexpr int LEFT_TRIGGER = GAMEPAD_AXIS_SOURCE_BASE + 4;
constexpr int RIGHT_TRIGGER = GAMEPAD_AXIS_SOURCE_BASE + 5;

struct CombinedInputState
{
    SDL_JoystickID active_gamepad = 0;
    bool trigger_pressed[2] = {false, false};
    std::unordered_map<int, int> source_keys;
    std::unordered_map<int, int> key_counts;

    void emit(EventHandler &handler, Event &primary, EventType type, int key)
    {
        Event event;
        event.type = type;
        event.key = key;
        event.mouse_move = primary.mouse_move;
        event.mouse_button = primary.mouse_button;
        if (primary.type == EV_SPURIOUS)
            primary = std::move(event);
        else
            handler.Push(std::move(event));
    }

    void change(EventHandler &handler, Event &primary, int source, bool pressed, int key, bool allow_repeat = false)
    {
        auto held = source_keys.find(source);
        if (pressed)
        {
            if (held != source_keys.end())
            {
                if (allow_repeat && held->second == key && key_is_valid(key))
                    emit(handler, primary, EV_KEY, key);
                return;
            }
            if (!key_is_valid(key))
                return;

            source_keys.emplace(source, key);
            int &count = key_counts[key];
            if (count++ == 0)
                emit(handler, primary, EV_KEY, key);
            return;
        }

        if (held == source_keys.end())
            return;
        key = held->second;
        source_keys.erase(held);
        auto count = key_counts.find(key);
        if (count != key_counts.end() && --count->second == 0)
        {
            key_counts.erase(count);
            emit(handler, primary, EV_KEYRELEASE, key);
        }
    }

    void release_gamepad(EventHandler &handler)
    {
        std::vector<int> sources;
        for (const auto &entry : source_keys)
            if (entry.first >= GAMEPAD_SOURCE_BASE)
                sources.push_back(entry.first);

        for (int source : sources)
        {
            Event release;
            change(handler, release, source, false, JK_NONE);
            if (release.type != EV_SPURIOUS)
                handler.Push(std::move(release));
        }
        trigger_pressed[0] = false;
        trigger_pressed[1] = false;
    }

    bool accepts(SDL_JoystickID id, bool activate)
    {
        if (!active_gamepad && activate)
            active_gamepad = id;
        return active_gamepad == id;
    }
};

CombinedInputState combined_input;

int gamepad_action_key(const std::string &action)
{
    if (action == "none")
        return JK_NONE;
    if (action == "confirm")
        return JK_ENTER;
    if (action == "cancel")
        return JK_ESC;
    if (action == "help")
        return JK_F1;
    return get_key_binding(action.c_str(), 0);
}

const std::string &gamepad_button_action(int button)
{
    switch (button)
    {
    case SDL_GAMEPAD_BUTTON_SOUTH:
        return settings.ctr_a;
    case SDL_GAMEPAD_BUTTON_EAST:
        return settings.ctr_b;
    case SDL_GAMEPAD_BUTTON_WEST:
        return settings.ctr_x;
    case SDL_GAMEPAD_BUTTON_NORTH:
        return settings.ctr_y;
    case SDL_GAMEPAD_BUTTON_LEFT_STICK:
        return settings.ctr_lst;
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
        return settings.ctr_rst;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
        return settings.ctr_lsr;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
        return settings.ctr_rsr;
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
        return settings.ctr_dpad_up;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
        return settings.ctr_dpad_down;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        return settings.ctr_dpad_left;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        return settings.ctr_dpad_right;
    case SDL_GAMEPAD_BUTTON_START:
        return settings.ctr_start;
    case SDL_GAMEPAD_BUTTON_BACK:
        return settings.ctr_back;
    case SDL_GAMEPAD_BUTTON_GUIDE:
        return settings.ctr_guide;
    default:
        static const std::string none = "none";
        return none;
    }
}

void perform_quick_save()
{
    if (current_level && settings.player_touching_console && current_level->save("save0001.spe", 1) == 1)
    {
        the_game->show_help("Station secured!");
        cache.sfx(1031)->play(1.0f);
        settings.quick_load = get_save_path(1);
    }
}

void perform_quick_load()
{
    if (!settings.quick_load.empty())
        the_game->request_level_load(settings.quick_load);
}
}

void reset_input_sources()
{
    combined_input.source_keys.clear();
    combined_input.key_counts.clear();
    combined_input.trigger_pressed[0] = false;
    combined_input.trigger_pressed[1] = false;
}

EventHandler::EventHandler(image *screen, palette *pal)
{
    m_ignore_wheel_events = false;
    m_button = 0;
    m_center = ivec2(0, 0);

    CHECK(screen && pal);
    m_screen = screen;

    uint8_t mouse_sprite[] = {0, 2, 0, 0, 0, 0, 0, 0, 2, 1, 2, 0, 0, 0, 0, 0, 2, 1, 1, 2, 0, 0, 0, 0, 2, 1, 1,
                              1, 2, 0, 0, 0, 2, 1, 1, 1, 1, 2, 0, 0, 2, 1, 1, 1, 1, 1, 2, 0, 0, 2, 1, 1, 2, 2,
                              0, 0, 0, 0, 2, 1, 1, 2, 0, 0, 0, 0, 2, 1, 1, 2, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0};

    Filter f;
    f.Set(1, pal->brightest(1));
    f.Set(2, pal->darkest(1));
    image *im = new image(ivec2(8, 10), mouse_sprite);
    f.Apply(im);

    m_sprite = new Sprite(screen, im, ivec2(100, 100));
    m_pos = screen->Size() / 2;

    if (!video_start_text_input())
        fprintf(stderr, "Warning: Unable to start text input: %s\n", SDL_GetError());
}

EventHandler::~EventHandler()
{
    video_stop_text_input();
    delete m_sprite;
}

void EventHandler::SetMousePos(ivec2 pos)
{
    m_pos = ivec2(std::clamp(pos.x, 0, m_screen->Size().x - 1), std::clamp(pos.y, 0, m_screen->Size().y - 1));

    video_warp_mouse(m_pos);
}

//
// IsPending()
// Are there any events in the queue?
//
bool EventHandler::IsPending()
{
    return !m_events.empty() || SDL_PollEvent(nullptr);
}

void EventHandler::Get(Event &ev)
{
    if (!m_events.empty())
    {
        ev = std::move(m_events.front());
        m_events.pop_front();
        return;
    }

    ev = Event{};
    ev.mouse_move = m_pos;
    ev.mouse_button = m_button;

    SDL_Event sdlev;
    if (!SDL_WaitEvent(&sdlev))
        return;

    int keyboard_source = -1;

    float x_f, y_f;
    SDL_GetMouseState(&x_f, &y_f);
    const ivec2 game_pos = video_window_to_game(x_f, y_f);
    ev.mouse_move = game_pos;
    m_pos = game_pos;

    // Sort out other kinds of events
    switch (sdlev.type)
    {
    case SDL_EVENT_MOUSE_MOTION:
        ev.type = EV_MOUSE_MOVE;
        break;
    case SDL_EVENT_QUIT:
        exit(EXIT_SUCCESS);
        break;
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        video_update_mouse_confinement();
        break;
    case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
    case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
        video_update_mouse_confinement();
        break;
    case SDL_EVENT_WINDOW_MAXIMIZED:
    case SDL_EVENT_WINDOW_RESTORED:
    case SDL_EVENT_WINDOW_MINIMIZED:
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        if (m_ignore_wheel_events)
            break;
        // Conceptually this can be in multiple directions, so use left/right
        // first because those match the bars on the button
        if (sdlev.wheel.x < 0)
        {
            ev.key = get_key_binding("b4", 0);
            ev.type = EV_KEY;
        }
        else if (sdlev.wheel.x > 0)
        {
            ev.key = get_key_binding("b3", 0);
            ev.type = EV_KEY;
        }
        else if (sdlev.wheel.y < 0)
        {
            ev.key = get_key_binding("b4", 0);
            ev.type = EV_KEY;
        }
        else if (sdlev.wheel.y > 0)
        {
            ev.key = get_key_binding("b3", 0);
            ev.type = EV_KEY;
        }
        if (ev.type == EV_KEY)
        {
            // We also need to immediately queue a "release" event or this will
            // be stuck down forever.
            Event release_event = ev;
            release_event.type = EV_KEYRELEASE;
            Push(std::move(release_event));
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        const bool pressed = sdlev.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        int button = 0;
        switch (sdlev.button.button)
        {
        case SDL_BUTTON_LEFT:
            button = LEFT_BUTTON;
            break;
        case SDL_BUTTON_MIDDLE:
            button = MIDDLE_BUTTON;
            break;
        case SDL_BUTTON_RIGHT:
            button = RIGHT_BUTTON;
            break;
        case SDL_BUTTON_X1:
            ev.key = get_key_binding("b4", 0);
            ev.type = pressed ? EV_KEY : EV_KEYRELEASE;
            break;
        case SDL_BUTTON_X2:
            ev.key = get_key_binding("b3", 0);
            ev.type = pressed ? EV_KEY : EV_KEYRELEASE;
            break;
        }

        if (button)
        {
            if (pressed)
                m_button |= button;
            else
                m_button &= ~button;
            ev.mouse_button = m_button;
            ev.type = EV_MOUSE_BUTTON;
        }
        break;
    }
    case SDL_EVENT_TEXT_INPUT:
        ev.type = EV_TEXT_INPUT;
        ev.text = sdlev.text.text ? sdlev.text.text : "";
        break;

    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        keyboard_source = static_cast<int>(sdlev.key.scancode);
        ev.key = JK_NONE;

        if (sdlev.type == SDL_EVENT_KEY_DOWN)
            ev.type = EV_KEY;
        else
            ev.type = EV_KEYRELEASE;

        switch (sdlev.key.key)
        {
        case SDLK_DOWN:
            ev.key = JK_DOWN;
            break;
        case SDLK_UP:
            ev.key = JK_UP;
            break;
        case SDLK_LEFT:
            ev.key = JK_LEFT;
            break;
        case SDLK_RIGHT:
            ev.key = JK_RIGHT;
            break;
        case SDLK_LCTRL:
            ev.key = JK_CTRL_L;
            break;
        case SDLK_RCTRL:
            ev.key = JK_CTRL_R;
            break;
        case SDLK_LALT:
            ev.key = JK_ALT_L;
            break;
        case SDLK_RALT:
            ev.key = JK_ALT_R;
            break;
        case SDLK_LSHIFT:
            ev.key = JK_SHIFT_L;
            break;
        case SDLK_RSHIFT:
            ev.key = JK_SHIFT_R;
            break;
        case SDLK_NUMLOCKCLEAR:
            ev.key = JK_NUM_LOCK;
            break;
        case SDLK_HOME:
            ev.key = JK_HOME;
            break;
        case SDLK_END:
            ev.key = JK_END;
            break;
        case SDLK_BACKSPACE:
            ev.key = JK_BACKSPACE;
            break;
        case SDLK_TAB:
            ev.key = JK_TAB;
            break;
        case SDLK_RETURN:
            ev.key = JK_ENTER;
            break;
        case SDLK_SPACE:
            ev.key = JK_SPACE;
            break;
        case SDLK_CAPSLOCK:
            ev.key = JK_CAPS;
            break;
        case SDLK_ESCAPE:
            ev.key = JK_ESC;
            break;
        case SDLK_F1:
            ev.key = JK_F1;
            break;
        case SDLK_F2:
            ev.key = JK_F2;
            break;
        case SDLK_F3:
            ev.key = JK_F3;
            break;
        case SDLK_F4:
            ev.key = JK_F4;
            break;
        case SDLK_INSERT:
            ev.key = JK_INSERT;
            break;
        case SDLK_KP_0:
            ev.key = JK_INSERT;
            break;
        case SDLK_PAGEUP:
            ev.key = JK_PAGEUP;
            break;
        case SDLK_PAGEDOWN:
            ev.key = JK_PAGEDOWN;
            break;
        case SDLK_KP_8:
            ev.key = JK_UP;
            break;
        case SDLK_KP_2:
        case SDLK_KP_5:
            ev.key = JK_DOWN;
            break;
        case SDLK_KP_4:
            ev.key = JK_LEFT;
            break;
        case SDLK_KP_6:
            ev.key = JK_RIGHT;
            break;

            //random controls

        case SDLK_F5: //AR quick save in dedicated quick save slot when touching the console
            if (ev.type == EV_KEYRELEASE && settings.player_touching_console)
            {
                if (current_level->save("save0001.spe", 1) == 1)
                {
                    the_game->show_help("Station secured!");
                    cache.sfx(1031)->play(1.0f); //id 1031 should be save05.wav
                    settings.quick_load = get_save_path(1);
                }
            }
            ev.key = JK_F5;
            break;

        case SDLK_F6: //AR toggle window input grab
            if (ev.type == EV_KEYRELEASE)
            {
                settings.grab_input = !settings.grab_input;
                video_update_mouse_confinement();
                if (!settings.Save())
                    fprintf(stderr, "Unable to save input grab setting\n");
            }
            ev.key = JK_F6;
            break;

        case SDLK_F7:
            ev.key = JK_F7;
            break;

        case SDLK_F8:
            if (ev.type == EV_KEYRELEASE)
            {
                settings.gamepad_enabled = !settings.gamepad_enabled;
                if (!settings.gamepad_enabled)
                {
                    combined_input.release_gamepad(*this);
                    combined_input.active_gamepad = 0;
                    settings.ctr_aim_x = 0;
                    settings.ctr_aim_y = 0;
                }
                if (!settings.Save())
                    fprintf(stderr, "Unable to save gamepad enabled setting\n");
                the_game->show_help(settings.gamepad_enabled ? "Gamepad enabled" : "Gamepad disabled");
            }
            ev.key = JK_F8;
            break;

        case SDLK_F9: //AR quick load
            if (ev.type == EV_KEYRELEASE && !settings.quick_load.empty())
                the_game->request_level_load(settings.quick_load);
            ev.key = JK_F9;
            break;

        case SDLK_F10: //toggle fullscreen,
            if (ev.type == EV_KEYRELEASE)
                video_change_settings(0, true);
            ev.key = JK_F10;
            break;

        case SDLK_F11: //AR scale window up
            if (ev.type == EV_KEYRELEASE)
                video_change_settings(1, false);
            ev.key = JK_NONE;
            break;

        case SDLK_F12: //AR scale window down
            if (ev.type == EV_KEYRELEASE)
                video_change_settings(-1, false);
            ev.key = JK_NONE;
            break;

        case SDLK_PRINTSCREEN: //grab a screenshot
            if (ev.type == EV_KEYRELEASE)
            {
                if (video_save_screenshot("screenshot.bmp"))
                    the_game->show_help("Screenshot saved to: screenshot.bmp.\n");
                else
                    fprintf(stderr, "Video: Unable to save screenshot: %s\n", SDL_GetError());
            }
            ev.key = JK_NONE;
            break;

        default:
            SDL_Keycode keycode = sdlev.key.key;
            if (the_game->state == MENU_STATE)
                keycode = SDL_GetKeyFromScancode(sdlev.key.scancode, sdlev.key.mod, false);

            const int key = static_cast<int>(keycode);
            ev.key = key_is_valid(key) ? key : JK_NONE;
            break;
        }
        break;

    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP: {
        const bool pressed = sdlev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        if (!settings.gamepad_enabled || !combined_input.accepts(sdlev.gbutton.which, pressed))
            break;

        if (the_game->state != MENU_STATE && settings.ctr_f5 == sdlev.gbutton.button)
        {
            if (!pressed)
                perform_quick_save();
            break;
        }
        if (the_game->state != MENU_STATE && settings.ctr_f9 == sdlev.gbutton.button)
        {
            if (!pressed)
                perform_quick_load();
            break;
        }

        int key = JK_NONE;
        if (the_game->state == MENU_STATE && sdlev.gbutton.button == settings.ctr_menu_confirm)
            key = JK_ENTER;
        else if (the_game->state == MENU_STATE && sdlev.gbutton.button == settings.ctr_menu_cancel)
            key = JK_ESC;
        else
            key = gamepad_action_key(gamepad_button_action(sdlev.gbutton.button));

        const int source = GAMEPAD_SOURCE_BASE + sdlev.gbutton.button;
        combined_input.change(*this, ev, source, pressed, key);
        break;
    }

    case SDL_EVENT_GAMEPAD_ADDED:
        joy_handle_added(sdlev.gdevice.which);
        ev.type = EV_SPURIOUS;
        break;

    case SDL_EVENT_GAMEPAD_REMOVED: {
        joy_handle_removed(sdlev.gdevice.which);
        if (combined_input.active_gamepad == sdlev.gdevice.which)
        {
            combined_input.release_gamepad(*this);
            combined_input.active_gamepad = 0;
            settings.ctr_aim_x = 0;
            settings.ctr_aim_y = 0;
        }
        ev.type = EV_SPURIOUS;
        break;
    }

    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        if (!settings.gamepad_enabled)
            break;

        {
            int activation_threshold = settings.ctr_rst_dz;
            if (sdlev.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX)
                activation_threshold = settings.ctr_lst_dzx;
            else if (sdlev.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY)
                activation_threshold = settings.ctr_lst_dzy;
            else if (sdlev.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ||
                     sdlev.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
                activation_threshold = settings.ctr_trigger_threshold;

            const bool activate = std::abs(static_cast<int>(sdlev.gaxis.value)) >= activation_threshold;
            if (!combined_input.accepts(sdlev.gaxis.which, activate))
                break;

            switch (sdlev.gaxis.axis)
            {
            case SDL_GAMEPAD_AXIS_LEFTX:
                if (sdlev.gaxis.value <= -settings.ctr_lst_dzx)
                {
                    combined_input.change(*this, ev, LEFT_X_POSITIVE, false, JK_NONE);
                    combined_input.change(*this, ev, LEFT_X_NEGATIVE, true, get_key_binding("left", 0));
                }
                else if (sdlev.gaxis.value >= settings.ctr_lst_dzx)
                {
                    combined_input.change(*this, ev, LEFT_X_NEGATIVE, false, JK_NONE);
                    combined_input.change(*this, ev, LEFT_X_POSITIVE, true, get_key_binding("right", 0));
                }
                else
                {
                    combined_input.change(*this, ev, LEFT_X_NEGATIVE, false, JK_NONE);
                    combined_input.change(*this, ev, LEFT_X_POSITIVE, false, JK_NONE);
                }
                break;

            case SDL_GAMEPAD_AXIS_LEFTY:
                if (sdlev.gaxis.value <= -settings.ctr_lst_dzy)
                {
                    combined_input.change(*this, ev, LEFT_Y_POSITIVE, false, JK_NONE);
                    combined_input.change(*this, ev, LEFT_Y_NEGATIVE, true, get_key_binding("up", 0));
                }
                else if (sdlev.gaxis.value >= settings.ctr_lst_dzy)
                {
                    combined_input.change(*this, ev, LEFT_Y_NEGATIVE, false, JK_NONE);
                    combined_input.change(*this, ev, LEFT_Y_POSITIVE, true, get_key_binding("down", 0));
                }
                else
                {
                    combined_input.change(*this, ev, LEFT_Y_NEGATIVE, false, JK_NONE);
                    combined_input.change(*this, ev, LEFT_Y_POSITIVE, false, JK_NONE);
                }
                break;

                //AR just save the values and update aim inside the game loop
            case SDL_GAMEPAD_AXIS_RIGHTX:
                settings.ctr_aim_x = sdlev.gaxis.value;
                ev.type = EV_SPURIOUS;
                break;

            case SDL_GAMEPAD_AXIS_RIGHTY:
                settings.ctr_aim_y = sdlev.gaxis.value;
                ev.type = EV_SPURIOUS;
                break;

            case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
            case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: {
                const bool left = sdlev.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
                const int source = left ? LEFT_TRIGGER : RIGHT_TRIGGER;
                const int binding = left ? GAMEPAD_BINDING_LEFT_TRIGGER : GAMEPAD_BINDING_RIGHT_TRIGGER;
                const int index = left ? 0 : 1;
                const bool was_pressed = combined_input.trigger_pressed[index];
                const bool pressed = sdlev.gaxis.value >= settings.ctr_trigger_threshold ||
                                     (was_pressed && sdlev.gaxis.value > settings.ctr_trigger_threshold -
                                                                             settings.ctr_trigger_hysteresis);
                combined_input.trigger_pressed[index] = pressed;

                if (the_game->state != MENU_STATE && (settings.ctr_f5 == binding || settings.ctr_f9 == binding))
                {
                    if (was_pressed && !pressed)
                    {
                        if (settings.ctr_f5 == binding)
                            perform_quick_save();
                        else
                            perform_quick_load();
                    }
                    break;
                }

                int key = JK_NONE;
                if (the_game->state == MENU_STATE && settings.ctr_menu_confirm == binding)
                    key = JK_ENTER;
                else if (the_game->state == MENU_STATE && settings.ctr_menu_cancel == binding)
                    key = JK_ESC;
                else
                    key = gamepad_action_key(left ? settings.ctr_ltg : settings.ctr_rtg);
                combined_input.change(*this, ev, source, pressed, key);
                break;
            }
            }
        }
        break;
    }

    if (keyboard_source >= 0 && (ev.type == EV_KEY || ev.type == EV_KEYRELEASE))
    {
        const bool pressed = ev.type == EV_KEY;
        const int key = ev.key;
        ev.type = EV_SPURIOUS;
        combined_input.change(*this, ev, keyboard_source, pressed, key, true);
    }
}

void EventHandler::flush_screen()
{
    update_dirty(main_screen);
}
