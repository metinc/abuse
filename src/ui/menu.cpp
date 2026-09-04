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

#include "audio_volume.h"
#include "dev.h"

#include "audio_settings.h"
#include "general_settings.h"

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
#include "video_settings.h"
#include "demo.h"
#include "loadgame.h"
#include "multiplayer.h"
#include "scroller.h"
#include "netcfg.h"
#include "nfserver.h"

#include "net/sock.h"

//AR
#include "sdlport/setup.h"
#include "sdlport/sound.h"
#include <SDL3/SDL_timer.h>
extern Settings settings;
extern int get_key_binding(char const *dir, int i);
//

extern net_protocol *prot;

static AudioSettingsWindow *audio_settings_window;

static bool load_player_game_enabled()
{
    if (!the_game->multiplayer_menu_active())
        return show_load_icon();

    // The co-op host can always restart. If no save console has created a
    // checkpoint yet, restart_coop_from_checkpoint() reloads the level start
    // and restores every player's initial ammunition instead.
    return main_net_cfg && main_net_cfg->game_mode == net_configuration::COOP && client_number() == 0;
}

static bool apply_soundfont(const std::string &selected_soundfont)
{
    if (selected_soundfont == settings.soundfont)
        return true;

    const std::string previous_soundfont = settings.soundfont;
    bool applied = sound_set_soundfont(selected_soundfont);
    if (applied && current_song && sound_is_initialized())
        applied = current_song->reload();

    if (!applied)
    {
        fprintf(stderr, "Unable to apply SoundFont '%s'\n", selected_soundfont.c_str());
        if (sound_set_soundfont(previous_soundfont) && current_song && sound_is_initialized() &&
            !current_song->reload())
            fprintf(stderr, "Unable to restore the previous SoundFont\n");
        return false;
    }

    settings.SetSoundFont(selected_soundfont);
    if (!settings.Save())
        fprintf(stderr, "Unable to save SoundFont setting\n");
    return true;
}

static void create_audio_settings_window()
{
    audio_settings_window = new AudioSettingsWindow();
    Jwindow *window = audio_settings_window->window();
    window->inm->allow_no_selections();
    window->inm->clear_current();
    window->show();

    wm->grab_focus(window);
    wm->flush_screen();

    while (audio_settings_window)
    {
        Event ev;

        do
        {
            wm->get_event(ev);
        } while (ev.type == EV_MOUSE_MOVE && wm->IsPending());

        if (ev.type == EV_CLOSE_WINDOW || (ev.type == EV_KEY && ev.key == JK_ESC))
            break;

        if (ev.type == EV_MESSAGE)
        {
            char const *s;

            switch (ev.message.id)
            {
            case ID_SFX_UP:
                sfx_volume = increase_audio_volume(sfx_volume);
                audio_settings_window->draw_sfx_vol();
                s = "sfx/ambtech1.wav";
                if (sound_is_initialized())
                    cache.sfx(cache.reg(s, s, SPEC_EXTERN_SFX, 1))->play(sfx_volume);
                break;
            case ID_SFX_DOWN:
                sfx_volume = decrease_audio_volume(sfx_volume);
                audio_settings_window->draw_sfx_vol();
                s = "sfx/ambtech1.wav";
                if (sound_is_initialized())
                    cache.sfx(cache.reg(s, s, SPEC_EXTERN_SFX, 1))->play(sfx_volume);
                break;

            case ID_MUSIC_UP:
                music_volume = increase_audio_volume(music_volume);
                audio_settings_window->draw_music_vol();
                if (current_song)
                    current_song->set_gain(music_volume);
                break;
            case ID_MUSIC_DOWN:
                music_volume = decrease_audio_volume(music_volume);
                audio_settings_window->draw_music_vol();
                if (current_song)
                    current_song->set_gain(music_volume);
                break;
            case ID_SOUNDFONT_PICKER:
                if (!apply_soundfont(audio_settings_window->selected_soundfont()))
                    audio_settings_window->select_soundfont(settings.soundfont);
                break;
            }
        }

        wm->flush_screen();
    }

    const std::string selected_soundfont = audio_settings_window->selected_soundfont();
    settings.volume_sound = sfx_volume;
    settings.volume_music = music_volume;
    if (!apply_soundfont(selected_soundfont))
        audio_settings_window->select_soundfont(settings.soundfont);
    if (!settings.Save())
        fprintf(stderr, "Unable to save audio settings\n");
    wm->close_window(window);
    delete audio_settings_window;
    audio_settings_window = nullptr;
}

void save_difficulty()
{
    if (DEFINEDP(symbol_value(l_difficulty)))
    {
        if (symbol_value(l_difficulty) == l_extreme)
            settings.difficulty = "extreme";
        else if (symbol_value(l_difficulty) == l_hard)
            settings.difficulty = "hard";
        else if (symbol_value(l_difficulty) == l_easy)
            settings.difficulty = "easy";
        else
            settings.difficulty = "medium";
    }
    else
        settings.difficulty = "medium";

    if (!settings.Save())
        fprintf(stderr, "Unable to save difficulty setting\n");
}

void fade_out(int steps);
void fade_in(image *im, int steps);

image *credits_hires = NULL;

static bool choose_new_game_campaign()
{
    char const *prompt_text = symbol_str("campaign_prompt");
    char const *abuse_text = "Abuse";
    char const *frabs_text = "fRaBs";
    const int gap = 8;
    const int content_x = Jwindow::left_border();
    const int content_y = Jwindow::top_border();

    int x1, y1, prompt_right, prompt_bottom;
    info_field prompt_bounds(content_x, content_y, ID_NULL, prompt_text, NULL);
    prompt_bounds.area(x1, y1, prompt_right, prompt_bottom);

    const int font_width = wm->font()->Size().x;
    const int abuse_width = static_cast<int>(strlen(abuse_text)) * font_width + 7;
    const int frabs_width = static_cast<int>(strlen(frabs_text)) * font_width + 7;
    const int controls_width = abuse_width + gap + frabs_width;
    const int content_width = std::max(prompt_right - content_x + 1, controls_width);
    const int button_y = prompt_bottom + 8;
    const int abuse_x = content_x + (content_width - controls_width) / 2;
    const int frabs_x = abuse_x + abuse_width + gap;

    button *frabs = new button(frabs_x, button_y, ID_CAMPAIGN_FRABS, frabs_text, NULL);
    button *abuse = new button(abuse_x, button_y, ID_CAMPAIGN_ABUSE, abuse_text, frabs);
    info_field *fields = new info_field(content_x, content_y, ID_NULL, prompt_text, abuse);
    Jwindow *window = wm->CreateWindow(ivec2(0), ivec2(-1), fields, symbol_str("ic_start"));
    wm->move_window(window, std::max(0, (xres - window->m_size.x) / 2),
                    std::max(0, (yres - window->m_size.y) / 2));
    wm->grab_focus(window);

    bool selected = false;
    bool done = false;
    wm->flush_screen();
    while (!done && !application_quit_requested())
    {
        Event event;
        do
        {
            wm->get_event(event);
        } while (event.type == EV_MOUSE_MOVE && wm->IsPending());

        if (event.type == EV_CLOSE_WINDOW || (event.type == EV_KEY && event.key == JK_ESC))
            done = true;
        else if (event.type == EV_MESSAGE &&
                 (event.message.id == ID_CAMPAIGN_ABUSE || event.message.id == ID_CAMPAIGN_FRABS))
        {
            strcpy(level_file,
                   event.message.id == ID_CAMPAIGN_FRABS ? "levels/frabs00.spe" : "levels/level00.spe");
            selected = true;
            done = true;
        }
        else if (event.type == EV_KEY &&
                 (event.key == get_key_binding("up", 0) || event.key == get_key_binding("up2", 0) ||
                  event.key == get_key_binding("down", 0) || event.key == get_key_binding("down2", 0) ||
                  event.key == get_key_binding("left", 0) || event.key == get_key_binding("left2", 0) ||
                  event.key == get_key_binding("right", 0) || event.key == get_key_binding("right2", 0)))
        {
            Event tab_event = event;
            tab_event.key = JK_TAB;
            window->inm->handle_event(tab_event, window);
        }

        wm->flush_screen();
    }

    wm->close_window(window);
    wm->flush_screen();
    return selected;
}

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
        case ID_GENERAL_SETTINGS:
            if (!audio_settings_window)
            {
                show_general_settings();
                inm->redraw();
            }
            break;
        case ID_LIGHT_OFF:
            if (!audio_settings_window)
            {
                show_video_settings(pal);
            }
            break;
        case ID_RETURN:
            if (!audio_settings_window)
            {
                the_game->set_state(RUN_STATE);
            }
            break;
        case ID_START_GAME:
            if (!audio_settings_window)
            {
                const bool bundled_campaign = !strcmp(level_file, "levels/level00.spe") ||
                                              !strcmp(level_file, "levels/frabs00.spe");
                if (bundled_campaign && !choose_new_game_campaign())
                    break;

                if (demo_man.is_automatic_recording())
                    demo_man.set_state(demo_manager::NORMAL);
                the_game->load_level(level_file);
                the_game->set_state(RUN_STATE);
                view *v;
                for (v = player_list; v; v = v->next)
                    if (v->m_focus)
                        v->reset_player();

                settings.quick_load = level_file; //AR
            }
            break;

        case ID_EDITOR:
            if (!audio_settings_window)
            {
                if (demo_man.is_automatic_recording())
                    demo_man.set_state(demo_manager::NORMAL);
                the_game->set_editor_mode(true);
            }
            break;

        case ID_LOAD_PLAYER_GAME:
            if (!audio_settings_window)
            {
                if (the_game->multiplayer_menu_active())
                {
                    if (load_player_game_enabled())
                    {
                        the_game->request_coop_restart();
                        the_game->set_state(RUN_STATE);
                    }
                    break;
                }

                int got_level = load_game(0, symbol_str("LOAD"));
                the_game->reset_keymap();
                if (got_level)
                {
                    if (demo_man.is_automatic_recording())
                        demo_man.set_state(demo_manager::NORMAL);
                    char name[255];
                    sprintf(name, "%ssave%04d.spe", get_save_filename_prefix(), got_level);

                    the_game->load_level(name);
                    the_game->set_state(RUN_STATE);

                    settings.quick_load = name; //AR
                }
            }
            break;

        case ID_VOLUME:
            if (!audio_settings_window)
            {
                create_audio_settings_window();
            }
            break;

        case ID_MEDIUM: {
            if (demo_man.is_automatic_recording())
                demo_man.set_state(demo_manager::NORMAL);
            l_difficulty->SetValue(l_medium);
            save_difficulty();
        }
        break;
        case ID_HARD: {
            if (demo_man.is_automatic_recording())
                demo_man.set_state(demo_manager::NORMAL);
            l_difficulty->SetValue(l_hard);
            save_difficulty();
        }
        break;
        case ID_EXTREME: {
            if (demo_man.is_automatic_recording())
                demo_man.set_state(demo_manager::NORMAL);
            l_difficulty->SetValue(l_extreme);
            save_difficulty();
        }
        break;
        case ID_EASY: {
            if (demo_man.is_automatic_recording())
                demo_man.set_state(demo_manager::NORMAL);
            l_difficulty->SetValue(l_easy);
            save_difficulty();
        }
        break;

        case ID_MULTIPLAYER: {
            if (!audio_settings_window)
            {
                net_configuration *cfg = new net_configuration;
                if (configure_multiplayer(*cfg))
                {
                    if (demo_man.is_automatic_recording())
                        demo_man.set_state(demo_manager::NORMAL);
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
            if (!audio_settings_window)
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
        if (audio_settings_window && ev.window == audio_settings_window->window())
        {
            wm->close_window(audio_settings_window->window());
            delete audio_settings_window;
            audio_settings_window = nullptr;
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

    ico_button *general_settings = load_icon(12, ID_GENERAL_SETTINGS, x, y, h, NULL, "ic_general");
    y += h;

    ico_button *start = load_icon(0, ID_START_GAME, x, y, h, NULL, "ic_start");
    start->set_enabled(!the_game->multiplayer_menu_active());
    y += h;

    //difficulty/hardness icon
    ico_switch_button *set = new ico_switch_button(
        x, y, ID_NULL, diff_on,
        load_icon(3, ID_EASY, x, y, h,
                  load_icon(8, ID_MEDIUM, x, y, h,
                            load_icon(9, ID_HARD, x, y, h,
                                      load_icon(10, ID_EXTREME, x, y, h, NULL, "ic_extreme"), "ic_hard"),
                            "ic_medium"),
                  "ic_easy"),
        NULL);
    y += h;

    ico_button *color = load_icon(4, ID_LIGHT_OFF, x, y, h, NULL, "ic_gamma");
    y += h;

    ico_button *volume = load_icon(5, ID_VOLUME, x, y, h, NULL, "ic_volume");
    y += h;

    ico_button *multiplayer = load_icon(11, ID_MULTIPLAYER, x, y, h, NULL, "ic_multiplayer");
    multiplayer->set_enabled(prot != NULL);
    y += h;

    //credits in full version
    // ico_button *sell = load_icon(2, ID_SHOW_SELL, x, y, h, NULL, "ic_sell");
    // y += h;

    ico_button *quit = load_icon(6, ID_QUIT, x, y, h, NULL, "ic_quit");
    y += h;

    //connect buttons/make list

    general_settings->next = start;
    start->next = set;
    set->next = color;

    color->next = volume;

    volume->next = multiplayer;
    multiplayer->next = quit;

    // sell->next = quit;

    ico_button *list = append_list;

    if (append_list)
    {
        while (append_list->next)
            append_list = (ico_button *)append_list->next;

        append_list->next = general_settings;
    }
    else
        list = general_settings;

    return list;
}

ico_button *make_context_buttons(int x, int &y, ico_button *&load_game_button)
{
    int h;

    ico_button *return_to_game = load_icon(7, ID_RETURN, x, y, h, NULL, "ic_return");
    return_to_game->set_enabled(current_level != NULL);
    y += h;

    load_game_button = load_icon(1, ID_LOAD_PLAYER_GAME, x, y, h, NULL, "ic_load");
    load_game_button->set_enabled(load_player_game_enabled());
    y += h;

    return_to_game->next = load_game_button;

    return return_to_game;
}

void main_menu()
{
    //AR enabled button selection with a controller, enabled highres button images

    // Build the list first so its actual artwork dimensions can drive the layout.
    int y = 0;
    ico_button *load_game_button;
    ico_button *list = make_context_buttons(0, y, load_game_button);
    list = make_default_buttons(0, y, list);

    int editor_h;
    ico_button *editor = load_icon(2, ID_EDITOR, 0, 0, editor_h, list, "ic_editor");
    list = editor;

    int icon_x1, icon_y1, icon_x2, icon_y2;
    list->area(icon_x1, icon_y1, icon_x2, icon_y2);
    const int button_w = icon_x2 - icon_x1 + 1;
    const int button_h = icon_y2 - icon_y1 + 1;
    const int padding_x = settings.hires ? 2 : 1;

    auto icon_column = [](const ifield *button) {
        switch (button->id)
        {
        case ID_GENERAL_SETTINGS:
        case ID_LIGHT_OFF:
        case ID_VOLUME:
        case ID_MULTIPLAYER:
        case ID_EDITOR:
            return 0;
        default:
            return 1;
        }
    };

    auto icon_order = [](const ifield *button) {
        switch (button->id)
        {
        case ID_GENERAL_SETTINGS:
        case ID_RETURN:
            return 0;
        case ID_LIGHT_OFF:
        case ID_NULL: // Difficulty selector
            return 1;
        case ID_VOLUME:
        case ID_START_GAME:
            return 2;
        case ID_MULTIPLAYER:
        case ID_LOAD_PLAYER_GAME:
            return 3;
        case ID_EDITOR:
        case ID_QUIT:
            return 4;
        default:
            return 5;
        }
    };

    int column_counts[2] = {0, 0};
    for (ifield *button = list; button; button = button->next)
        column_counts[icon_column(button)]++;

    // Split entries between two compact columns in the requested order.
    const int column_x[2] = {padding_x, xres - button_w - padding_x};
    const int column_y[2] = {(yres - column_counts[0] * button_h) / 2, (yres - column_counts[1] * button_h) / 2};

    for (ifield *button = list; button; button = button->next)
    {
        const int column = icon_column(button);
        int row = 0;
        for (ifield *candidate = list; candidate; candidate = candidate->next)
        {
            if (icon_column(candidate) == column && icon_order(candidate) < icon_order(button))
                row++;
        }
        button->Move(ivec2(column_x[column], column_y[column] + row * button_h));
    }

    //AR controller ui movement
    int selected_column = 1;
    int selected_row = 0;

    InputManager *inm = new InputManager(main_screen, list);
    inm->allow_no_selections();
    inm->clear_current();

    Event ev;

    int stop_menu = 0;
    time_marker start;
    Uint64 last_multiplayer_update = SDL_GetTicks();
    bool load_game_enabled = load_player_game_enabled();
    wm->flush_screen();
    do
    {
        time_marker new_time;

        if (the_game->multiplayer_menu_active() &&
            SDL_GetTicks() - last_multiplayer_update >= settings.physics_update)
        {
            the_game->run_multiplayer_menu_tick();
            last_multiplayer_update = SDL_GetTicks();

            if (the_game->multiplayer_menu_active())
            {
                const bool enabled = load_player_game_enabled();
                if (enabled != load_game_enabled)
                {
                    load_game_enabled = enabled;
                    load_game_button->set_enabled(enabled);
                    inm->redraw();
                    wm->flush_screen();
                }
            }
        }

        if (wm->IsPending())
        {
            do
            {
                wm->get_event(ev);
            } while (ev.type == EV_MOUSE_MOVE && wm->IsPending());
            inm->handle_event(ev, NULL);
            if (ev.type == EV_KEY && ev.key == JK_ESC && current_level)
                wm->PushMessage(ID_RETURN);

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
            if (audio_settings_window)
                start.get_time();
            else if (settings.menu_demos && !the_game->multiplayer_menu_active())
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

        if (audio_settings_window)
            stop_menu = 0; // can't exit with volume window open
        else if (main_net_cfg && main_net_cfg->restart_state())
            stop_menu = 1;
        else if (the_game->state == RUN_STATE)
            stop_menu = 1;
        else if (ev.type == EV_MESSAGE)
        {
            if (ev.message.id == ID_RETURN || ev.message.id == ID_EDITOR)
                stop_menu = 1;
            else if (ev.message.id == ID_QUIT)
            {
                if (demo_man.is_automatic_recording())
                    demo_man.set_state(demo_manager::NORMAL);
                stop_menu = 1;
            }
        }

        //AR move cursor over icons
        if (ev.type == EV_KEY)
        {
            bool navigation_key = true;
            if ((ev.key == get_key_binding("up", 0) || ev.key == get_key_binding("up2", 0)))
            {
                if (selected_row > 0)
                    selected_row--;
            }
            else if ((ev.key == get_key_binding("down", 0) || ev.key == get_key_binding("down2", 0)))
            {
                if (selected_row + 1 < column_counts[selected_column])
                    selected_row++;
            }
            else if ((ev.key == get_key_binding("left", 0) || ev.key == get_key_binding("left2", 0)))
            {
                selected_column = 0;
                selected_row = std::min(selected_row, column_counts[selected_column] - 1);
            }
            else if ((ev.key == get_key_binding("right", 0) || ev.key == get_key_binding("right2", 0)))
            {
                selected_column = 1;
                selected_row = std::min(selected_row, column_counts[selected_column] - 1);
            }
            else
                navigation_key = false;

            if (navigation_key)
                wm->SetMousePos(ivec2(column_x[selected_column] + button_w / 2,
                                      column_y[selected_column] + selected_row * button_h + button_h / 2));
        }
        //

    } while (!stop_menu && !application_quit_requested());

    delete inm;

    if (application_quit_requested() || (ev.type == EV_MESSAGE && ev.message.id == ID_QUIT)) // propagate quit
        the_game->end_session();
}
