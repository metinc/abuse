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

#include "game.h"

#include "netcfg.h"
#include "input.h"
#include "cache.h"
#include "timing.h"
#include "light.h"

#include "dev.h"

#include "net/sock.h"
#include <SDL_timer.h>
#include <dirent.h>
#include <vector>
#include <algorithm>
#include "imlib/scroller.h"
#include "file_utils.h"

// Static storage for available network levels (single-level selection UI)
static std::vector<std::string> g_net_levels; // filenames only (e.g. "2play1.spe")
static std::vector<char *> g_net_levels_c; // c_str pointers for pick_list

static void build_net_level_list()
{
    if (!g_net_levels.empty())
        return;
    const char *base = get_filename_prefix();
    std::string dir_path = base ? std::string(base) + "netlevel" : std::string("netlevel");
    DIR *d = opendir(dir_path.c_str());
    if (!d)
        return;
    struct dirent *ent;
    while ((ent = readdir(d)))
    {
        std::string n = ent->d_name;
        if (n.size() > 4 && n.substr(n.size() - 4) == ".spe")
            g_net_levels.push_back(n);
    }
    closedir(d);
    std::sort(g_net_levels.begin(), g_net_levels.end());
    g_net_levels_c.clear();
    for (auto &s : g_net_levels)
        g_net_levels_c.push_back((char *)s.c_str());
}

extern char const *get_login();
net_configuration *main_net_cfg = NULL;
extern char lsf[256];

extern net_protocol *prot;

net_configuration::net_configuration()
{
    strcpy(name, get_login());

    strcpy(server_name, "My Netgame");

    min_players = 2;
    max_players = 8;
    kills = 25;
    port = 20202;
    server_port = 20202;
    state = SINGLE_PLAYER;
}

extern char game_name[50];

enum
{
    NET_OK = 1,
    NET_CANCEL,
    NET_SERVER_NAME,
    NET_NAME,
    NET_PORT,
    NET_SERVER_PORT,
    NET_MAX,
    NET_MIN,
    NET_KILLS,
    CFG_ERR_OK,
    NET_SERVER,
    NET_CLIENT,
    NET_SINGLE,
    NET_GAME = 400,
    MIN_1,
    MIN_2,
    MIN_3,
    MIN_4,
    MIN_5,
    MIN_6,
    MIN_7,
    MIN_8,
    MAX_2,
    MAX_3,
    MAX_4,
    MAX_5,
    MAX_6,
    MAX_7,
    MAX_8,
    LVL_2,
    LVL_4,
    LVL_8,
    LEVEL_BOX
};

void net_configuration::cfg_error(char const *msg)
{
    Jwindow *j =
        wm->CreateWindow(ivec2(-1, 0), ivec2(-1),
                         new info_field(0, 0, 0, msg, new button(0, 30, CFG_ERR_OK, symbol_str("ok_button"), NULL)),
                         symbol_str("input_error"));
    Event ev;
    do
    {
        wm->flush_screen();
        do
        {
            wm->get_event(ev);
        } while (ev.type == EV_MOUSE_MOVE && wm->IsPending());
    } while (ev.type != EV_MESSAGE || ev.message.id != CFG_ERR_OK || ev.type == EV_CLOSE_WINDOW ||
             (ev.type == EV_KEY && ev.key == JK_ESC));
    wm->close_window(j);
    wm->flush_screen();
}

int net_configuration::restart_state()
{
    switch (state)
    {
    case RESTART_SERVER:
    case RESTART_CLIENT:
    case RESTART_SINGLE:
        return 1;
    default:
        break;
    }
    return 0;
}

int net_configuration::notify_reset()
{
    switch (state)
    {
    case RESTART_SERVER: {
        state = SERVER;
    }
    break;
    case RESTART_CLIENT: {
        state = CLIENT;
    }
    break;
    case RESTART_SINGLE: {
        state = SINGLE_PLAYER;
    }
    break;
    default:
        break;
    }

    return 1;
}

int net_configuration::confirm_inputs(InputManager *i, int server)
{
    if (server)
    {
        int kl;

        if (sscanf(i->get(NET_KILLS)->read(), "%d", &kl) != 1 || kl < 1 || kl > 99)
        {
            error(symbol_str("kill_error"));
            return 0;
        }

        char *nm = i->get(NET_NAME)->read();
        if (strstr(nm, "\""))
        {
            error(symbol_str("name_error"));
            return 0;
        }
        strcpy(name, nm);

        min_players = ((ifield *)(i->get(NET_MIN)->read()))->id - MIN_1 + 1;
        max_players = ((ifield *)(i->get(NET_MAX)->read()))->id - MAX_2 + 2;
        if (max_players < min_players)
        {
            error(symbol_str("max_error"));
            return 0;
        }

        char *s_nm = i->get(NET_SERVER_NAME)->read();
        if (strstr(s_nm, "\""))
        {
            error(symbol_str("game_error"));
            return 0;
        }

        strcpy(game_name, s_nm);

        bFILE *fp = open_file("addon/deathmat/gamename.lsp", "wb");
        if (!fp->open_failure())
        {
            char str[256];
            snprintf(str, sizeof(str), "(setq gamename \"%s\")\n", game_name);
            fp->write(str, strlen(str) + 1);
        }
        delete fp;
        strcpy(lsf, "addon/deathmat/deathmat.lsp");

        fp = open_file("addon/deathmat/levelset.lsp", "wb");
        if (!fp->open_failure())
        {
            int sel_index = -1;
            ifield *lvl_if = i->get(LEVEL_BOX);
            if (lvl_if)
            {
                pick_list *pl = (pick_list *)lvl_if; /* lvl_if is pick_list (LEVEL_BOX) */
                if (pl)
                    sel_index = pl->get_selection();
            }
            if (sel_index >= 0 && sel_index < (int)g_net_levels.size())
            {
                char str[512];
                // Prepend directory path
                snprintf(str, sizeof(str), "(setq net_levels '(\"netlevel/%s\"))\n", g_net_levels[sel_index].c_str());
                fp->write(str, strlen(str) + 1);
            }
        }
        delete fp;

        kills = kl;
    }
    else
    {
        char *nm = i->get(NET_NAME)->read();
        if (strstr(nm, "\""))
        {
            error(symbol_str("name_error"));
            return 0;
        }
        strcpy(name, nm);
    }

    bFILE *fp = open_file("addon/deathmat/username.lsp", "wb");
    if (!fp->open_failure())
    {
        char str[200];
        sprintf(str, "(setq username \"%s\")\n", name);
        fp->write(str, strlen(str) + 1);
    }
    delete fp;

    return 1;
}

extern int start_running, demo_start, start_edit;

void net_configuration::error(char const *message)
{
    image *screen_backup = main_screen->copy();

    image *ns = cache.img(cache.reg("art/frame.spe", "net_screen", SPEC_IMAGE, 1));
    int ns_w = ns->Size().x, ns_h = ns->Size().y;
    int x = (xres + 1) / 2 - ns_w / 2, y = (yres + 1) / 2 - ns_h / 2;
    main_screen->PutImage(ns, ivec2(x, y));
    JCFont *fnt = wm->font();

    uint8_t *remap = white_light + 30 * 256;

    uint8_t *sl = main_screen->scan_line(0);
    int xx = main_screen->Size().x * main_screen->Size().y;
    for (; xx; xx--, sl++)
        *sl = remap[*sl];

    int fx = x + ns_w / 2 - strlen(message) * fnt->Size().x / 2, fy = y + ns_h / 2 - fnt->Size().y;

    fnt->PutString(main_screen, ivec2(fx + 1, fy + 1), message, wm->black());
    fnt->PutString(main_screen, ivec2(fx, fy), message, wm->bright_color());

    {
        char const *ok = symbol_str("ok_button");

        int bx = x + ns_w / 2 - strlen(ok) * fnt->Size().x / 2 - 3, by = y + ns_h / 2 + fnt->Size().y * 3;

        button *sb = new button(bx, by, NET_SERVER, ok, NULL);

        InputManager inm(main_screen, sb);
        inm.allow_no_selections();
        inm.clear_current();

        int done = 0;
        Event ev;
        do
        {
            wm->flush_screen();
            do
            {
                wm->get_event(ev);
            } while (ev.type == EV_MOUSE_MOVE && wm->IsPending());
            inm.handle_event(ev, NULL);
            if ((ev.type == EV_KEY && (ev.key == JK_ESC || ev.key == JK_ENTER)) || ev.type == EV_MESSAGE)
                done = 1;
        } while (!done);
    }

    main_screen->PutImage(screen_backup, ivec2(0, 0));
    wm->flush_screen();
    delete screen_backup;
}

ifield *net_configuration::center_ifield(ifield *i, int x1, int x2, ifield *place_below)
{
    int X1, Y1, X2, Y2;
    i->area(X1, Y1, X2, Y2);
    i->m_pos.x = (x1 + x2) / 2 - (X2 - X1) / 2;

    if (place_below)
    {
        place_below->area(X1, Y1, X2, Y2);
        i->m_pos.y = Y2 + 2;
    }
    return i;
}

int net_configuration::get_options(int server)
{
    image *ns = cache.img(cache.reg("art/frame.spe", "net_screen", SPEC_IMAGE, 1));
    int ns_w = ns->Size().x, ns_h = ns->Size().y;
    int x = (xres + 1) / 2 - ns_w / 2, y = (yres + 1) / 2 - ns_h / 2;
    main_screen->PutImage(ns, ivec2(x, y));
    JCFont *fnt = wm->font();
    image *ok_image = cache.img(cache.reg("art/frame.spe", "dev_ok", SPEC_IMAGE, 1))->copy(),
          *cancel_image = cache.img(cache.reg("art/frame.spe", "cancel", SPEC_IMAGE, 1))->copy();

    ifield *list = NULL;

    if (server)
    {
        // Left column positioning
        int left_x = x + 40;
        int left_y = y + 30;
        int col_gap = 10;

        // Right column positioning
        int right_x = x + ns_w / 3 * 2;
        int right_y = y + 30;

        // Left column fields
        list = new text_field(left_x, left_y, NET_NAME, symbol_str("your_name"), "*******************", name, list);
        left_y += fnt->Size().y + col_gap;
        list = new text_field(left_x, left_y, NET_SERVER_NAME, symbol_str("server_name"), "*******************",
                              game_name, list);
        left_y += fnt->Size().y + 5;

        // Min players label & buttons
        info_field *min_lbl = new info_field(left_x, left_y, 0, symbol_str("min_play"), list);
        list = min_lbl;
        int ax1, ay1, ax2, ay2;
        min_lbl->area(ax1, ay1, ax2, ay2);
        left_y = ay2 + 4;
        button_box *b = new button_box(left_x, left_y, NET_MIN, 1, NULL, list);
        b->add_button(new button(0, 0, MIN_8, "8", NULL));
        b->add_button(new button(0, 0, MIN_7, "7", NULL));
        b->add_button(new button(0, 0, MIN_6, "6", NULL));
        b->add_button(new button(0, 0, MIN_5, "5", NULL));
        b->add_button(new button(0, 0, MIN_4, "4", NULL));
        b->add_button(new button(0, 0, MIN_3, "3", NULL));
        button *r = new button(0, 0, MIN_2, "2", NULL);
        r->push();
        b->add_button(r);
        b->add_button(new button(0, 0, MIN_1, "1", NULL));
        b->arrange_left_right();
        list = b;
        int bx1, by1, bx2, by2;
        b->area(bx1, by1, bx2, by2);
        left_y = by2 + col_gap;

        // Max players label & buttons
        info_field *max_lbl = new info_field(left_x, left_y, 0, symbol_str("max_play"), list);
        list = max_lbl;
        max_lbl->area(ax1, ay1, ax2, ay2);
        left_y = ay2 + 4;
        b = new button_box(left_x, left_y, NET_MAX, 1, NULL, list);
        button *q = new button(0, 0, MAX_8, "8", NULL);
        q->push();
        b->add_button(q);
        b->add_button(new button(0, 0, MAX_7, "7", NULL));
        b->add_button(new button(0, 0, MAX_6, "6", NULL));
        b->add_button(new button(0, 0, MAX_5, "5", NULL));
        b->add_button(new button(0, 0, MAX_4, "4", NULL));
        b->add_button(new button(0, 0, MAX_3, "3", NULL));
        b->add_button(new button(0, 0, MAX_2, "2", NULL));
        b->arrange_left_right();
        list = b;
        b->area(bx1, by1, bx2, by2);
        left_y = by2 + col_gap;

        // Kills field
        list = new text_field(left_x, left_y, NET_KILLS, symbol_str("kills_to_win"), "***", "25", list);

        // Right column : level selection list
        build_net_level_list();
        if (!g_net_levels.empty())
        {
            list = new info_field(right_x, right_y, 0, symbol_str("select_level"), list);
            right_y += fnt->Size().y + 4;
            pick_list *pl = new pick_list(right_x, right_y, LEVEL_BOX, 15, g_net_levels_c.data(),
                                          (int)g_net_levels_c.size(), 0, list, cache.img(window_texture));
            list = pl;
        }
    }
    else
    {
        list = center_ifield(
            new text_field(x, y + 80, NET_NAME, symbol_str("your_name"), "************************", name, list), x,
            x + ns_w, NULL);
    }

    list = new button(x + 80 - 17, y + ns_h - 20 - fnt->Size().y, NET_OK, ok_image, list);
    list = new button(x + 80 + 17, y + ns_h - 20 - fnt->Size().y, NET_CANCEL, cancel_image, list);

    int ret = 0;

    {
        InputManager inm(main_screen, list);
        inm.allow_no_selections();
        inm.clear_current();

        int done = 0;
        Event ev;
        do
        {
            wm->flush_screen();
            do
            {
                wm->get_event(ev);
            } while (ev.type == EV_MOUSE_MOVE && wm->IsPending());
            inm.handle_event(ev, NULL);
            if (ev.type == EV_MESSAGE)
            {
                switch (ev.message.id)
                {
                case NET_OK: {
                    if (confirm_inputs(&inm, server))
                    {
                        ret = 1;
                        done = 1;
                    }
                    else
                    {
                        ((button *)inm.get(NET_OK))->push();
                        inm.redraw();
                    }
                }
                break;
                case NET_CANCEL:
                    done = 1;
                }
            }
            if (ev.type == EV_KEY && ev.key == JK_ESC)
                done = 1;

        } while (!done);
    }
    delete ok_image;
    delete cancel_image;

    return ret;
}

int net_configuration::input() // pulls up dialog box and input fileds
{
    int ret = 0;
    main_screen->clear();

    image *ns = cache.img(cache.reg("art/frame.spe", "net_screen", SPEC_IMAGE, 1));
    int ns_w = ns->Size().x, ns_h = ns->Size().y;
    int x = (xres + 1) / 2 - ns_w / 2, y = (yres + 1) / 2 - ns_h / 2;
    main_screen->PutImage(ns, ivec2(x, y));
    char const *nw_s = symbol_str("Networking");
    JCFont *fnt = wm->font();

    wm->font()->PutString(main_screen,
                          ivec2(x + ns_w / 2 - strlen(nw_s) * fnt->Size().x / 2, y + 21 / 2 - fnt->Size().y / 2), nw_s,
                          wm->medium_color());
    {

        char const *server_str = symbol_str("server");
        button *sb = new button(x + 40, y + ns_h - 23 - fnt->Size().y, NET_SERVER, server_str, NULL);

        if (main_net_cfg && (main_net_cfg->state == CLIENT || main_net_cfg->state == SERVER))
            sb = new button(x + 40, y + ns_h - 9 - fnt->Size().y, NET_SINGLE, symbol_str("single_play"), sb);

        InputManager inm(main_screen, sb);

        inm.allow_no_selections();
        inm.clear_current();

        Event ev;
        int done = 0;
        int button_y = 25, total_games = 0;
        enum
        {
            MAX_GAMES = 9
        };
        net_address *game_addr[MAX_GAMES + 1];
        int join_game = -1;
        time_marker start, now;

        do
        {
            if (wm->IsPending())
            {
                do
                {
                    wm->get_event(ev);
                } while (ev.type == EV_MOUSE_MOVE && wm->IsPending());
                inm.handle_event(ev, NULL);
                if (ev.type == EV_MESSAGE)
                {
                    switch (ev.message.id)
                    {
                    case NET_CANCEL:
                        done = 1;
                        break;
                    case NET_SERVER:
                        done = 1;
                        break;
                    case NET_SINGLE:
                        done = 1;
                        break;
                    default:
                        if (ev.message.id >= NET_GAME && ev.message.id < NET_GAME + MAX_GAMES)
                        {
                            join_game = ev.message.id - NET_GAME;
                            done = 1;
                        }
                    }
                }
                else if (ev.type == EV_KEY && ev.key == JK_ESC)
                {
                    done = 1;
                }
                else
                {
                    // No event waiting...  We can't wait for long, because we are
                    // pretending to broadcast.
                    // ECS - Added so waiting in dialog doesn't use 100% of CPU
                    SDL_Delay(5);
                }
            }

            wm->flush_screen();
            char name[256];

            now.get_time();
            if (total_games < MAX_GAMES && now.diff_time(&start) > 0.5)
            {
                start.get_time();
                net_address *find = prot->find_address(0x9090, name); // was server_port
                if (find)
                {
                    int bw = strlen(name) * fnt->Size().x;
                    inm.add(new button(x + ns_w / 2 - bw / 2, y + button_y, NET_GAME + total_games, name, NULL));
                    find->set_port(server_port);
                    game_addr[total_games] = find;

                    total_games++;
                    button_y += fnt->Size().y + 10;
                    inm.redraw();
                }
            }

        } while (!done);

        prot->reset_find_list();

        if (join_game >= 0)
        {
            if (get_options(0))
            {
                int still_there = 1; // change this back to 0, to check if games are stil alive
                time_marker start, now;
                do
                {
                    now.get_time();
                    char name[256];
                    net_address *find = prot->find_address(0x9090, name); // was server_port
                    if (find)
                    {
                        if (find->equal(game_addr[join_game]))
                            still_there = 1;
                        delete find;
                    }

                } while (now.diff_time(&start) < 3 && !still_there);

                if (still_there)
                {
                    game_addr[join_game]->store_string(server_name, sizeof(server_name));
                    state = RESTART_CLIENT;
                    ret = 1;
                }
                else
                    error(symbol_str("not_there"));

                prot->reset_find_list();
                int i;
                for (i = 0; i < total_games; i++) // delete all the addresses we found and stored
                    delete game_addr[join_game];
            }
        }
        else if (ev.type == EV_MESSAGE && ev.message.id == NET_SERVER)
        {
            if (get_options(1))
            {
                state = RESTART_SERVER;
                return 1;
            }
            else
                return 0;
        }
        else if (ev.type == EV_MESSAGE && ev.message.id == NET_SINGLE)
        {
            state = RESTART_SINGLE;
            start_running = 0;

            strcpy(lsf, "abuse.lsp");
            return 1;
        }
    }

    return ret;
}
