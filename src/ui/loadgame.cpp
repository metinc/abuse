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

#include <string.h>

#include "common.h"

#include "game.h"
#include "specache.h"

#include "specs.h"
#include "jwindow.h"
#include "id.h"
#include "input.h"
#include "fonts.h"
#include "lisp.h"
#include "cache.h"
#include "gui.h"
#include "dev.h"
#include "id.h"
#include "demo.h"

//AR
#include "sdlport/setup.h"
#include <SDL3/SDL_timer.h>
extern Settings settings;
extern int get_key_binding(char const *dir, int i);
//

extern void *save_order; // load from "saveordr.lsp", contains a list ordering the save games

extern JCFont *console_font;

#define MAX_SAVE_GAMES 15
#define MAX_SAVE_LINES 5
int last_save_game_number = 0;

int save_buts[MAX_SAVE_GAMES * 3];

namespace
{
struct SlotButtons
{
    ico_button *first = nullptr;
    int width = 0;
    int height = 0;
    int button_width = 0;
    int button_height = 0;
};

SlotButtons create_slot_buttons(int total_saved, int rows, image **thumbnails)
{
    SlotButtons result;
    result.button_width = cache.img(save_buts[0])->Size().x;
    result.button_height = cache.img(save_buts[0])->Size().y;

    ico_button *last = nullptr;
    int slot = 0;
    for (int index = 0; index < total_saved; index++)
    {
        while (thumbnails && slot < MAX_SAVE_GAMES && !thumbnails[slot])
            slot++;

        const int x = index / rows * result.button_width;
        const int y = index % rows * result.button_height;
        ico_button *button =
            new ico_button(x, y, ID_LOAD_GAME_NUMBER + slot, save_buts[slot * 3 + 0], save_buts[slot * 3 + 0],
                           save_buts[slot * 3 + 1], save_buts[slot * 3 + 2], nullptr);
        button->set_act_id(ID_LOAD_GAME_PREVIEW + slot);

        if (last)
            last->next = button;
        else
            result.first = button;
        last = button;
        slot++;
    }

    result.width = (total_saved + rows - 1) / rows * result.button_width;
    result.height = std::min(total_saved, rows) * result.button_height;
    return result;
}

ivec2 centered_window_position(ivec2 client_size)
{
    const ivec2 total_size = client_size + ivec2(Jwindow::left_border() + Jwindow::right_border(),
                                                 Jwindow::top_border() + Jwindow::bottom_border());
    return ivec2(std::max(0, (xres - total_size.x) / 2), std::max(0, (yres - total_size.y) / 2));
}

void draw_preview(Jwindow *window, image *thumbnail, ivec2 position, ivec2 size)
{
    window->m_surf->Bar(position, position + size - ivec2(1), wm->dark_color());
    const ivec2 content_position = position + ivec2(1);
    const ivec2 content_size = size - ivec2(2);
    window->m_surf->Bar(content_position, content_position + content_size - ivec2(1), window->backg);
    window->m_surf->PutImage(thumbnail, content_position + (content_size - thumbnail->Size()) / 2);
}
}

void load_number_icons()
{
    for (int i = 0; i < MAX_SAVE_GAMES * 3; i++)
    {
        char name[100];
        sprintf(name, "nums%04d.pcx", i + 1);

        spec_directory *sd = sd_cache.get_spec_directory("art/icons.spe");
        if (!sd || !sd->find(name))
        {
            printf("File not found in cache: %s. Stopping further loading.\n", name);
            break; //
        }

        save_buts[i] = cache.reg("art/icons.spe", name, SPEC_IMAGE, 1);
    }
}

void last_savegame_name(char *buf)
{
    sprintf(buf, "%ssave%04d.spe", get_save_filename_prefix(),
            (last_save_game_number + MAX_SAVE_GAMES - 1) % MAX_SAVE_GAMES + 1);
}

int get_save_spot()
{
    int last_free = 0;
    for (int i = MAX_SAVE_GAMES; i > 0;)
    {
        std::string name = get_save_path(i);
        FILE *fp = prefix_fopen(name.c_str(), "rb");
        if (fp)
            i = 0;
        else
        {
            last_free = i;
            i--;
        }
        fclose(fp);
    }

    if (last_free)
        return last_free; // if there are any slots not created yet...

    const SlotButtons slots = create_slot_buttons(MAX_SAVE_GAMES, MAX_SAVE_LINES, nullptr);
    const ivec2 client_size(slots.width, slots.height);
    Jwindow *l_win =
        wm->CreateWindow(centered_window_position(client_size), client_size, slots.first, symbol_str("SAVE"));
    Event ev;
    int got_level = 0;
    int quit = 0;
    do
    {
        wm->flush_screen();
        wm->get_event(ev);
        if (ev.type == EV_MESSAGE && ev.message.id >= ID_LOAD_GAME_NUMBER && ev.message.id < ID_LOAD_GAME_PREVIEW)
            got_level = ev.message.id - ID_LOAD_GAME_NUMBER + 1;

        if (ev.type == EV_CLOSE_WINDOW && ev.window == l_win)
            quit = 1;
    } while (!got_level && !quit);

    wm->close_window(l_win);
    the_game->reset_keymap();
    return got_level;
}

void get_savegame_name(char *buf) // buf should be at least 50 bytes
{
    sprintf(buf, "save%04d.spe", (last_save_game_number++) % MAX_SAVE_GAMES + 1);
    /*  FILE *fp=prefix_fopen("lastsave.lsp","wb");
  if (fp)
  {
    fprintf(fp,"(setq last_save_game %d)\n",last_save_game_number%MAX_SAVE_GAMES);
    fclose(fp);
  } else printf("Warning unable to open lastsave.lsp for writing\n"); */
}

int show_load_icon()
{
    for (int slot = 0; slot < MAX_SAVE_GAMES; slot++)
    {
        std::string path = get_save_path(slot);
        bFILE *fp = open_file(path.c_str(), "rb");
        if (fp->open_failure())
        {
            delete fp;
        }
        else
        {
            delete fp;
            return 1;
        }
    }
    return 0;
}

int load_game(int show_all,
              char const *title) // return 0 if the player escapes, else return the number of the game to load
{
    //AR this creates the small load/save game window
    //and takes complete control of the program until it leaves the loop
    //it is called via clisp.cpp (case 263)

    int total_saved = 0;
    image *thumbnails[MAX_SAVE_GAMES];
    int max_w = 160, max_h = 100;
    memset(thumbnails, 0, sizeof(thumbnails));

    image *first = NULL;

    for (int slot = 0; slot < MAX_SAVE_GAMES; slot++)
    {
        int fail = 0;

        std::string path = get_save_path(slot);
        bFILE *fp = open_file(path.c_str(), "rb");
        if (fp->open_failure())
        {
            fail = 1;
        }
        else
        {
            spec_directory sd(fp);
            spec_entry *se = sd.find("thumb nail");
            if (se && se->type == SPEC_IMAGE)
            {
                thumbnails[slot] = new image(fp, se);
                if (thumbnails[slot]->Size().x > max_w)
                    max_w = thumbnails[slot]->Size().x;
                if (thumbnails[slot]->Size().y > max_h)
                    max_h = thumbnails[slot]->Size().y;
                if (!first)
                    first = thumbnails[slot];
                total_saved++;
            }
            else
                fail = 1;
        }
        if (fail && show_all)
        {
            thumbnails[slot] = new image(ivec2(160, 100));
            thumbnails[slot]->clear();
            console_font->PutString(thumbnails[slot], ivec2(0), symbol_str("no_saved"));
            total_saved++;
            if (!first)
                first = thumbnails[slot];
        }
        delete fp;
    }

    if (!total_saved)
        return 0;
    if (total_saved > MAX_SAVE_GAMES)
        total_saved = MAX_SAVE_GAMES;

    constexpr int preview_gap = 5;
    const SlotButtons slots = create_slot_buttons(total_saved, MAX_SAVE_LINES, thumbnails);
    const ivec2 preview_size(max_w + 2, max_h + 2);
    const ivec2 client_size(slots.width + preview_gap + preview_size.x, std::max(slots.height, preview_size.y));
    Jwindow *window = wm->CreateWindow(centered_window_position(client_size), client_size, slots.first, title);
    const ivec2 preview_position(window->x1() + slots.width + preview_gap,
                                 window->y1() + (client_size.y - preview_size.y) / 2);
    draw_preview(window, first, preview_position, preview_size);

    // AR controller UI movement
    int mx, my; //mouse position
    const ivec2 button_origin = window->m_pos + ivec2(window->x1(), window->y1());

    //AR initial position of the mouse in the window for controller use
    mx = button_origin.x + slots.button_width / 2;
    my = button_origin.y + slots.button_height / 2;
    //

    Event ev;
    int got_level = 0;
    int quit = 0;
    do
    {
        if (wm->IsPending())
        {
            wm->flush_screen();
            do
            {
                wm->get_event(ev);
                if (ev.type == EV_MESSAGE && ev.message.id >= ID_LOAD_GAME_NUMBER &&
                    ev.message.id < ID_LOAD_GAME_PREVIEW)
                    got_level = ev.message.id - ID_LOAD_GAME_NUMBER + 1;

                if (ev.type == EV_MESSAGE && ev.message.id >= ID_LOAD_GAME_PREVIEW &&
                    ev.message.id < ID_LOAD_PLAYER_GAME)
                {
                    int draw_num = ev.message.id - ID_LOAD_GAME_PREVIEW;
                    draw_preview(window, thumbnails[draw_num], preview_position, preview_size);
                }

                if ((ev.type == EV_CLOSE_WINDOW && ev.window == window) || (ev.type == EV_KEY && ev.key == JK_ESC))
                    quit = 1;

                //AR move cursor over icons
                if (ev.type == EV_KEY)
                {
                    if ((ev.key == get_key_binding("left", 0) || ev.key == get_key_binding("left2", 0)))
                    {
                        if (mx - slots.button_width > button_origin.x)
                            mx -= slots.button_width;
                        wm->SetMousePos(ivec2(mx, my));
                    }
                    if ((ev.key == get_key_binding("right", 0) || ev.key == get_key_binding("right2", 0)))
                    {
                        if (mx + slots.button_width < button_origin.x + slots.width)
                            mx += slots.button_width;
                        wm->SetMousePos(ivec2(mx, my));
                    }
                    if ((ev.key == get_key_binding("up", 0) || ev.key == get_key_binding("up2", 0)))
                    {
                        if (my - slots.button_height > button_origin.y)
                            my -= slots.button_height;
                        wm->SetMousePos(ivec2(mx, my));
                    }
                    if ((ev.key == get_key_binding("down", 0) || ev.key == get_key_binding("down2", 0)))
                    {
                        if (my + slots.button_height < button_origin.y + slots.height)
                            my += slots.button_height;
                        wm->SetMousePos(ivec2(mx, my));
                    }
                }
                //
            } while (ev.type == EV_MOUSE_MOVE && wm->IsPending());
        }
        else
        {
            // add timer so that load dialog doesn't grab 100% of CPU
            SDL_Delay(10);
        }
    } while (!got_level && !quit);

    wm->close_window(window);

    for (image *thumbnail : thumbnails)
        delete thumbnail;

    return got_level;
}
