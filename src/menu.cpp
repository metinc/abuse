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

#include <math.h>

#include "common.h"

#include "dev.h"

#include "ui/volumewindow.h"

#include "menu.h"
#include "lisp.h"
#include "game.h"
#include "timing.h"
#include "game.h"
#include "id.h"
#include "pmenu.h"
#include "gui.h"
#include "property.h"
#include "clisp.h"
#include "gamma.h"
#include "demo.h"
#include "loadgame.h"
#include "scroller.h"
#include "netcfg.h"

#include "net/sock.h"

//AR
#include "sdlport/setup.h"
#include <SDL3/SDL_timer.h>
extern Settings settings;
extern int get_key_binding(char const *dir, int i);
//

extern net_protocol *prot;

static VolumeWindow *volume_window;

static void create_volume_window()
{
    volume_window = new VolumeWindow();
    volume_window->inm->allow_no_selections();
    volume_window->inm->clear_current();
    volume_window->show();

    wm->grab_focus(volume_window);
    wm->flush_screen();

    while (volume_window)
    {
        Event ev;

        do
        {
            wm->get_event(ev);
        } while (ev.type == EV_MOUSE_MOVE && wm->IsPending());

        wm->flush_screen();

        if (ev.type == EV_CLOSE_WINDOW || (ev.type == EV_KEY && ev.key == JK_ESC))
        {
            wm->close_window(volume_window);
            volume_window = NULL;
        }

        if (!volume_window)
            break;

        if (ev.type == EV_MESSAGE)
        {
            char const *s;

            switch (ev.message.id)
            {
            case ID_SFX_UP:
                sfx_volume += 16;
                if (sfx_volume > 127)
                    sfx_volume = 127;
                volume_window->draw_sfx_vol();
                s = "sfx/ambtech1.wav";
                if (sound_avail & SFX_INITIALIZED)
                    cache.sfx(cache.reg(s, s, SPEC_EXTERN_SFX, 1))->play(sfx_volume);
                break;
            case ID_SFX_DOWN:
                sfx_volume -= 16;
                if (sfx_volume < 0)
                    sfx_volume = 0;
                volume_window->draw_sfx_vol();
                s = "sfx/ambtech1.wav";
                if (sound_avail & SFX_INITIALIZED)
                    cache.sfx(cache.reg(s, s, SPEC_EXTERN_SFX, 1))->play(sfx_volume);
                break;

            case ID_MUSIC_UP:
                music_volume += 16;
                if (music_volume > 127)
                    music_volume = 127;
                volume_window->draw_music_vol();
                if (current_song)
                    current_song->set_volume(music_volume);
                break;
            case ID_MUSIC_DOWN:
                music_volume -= 16;
                if (music_volume < 0)
                    music_volume = 0;
                volume_window->draw_music_vol();
                if (current_song)
                    current_song->set_volume(music_volume);
                break;
            }
        }
    }

    wm->close_window(volume_window);
}

void save_difficulty()
{
    FILE *fp = prefix_fopen("hardness.lsp", "wb");

    if (!fp)
        printf("Unable to write to file hardness.lsp\n");
    else
    {
        fprintf(fp, "(setf difficulty '");
        if (DEFINEDP(symbol_value(l_difficulty)))
        {
            if (symbol_value(l_difficulty) == l_extreme)
                fprintf(fp, "extreme)\n");
            else if (symbol_value(l_difficulty) == l_hard)
                fprintf(fp, "hard)\n");
            else if (symbol_value(l_difficulty) == l_easy)
                fprintf(fp, "easy)\n");
            else
                fprintf(fp, "medium)\n");
        }
        else
            fprintf(fp, "medium)\n");

        fclose(fp);
    }
}

void fade_out(int steps);
void fade_in(image *im, int steps);

image *credits_hires = NULL;

void show_sell(int abortable)
{
    //AR credits screen, enabled hires image

    if (settings.hires && !credits_hires)
        credits_hires = cache.img(cache.reg("art/fore/endgame.spe", "credit_hires", SPEC_IMAGE, 1));

    LSymbol *ss = LSymbol::FindOrCreate("sell_screens");
    if (!DEFINEDP(ss->GetValue()))
    {
        LSpace *sp = LSpace::Current;
        LSpace::Current = &LSpace::Perm;
        //    char *prog="((\"art/help.spe\" . \"sell2\")(\"art/help.spe\" . \"sell4\")(\"art/help.spe\" . \"sell3\")(\"art/fore/endgame.spe\" . \"credit\"))";
        //    char *prog="((\"art/fore/endgame.spe\" . \"credit\") (\"art/help.spe\" . \"sell6\"))";
        char const *prog = "((\"art/fore/endgame.spe\" . \"credit\"))";
        ss->SetValue(LObject::Compile(prog));
        LSpace::Current = sp;
    }

    if (DEFINEDP(ss->GetValue()))
    {
        image blank(ivec2(2, 2));
        blank.clear();
        wm->SetMouseShape(blank.copy(), ivec2(0, 0)); // don't show mouse

        LObject *tmp = (LObject *)ss->GetValue();
        int quit = 0;
        while (tmp && !quit)
        {
            if (settings.hires && credits_hires)
                fade_in(credits_hires, 16);
            else
            {
                int im = cache.reg_object("art/help.spe", CAR(tmp), SPEC_IMAGE, 1);
                fade_in(cache.img(im), 16);
            }

            Event ev;
            do
            {
                wm->flush_screen();
                wm->get_event(ev);
            } while (ev.type != EV_KEY);
            if (ev.key == JK_ESC && abortable)
                quit = 1;
            fade_out(16);
            tmp = (LObject *)CDR(tmp);
        }
        wm->SetMouseShape(cache.img(c_normal)->copy(), ivec2(1, 1));
    }
}

void menu_handler(Event &ev, InputManager *inm)
{
    switch (ev.type)
    {
    case EV_MESSAGE: {
        switch (ev.message.id)
        {
        case ID_LIGHT_OFF:
            if (!volume_window)
            {
                gamma_correct(pal, 1);
            }
            break;
        case ID_RETURN:
            if (!volume_window)
            {
                the_game->set_state(RUN_STATE);
            }
            break;
        case ID_START_GAME:
            if (!volume_window)
            {
                the_game->load_level(level_file);
                the_game->set_state(RUN_STATE);
                view *v;
                for (v = player_list; v; v = v->next)
                    if (v->m_focus)
                        v->reset_player();

                settings.quick_load = level_file; //AR
            }
            break;

        case ID_LOAD_PLAYER_GAME:
            if (!volume_window)
            {
                int got_level = load_game(0, symbol_str("LOAD"));
                the_game->reset_keymap();
                if (got_level)
                {
                    char name[255];
                    sprintf(name, "%ssave%04d.spe", get_save_filename_prefix(), got_level);

                    the_game->load_level(name);
                    the_game->set_state(RUN_STATE);

                    settings.quick_load = name; //AR
                }
            }
            break;

        case ID_VOLUME:
            if (!volume_window)
            {
                create_volume_window();
            }
            break;

        case ID_MEDIUM: {
            l_difficulty->SetValue(l_medium);
            save_difficulty();
        }
        break;
        case ID_HARD: {
            l_difficulty->SetValue(l_hard);
            save_difficulty();
        }
        break;
        case ID_EXTREME: {
            l_difficulty->SetValue(l_extreme);
            save_difficulty();
        }
        break;
        case ID_EASY: {
            l_difficulty->SetValue(l_easy);
            save_difficulty();
        }
        break;

        case ID_NETWORKING: {
            if (!volume_window)
            {
                net_configuration *cfg = new net_configuration;
                if (cfg->input())
                {
                    if (main_net_cfg)
                        delete main_net_cfg;
                    main_net_cfg = cfg;
                }
                else
                    delete cfg;
                the_game->draw(0);
                inm->redraw();
            }
        }
        break;

        case ID_SHOW_SELL:
            if (!volume_window)
            {
                show_sell(1);
                main_screen->clear();
                if (title_screen >= 0)
                {
                    image *im = cache.img(title_screen);
                    main_screen->PutImage(im, main_screen->Size() / 2 - im->Size() / 2);
                }
                inm->redraw();
                fade_in(NULL, 8);
                wm->flush_screen();
            }
            break;
        }
        break;
    }
    break;
    case EV_CLOSE_WINDOW: {
        if (ev.window == volume_window)
        {
            wm->close_window(volume_window);
            volume_window = NULL;
        }
    }
    break;
    }
}

void *current_demo = NULL;

static ico_button *load_icon(int num, int id, int x, int y, int &h, ifield *next, char const *key)
{
    //AR enabled high resolution images

    char name[20];
    char const *base = "newi";
    int a, b, c;

    std::string img_name = "%s%04d.pcx";
    if (settings.hires)
        img_name += "_hires";

    sprintf(name, img_name.c_str(), base, num * 3 + 1);
    a = cache.reg("art/icons.spe", name, SPEC_IMAGE, 1);

    sprintf(name, img_name.c_str(), base, num * 3 + 2);
    b = cache.reg("art/icons.spe", name, SPEC_IMAGE, 1);

    sprintf(name, img_name.c_str(), base, num * 3 + 3);
    c = cache.reg("art/icons.spe", name, SPEC_IMAGE, 1);

    h = cache.img(a)->Size().y;

    return new ico_button(x, y, id, b, b, c, a, next, -1, key);
}

ico_button *make_default_buttons(int x, int &y, ico_button *append_list)
{
    //AR main menu buttons, reenabled the credits button

    int h;
    int diff_on;

    if (DEFINEDP(symbol_value(l_difficulty)))
    {
        if (symbol_value(l_difficulty) == l_extreme)
            diff_on = 3;
        else if (symbol_value(l_difficulty) == l_hard)
            diff_on = 2;
        else if (symbol_value(l_difficulty) == l_easy)
            diff_on = 0;
        else
            diff_on = 1;
    }
    else
        diff_on = 3;

    ico_button *start = load_icon(0, ID_START_GAME, x, y, h, NULL, "ic_start");
    y += h;

    //difficulty/hardness icon
    ico_switch_button *set = NULL;
    if (!main_net_cfg ||
        (main_net_cfg->state != net_configuration::SERVER && main_net_cfg->state != net_configuration::CLIENT))
    {
        set = new ico_switch_button(
            x, y, ID_NULL, diff_on,
            load_icon(3, ID_EASY, x, y, h,
                      load_icon(8, ID_MEDIUM, x, y, h,
                                load_icon(9, ID_HARD, x, y, h, load_icon(10, ID_EXTREME, x, y, h, NULL, "ic_extreme"),
                                          "ic_hard"),
                                "ic_medium"),
                      "ic_easy"),
            NULL);
        y += h;
    }

    ico_button *color = load_icon(4, ID_LIGHT_OFF, x, y, h, NULL, "ic_gamma");
    y += h;

    ico_button *volume = load_icon(5, ID_VOLUME, x, y, h, NULL, "ic_volume");
    y += h;

    // Multiplayer button
    ico_button *multiplayer = NULL;
    if (prot)
    {
        multiplayer = load_icon(11, ID_NETWORKING, x, y, h, NULL, "ic_networking");
        y += h;
    }

    //credits in full version
    // ico_button *sell = load_icon(2, ID_SHOW_SELL, x, y, h, NULL, "ic_sell");
    // y += h;

    ico_button *quit = load_icon(6, ID_QUIT, x, y, h, NULL, "ic_quit");
    y += h;

    //connect buttons/make list

    if (set)
    {
        start->next = set;
        set->next = color;
    }
    else
        start->next = color;

    color->next = volume;

    if (prot)
    {
        volume->next = multiplayer;
        multiplayer->next = quit;
    }
    else
        volume->next = quit;

    // sell->next = quit;

    ico_button *list = append_list;

    if (append_list)
    {
        while (append_list->next)
            append_list = (ico_button *)append_list->next;

        append_list->next = start;
    }
    else
        list = start;

    return list;
}

ico_button *make_conditional_buttons(int x, int &y)
{
    //AR "return to game" and "load game" buttons

    ico_button *start_list = NULL;

    int h;

    //should we include a return icon ?
    if (current_level)
    {
        start_list = load_icon(7, ID_RETURN, x, y, h, NULL, "ic_return");
        y += h;
    }

    ico_button *load = NULL;
    if (show_load_icon())
    {
        load = load_icon(1, ID_LOAD_PLAYER_GAME, x, y, h, NULL, "ic_load");
        y += h;
    }

    if (start_list)
        start_list->next = load;
    else
        start_list = load;

    return start_list;
}

void main_menu()
{
    //AR enabled button selection with a controller, enabled highres button images

    //default button size 32x25, hires size 50x39
    int button_w = 32;
    int button_h = 25;
    int padding_x = 1;
    int move_up = 6; //6 menu buttons buttons by default

    if (current_level)
        move_up++;
    if (show_load_icon())
        move_up++;
    //if(prot) move_up++;//multiplayer button

    if (settings.hires)
    {
        button_w = 50;
        button_h = 39;
        padding_x = 2;
        move_up *= 39;
    }
    else
        move_up *= 25;

    move_up /= 2;

    int y = yres / 2 - move_up;
    int x = xres - button_w - padding_x;

    ico_button *list = make_conditional_buttons(x, y);
    list = make_default_buttons(x, y, list);

    //AR controller ui movement
    int mx, my; //mouse position
    int border_up = yres / 2 - move_up;
    int border_down = y;

    int old_mx = wm->GetMousePos().x;
    int old_my = wm->GetMousePos().y;

    //AR initial position of the mouse in the menu for controller use
    mx = x + button_w / 2;
    my = border_up + button_h / 2;
    //

    InputManager *inm = new InputManager(main_screen, list);
    inm->allow_no_selections();
    inm->clear_current();

    // Calculate total number of buttons
    int total_buttons = 5; // Start, Difficulty, Gamma, Volume, Quit

    // Remove difficulty button if we're in multiplayer server/client mode
    if (main_net_cfg &&
        (main_net_cfg->state == net_configuration::SERVER || main_net_cfg->state == net_configuration::CLIENT))
    {
        total_buttons--;
    }

    Event ev;

    int stop_menu = 0;
    time_marker start;
    wm->flush_screen();
    do
    {
        time_marker new_time;

        if (wm->IsPending())
        {
            do
            {
                wm->get_event(ev);
            } while (ev.type == EV_MOUSE_MOVE && wm->IsPending());
            inm->handle_event(ev, NULL);
            if (ev.type == EV_KEY && ev.key == JK_ESC && current_level)
                wm->Push(new Event(ID_RETURN, NULL));

            menu_handler(ev, inm);
            start.get_time();

            wm->flush_screen();
        }
        else
        {
            // ECS - Added so that main menu doesn't grab 100% of CPU
            SDL_Delay(10);
        }

        if (new_time.diff_time(&start) > 10)
        {
            if (volume_window)
                start.get_time();
            else
            {
                if (!current_demo)
                {
                    LSymbol *d = LSymbol::FindOrCreate("demos");
                    if (DEFINEDP(d->GetValue()))
                        current_demo = d->GetValue();
                }
                if (current_demo)
                {
                    demo_man.set_state(demo_manager::PLAYING, lstring_value(CAR(current_demo)));
                    stop_menu = 1;
                    current_demo = CDR(current_demo);
                }
            }
        }

        if (volume_window)
            stop_menu = 0; // can't exit with volume window open
        else if (main_net_cfg && main_net_cfg->restart_state())
            stop_menu = 1;
        else if (the_game->state == RUN_STATE)
            stop_menu = 1;
        else if (ev.type == EV_MESSAGE)
        {
            if (ev.message.id == ID_START_GAME || ev.message.id == ID_RETURN)
                stop_menu = 1;
            else if (ev.message.id == ID_QUIT)
            {
                exit(EXIT_SUCCESS);
            }
        }

        //AR move cursor over icons
        if (ev.type == EV_KEY)
        {
            if ((ev.key == get_key_binding("up", 0) || ev.key == get_key_binding("up2", 0)))
            {
                if (my - button_h > border_up)
                    my -= button_h;
                wm->SetMousePos(ivec2(mx, my));
            }
            if ((ev.key == get_key_binding("down", 0) || ev.key == get_key_binding("down2", 0)))
            {
                if (my + button_h < border_down)
                    my += button_h;
                wm->SetMousePos(ivec2(mx, my));
            }
        }
        //

    } while (!stop_menu);

    delete inm;

    if (ev.type == EV_MESSAGE && ev.message.id == ID_QUIT) // propogate the quit message
        the_game->end_session();
}
