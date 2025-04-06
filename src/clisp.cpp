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

#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef WIN32
#include <direct.h>
#endif

#include "common.h"

#include "sdlport/joy.h"

#include "ant.h"
#include "lisp.h"
#include "game.h"
#include "jrand.h"
#include "dev.h"
#include "pcxread.h"
#include "menu.h"
#include "clisp.h"
#include "chars.h"
#include "lisp_gc.h"
#include "cop.h"
#include "loadgame.h"
#include "nfserver.h"
#include "demo.h"
#include "chat.h"
#include "jdir.h"
#include "netcfg.h"
#include "funcs.h"

//AR
#include "sdlport/setup.h"
extern Settings settings;
//

//AR
#include "sdlport/setup.h"
extern Settings settings;
//

#define ENGINE_MAJOR 1
#define ENGINE_MINOR 20

extern int has_joystick;

// the following are references to lisp symbols
LSymbol *l_chat_input, *l_post_render;

LSymbol *l_difficulty, *l_easy, *l_hard, *l_medium, *l_extreme, *l_max_hp, *l_max_power, *l_empty_cache;

// FIXME: port these to LSymbol
void *l_main_menu, *l_logo, *l_state_art, *l_abilities, *l_state_sfx, *l_song_list, *l_filename, *l_sfx_directory,
    *l_default_font, *l_morph, *l_default_abilities, *l_default_ai_function, *l_tile_files, *l_range, *l_hurt_all,
    *l_death_handler, *l_title_screen, *l_console_font, *l_fields, *l_dist, *l_pushx, *l_pushy, *l_object, *l_tile,
    *l_fire_object, *l_FIRE, *l_cop_dead_parts, *l_restart_player, *l_help_screens, *l_player_draw, *l_sneaky_draw,
    *l_health_image, *l_fly_image, *l_sneaky_image, *l_draw_fast, *l_player_tints, *l_save_order, *l_next_song,
    *l_level_load_start, *l_level_load_end, *l_cdc_logo, *l_keep_backup, *l_switch_to_powerful, *l_mouse_can_switch,
    *l_ask_save_slot, *l_player_text_color,
    *l_level_loaded; // called when a new level is loaded

char game_name[50];
void *sensor_ai();

// variables for the status bar
void *l_statbar_ammo_x, *l_statbar_ammo_y, *l_statbar_ammo_w, *l_statbar_ammo_h, *l_statbar_ammo_bg_color,

    *l_statbar_health_x, *l_statbar_health_y, *l_statbar_health_w, *l_statbar_health_h, *l_statbar_health_bg_color,

    *l_statbar_logo_x, *l_statbar_logo_y;
uint8_t chatting_enabled = 0;

extern void show_end();

static view *lget_view(void *arg, char const *msg)
{
    game_object *o = (game_object *)lpointer_value(arg);
    view *c = o->controller();
    if (!c)
    {
        printf("%s : object does not have a view\n", msg);
        lbreak("");
        exit(EXIT_SUCCESS);
    }
    return c;
}

extern int get_option(char const *name);
extern void set_login(char const *name);

// called by lisp_init, defines symbols and functions to interface with c
void clisp_init()
{
    l_easy = LSymbol::FindOrCreate("easy");
    l_medium = LSymbol::FindOrCreate("medium");
    l_hard = LSymbol::FindOrCreate("hard");
    l_extreme = LSymbol::FindOrCreate("extreme");

    l_logo = LSymbol::FindOrCreate("logo");
    l_morph = LSymbol::FindOrCreate("morph");

    l_pushx = LSymbol::FindOrCreate("pushx");
    l_pushy = LSymbol::FindOrCreate("pushy");

    l_dist = LSymbol::FindOrCreate("dist");
    l_state_art = LSymbol::FindOrCreate("state_art");
    l_abilities = LSymbol::FindOrCreate("abilities");
    l_default_abilities = LSymbol::FindOrCreate("default_abilities");
    l_state_sfx = LSymbol::FindOrCreate("state_sfx");
    l_filename = LSymbol::FindOrCreate("filename");
    l_sfx_directory = LSymbol::FindOrCreate("sfx_directory");
    l_default_font = LSymbol::FindOrCreate("default_font");
    l_console_font = LSymbol::FindOrCreate("console_font");
    l_default_ai_function = LSymbol::FindOrCreate("default_ai");
    l_tile_files = LSymbol::FindOrCreate("tile_files");
    l_empty_cache = LSymbol::FindOrCreate("empty_cache");
    l_range = LSymbol::FindOrCreate("range");
    l_difficulty = LSymbol::FindOrCreate("difficulty");
    l_death_handler = LSymbol::FindOrCreate("death_handler");
    l_title_screen = LSymbol::FindOrCreate("title_screen");
    l_fields = LSymbol::FindOrCreate("fields");
    l_FIRE = LSymbol::FindOrCreate("FIRE");
    l_fire_object = LSymbol::FindOrCreate("fire_object");
    l_cop_dead_parts = LSymbol::FindOrCreate("cop_dead_parts");
    l_difficulty->SetValue(l_hard);
    l_restart_player = LSymbol::FindOrCreate("restart_player");
    l_help_screens = LSymbol::FindOrCreate("help_screens");
    l_save_order = LSymbol::FindOrCreate("save_order");
    l_next_song = LSymbol::FindOrCreate("next_song");
    l_player_draw = LSymbol::FindOrCreate("player_draw");
    l_sneaky_draw = LSymbol::FindOrCreate("sneaky_draw");
    l_keep_backup = LSymbol::FindOrCreate("keep_backup");
    l_level_loaded = LSymbol::FindOrCreate("level_loaded");

    l_draw_fast = LSymbol::FindOrCreate("draw_fast");
    l_player_tints = LSymbol::FindOrCreate("player_tints");

    l_max_hp = LSymbol::FindOrCreate("max_hp");
    l_max_hp->SetNumber(200);
    l_max_power = LSymbol::FindOrCreate("max_power");
    l_main_menu = LSymbol::FindOrCreate("main_menu");
    l_max_power->SetNumber(999);

    LSymbol::FindOrCreate("run_state")->SetNumber(RUN_STATE);
    LSymbol::FindOrCreate("pause_state")->SetNumber(PAUSE_STATE);
    LSymbol::FindOrCreate("menu_state")->SetNumber(MENU_STATE);
    LSymbol::FindOrCreate("scene_state")->SetNumber(SCENE_STATE);

    l_statbar_ammo_x = LSymbol::FindOrCreate("statbar_ammo_x");
    l_statbar_ammo_y = LSymbol::FindOrCreate("statbar_ammo_y");
    l_statbar_ammo_w = LSymbol::FindOrCreate("statbar_ammo_w");
    l_statbar_ammo_h = LSymbol::FindOrCreate("statbar_ammo_h");
    l_statbar_ammo_bg_color = LSymbol::FindOrCreate("statbar_ammo_bg_color");

    l_statbar_health_x = LSymbol::FindOrCreate("statbar_health_x");
    l_statbar_health_y = LSymbol::FindOrCreate("statbar_health_y");
    l_statbar_health_w = LSymbol::FindOrCreate("statbar_health_w");
    l_statbar_health_h = LSymbol::FindOrCreate("statbar_health_h");
    l_statbar_health_bg_color = LSymbol::FindOrCreate("statbar_health_bg_color");

    l_statbar_logo_x = LSymbol::FindOrCreate("statbar_logo_x");
    l_statbar_logo_y = LSymbol::FindOrCreate("statbar_logo_y");
    l_object = LSymbol::FindOrCreate("object");
    l_tile = LSymbol::FindOrCreate("tile");
    l_cdc_logo = LSymbol::FindOrCreate("logo");

    l_switch_to_powerful = LSymbol::FindOrCreate("switch_to_powerful");
    l_mouse_can_switch = LSymbol::FindOrCreate("mouse_can_switch");
    l_ask_save_slot = LSymbol::FindOrCreate("ask_save_slot");

    l_level_load_start = LSymbol::FindOrCreate("level_load_start");
    l_level_load_end = LSymbol::FindOrCreate("level_load_end");
    l_chat_input = LSymbol::FindOrCreate("chat_input");
    l_player_text_color = LSymbol::FindOrCreate("player_text_color");

    int i;
    for (i = 0; i < MAX_STATE; i++)
        LSymbol::FindOrCreate(state_names[i])->SetNumber(i);
    for (i = 0; i < TOTAL_ABILITIES; i++)
        LSymbol::FindOrCreate(ability_names[i])->SetNumber(i);
    for (i = 0; i < TOTAL_CFLAGS; i++)
        LSymbol::FindOrCreate(cflag_names[i])->SetNumber(i);

    l_song_list = LSymbol::FindOrCreate("song_list");
    l_post_render = LSymbol::FindOrCreate("post_render");

    initCFuncs();
    initLispFuncs();
}

// Note : args for l_caller have not been evaluated yet!
void *l_caller(LispFunc number, void *args)
{
    PtrRef r1(args);
    switch (number)
    {
    case LispFunc::GoState: {
        current_object->set_aistate(lnumber_value(CAR(args)->Eval()));
        current_object->set_aistate_time(0);
        void *ai = figures[current_object->otype]->get_fun(OFUN_AI);
        if (!ai)
        {
            lbreak("hrump... call to go_state, and no ai function defined?\n"
                   "Are you calling from move function (not mover)?\n");
            exit(EXIT_SUCCESS);
        }
        return ((LSymbol *)ai)->EvalFunction(NULL);
    }
    break;
    case LispFunc::WithObject: {
        game_object *old_cur = current_object;
        current_object = (game_object *)lpointer_value(CAR(args)->Eval());
        void *ret = eval_block(CDR(args));
        current_object = old_cur;
        return ret;
    }
    break;

    case LispFunc::BMove: {
        int collision;
        game_object *o;
        if (args)
            o = (game_object *)lpointer_value(CAR(args)->Eval());
        else
            o = current_object;
        game_object *hit = current_object->bmove(collision, o);
        if (hit)
            return LPointer::Create(hit);
        else if (collision)
            return NULL;
        else
            return true_symbol;
    }
    break;

    case LispFunc::Me:
        return LPointer::Create(current_object);
        break;
    case LispFunc::Bg: {
        if (player_list->next)
            return LPointer::Create(current_level->attacker(current_object));
        else
            return LPointer::Create(player_list->m_focus);
    }
    break;
    case LispFunc::FindClosest:
        return LPointer::Create(current_level->find_closest(current_object->x, current_object->y,
                                                            lnumber_value(CAR(args)->Eval()), current_object));
        break;
    case LispFunc::FindXClosest:
        return LPointer::Create(current_level->find_xclosest(current_object->x, current_object->y,
                                                             lnumber_value(CAR(args)->Eval()), current_object));
        break;
    case LispFunc::FindXRange: {
        long n1 = lnumber_value(CAR(args)->Eval());
        long n2 = lnumber_value(CAR(CDR(args))->Eval());
        return LPointer::Create(current_level->find_xrange(current_object->x, current_object->y, n1, n2));
    }
    break;
    case LispFunc::AddObject: {
        int type = lnumber_value(CAR(args)->Eval());
        args = CDR(args);
        long x = lnumber_value(CAR(args)->Eval());
        args = CDR(args);
        long y = lnumber_value(CAR(args)->Eval());
        args = CDR(args);
        game_object *o;
        if (args)
            o = create(type, x, y, 0, lnumber_value(CAR(args)->Eval()));
        else
            o = create(type, x, y);
        if (current_level)
            current_level->add_object(o);
        return LPointer::Create(o);
    }
    break;
    case LispFunc::AddObjectAfter: {
        int type = lnumber_value(CAR(args)->Eval());
        args = CDR(args);
        long x = lnumber_value(CAR(args)->Eval());
        args = CDR(args);
        long y = lnumber_value(CAR(args)->Eval());
        args = CDR(args);
        game_object *o;
        if (args)
            o = create(type, x, y, 0, lnumber_value(CAR(args)->Eval()));
        else
            o = create(type, x, y);
        if (current_level)
            current_level->add_object_after(o, current_object);
        return LPointer::Create(o);
    }
    break;

    case LispFunc::FirstFocus:
        return LPointer::Create(the_game->first_view->m_focus);
        break;
    case LispFunc::NextFocus: {
        view *v = ((game_object *)lpointer_value(CAR(args)->Eval()))->controller()->next;
        if (v)
            return LPointer::Create(v->m_focus);
        else
            return NULL;
    }
    break;
    case LispFunc::GetObject: {
        return LPointer::Create((void *)current_object->get_object(lnumber_value(CAR(args)->Eval())));
    }
    break;
    case LispFunc::GetLight: {
        return LPointer::Create((void *)current_object->get_light(lnumber_value(CAR(args)->Eval())));
    }
    break;
    case LispFunc::WithObjects: {
        game_object *old_cur = current_object;
        void *ret = NULL;
        for (int i = 0; i < old_cur->total_objects(); i++)
        {
            current_object = old_cur->get_object(i);
            ret = CAR(args)->Eval();
        }
        current_object = old_cur;
        return ret;
    }
    break;
    case LispFunc::AddLight: {
        int t = lnumber_value(CAR(args)->Eval());
        args = lcdr(args);
        int x = lnumber_value(CAR(args)->Eval());
        args = lcdr(args);
        int y = lnumber_value(CAR(args)->Eval());
        args = lcdr(args);
        int r1 = lnumber_value(CAR(args)->Eval());
        args = lcdr(args);
        int r2 = lnumber_value(CAR(args)->Eval());
        args = lcdr(args);
        int xs = lnumber_value(CAR(args)->Eval());
        args = lcdr(args);
        int ys = lnumber_value(CAR(args)->Eval());
        return LPointer::Create(add_light_source(t, x, y, r1, r2, xs, ys));
    }
    break;
    case LispFunc::FindEnemy: {
        //      return current_lev shit
    }
    break;
    case LispFunc::UserFun: {
        void *f = figures[current_object->otype]->get_fun(OFUN_USER_FUN);
        if (!f)
            return NULL;
        return ((LSymbol *)f)->EvalFunction(args);
    }
    break;
    case LispFunc::Time: {
        long trials = lnumber_value(CAR(args)->Eval());
        args = CDR(args);
        time_marker start;
        for (int x = 0; x < trials; x++)
        {
            LSpace::Tmp.Clear();
            CAR(args)->Eval();
        }
        time_marker end;
        return LFixedPoint::Create((long)(end.diff_time(&start) * (1 << 16)));
    }
    break;
    case LispFunc::Name: {
        return LString::Create(object_names[current_object->otype]);
    }
    break;
    case LispFunc::FloatTick: {
        return current_object->float_tick();
    }
    break;
    case LispFunc::FindObjectInArea: {
        long x1 = lnumber_value(CAR(args)->Eval());
        args = CDR(args);
        long y1 = lnumber_value(CAR(args)->Eval());
        args = CDR(args);
        long x2 = lnumber_value(CAR(args)->Eval());
        args = CDR(args);
        long y2 = lnumber_value(CAR(args)->Eval());
        args = CDR(args);

        void *list = CAR(args)->Eval();
        game_object *find = current_level->find_object_in_area(current_object->x, current_object->y, x1, y1, x2, y2,
                                                               list, current_object);
        if (find)
            return LPointer::Create(find);
        else
            return NULL;
    }
    break;

    case LispFunc::FindObjectInAngle: {
        long a1 = lnumber_value(CAR(args)->Eval());
        args = CDR(args);
        long a2 = lnumber_value(CAR(args)->Eval());
        args = CDR(args);

        void *list = CAR(args)->Eval();
        PtrRef r1(list);
        game_object *find =
            current_level->find_object_in_angle(current_object->x, current_object->y, a1, a2, list, current_object);
        if (find)
            return LPointer::Create(find);
        else
            return NULL;
    }
    break;
    case LispFunc::DefChar: // def_character
    {
        LSymbol *sym = (LSymbol *)lcar(args);
        if (item_type(sym) != L_SYMBOL)
        {
            lbreak("expecting first arg to def-character to be a symbol!\n");
            exit(EXIT_SUCCESS);
        }
        LSpace *sp = LSpace::Current;
        LSpace::Current = &LSpace::Perm;
        sym->SetNumber(total_objects); // set the symbol value to the object number
        LSpace::Current = sp;
        if (!total_objects)
        {
            object_names = (char **)malloc(sizeof(char *) * (total_objects + 1));
            figures = (CharacterType **)malloc(sizeof(CharacterType *) * (total_objects + 1));
        }
        else
        {
            object_names = (char **)realloc(object_names, sizeof(char *) * (total_objects + 1));
            figures = (CharacterType **)realloc(figures, sizeof(CharacterType *) * (total_objects + 1));
        }

        object_names[total_objects] = strdup(lstring_value(sym->GetName()));
        figures[total_objects] = new CharacterType((LList *)CDR(args), sym);
        total_objects++;
        return LNumber::Create(total_objects - 1);
    }
    break;
    case LispFunc::SeeDist: {
        int32_t x1 = lnumber_value(CAR(args)->Eval());
        args = CDR(args);
        int32_t y1 = lnumber_value(CAR(args)->Eval());
        args = CDR(args);
        int32_t x2 = lnumber_value(CAR(args)->Eval());
        args = CDR(args);
        int32_t y2 = lnumber_value(CAR(args)->Eval());
        current_level->foreground_intersect(x1, y1, x2, y2);
        void *ret = NULL;
        push_onto_list(LNumber::Create(y2), ret);
        push_onto_list(LNumber::Create(x2), ret);
        return ret;
    }
    break;
    case LispFunc::Platform: {
#ifdef __linux__
        return LSymbol::FindOrCreate("LINUX");
#endif
#ifdef __sgi
        return LSymbol::FindOrCreate("IRIX");
#endif
#ifdef __WIN32
        return LSymbol::FindOrCreate("WIN32");
#endif
    }
    break;
    case LispFunc::LevelName: {
        return LString::Create(current_level->name());
    }
    break;
    case LispFunc::AntAi:
        return ant_ai();
        break;
    case LispFunc::SensorAi:
        return sensor_ai();
        break;
    case LispFunc::DevDraw:
        if (dev & EDIT_MODE)
            current_object->drawer();
        break;
    case LispFunc::TopAi:
        decrement_fire_delay();
        return true_symbol;
        break;
    case LispFunc::LaserUFun:
        return laser_ufun(args);
        break;
    case LispFunc::TopUFun:
        return top_ufun(args);
        break;
    case LispFunc::PLaserUFun:
        return plaser_ufun(args);
        break;
    case LispFunc::PlayerRocketUFun:
        return player_rocket_ufun(args);
        break;
    case LispFunc::LSaberUFun:
        return lsaber_ufun(args);
        break;
    case LispFunc::CopMover: {

        int32_t xm, ym, but;
        xm = lnumber_value(CAR(args));
        args = CDR(args);
        ym = lnumber_value(CAR(args));
        args = CDR(args);
        but = lnumber_value(CAR(args));
        return cop_mover(xm, ym, but);
    }
    break;
    case LispFunc::LatterAi:
        return ladder_ai();
        break;
    case LispFunc::WithObj0: {
        game_object *old_cur = current_object;
        current_object = current_object->get_object(0);
        void *ret = eval_block(args);
        current_object = old_cur;
        return ret;
    }
    break;
    case LispFunc::Activated: {
        if (current_object->total_objects() == 0)
            return true_symbol;
        else if (current_object->get_object(0)->aistate())
            return true_symbol;
        else
            return NULL;
    }
    break;
    case LispFunc::TopDraw:
        top_draw();
        break;
    case LispFunc::BottomDraw:
        bottom_draw();
        break;
    case LispFunc::MoverAi:
        return mover_ai();
        break;
    case LispFunc::SGunAi:
        return sgun_ai();
    case LispFunc::LastSavegameName: {
        char nm[50];
        last_savegame_name(nm);
        return LString::Create(nm);
    }
    break;
    case LispFunc::NextSavegameName: {
        char nm[50];
        sprintf(nm, "save%04d.pcx", load_game(1, symbol_str("LOAD")));
        //      get_savegame_name(nm);
        the_game->reset_keymap();
        return LString::Create(nm);
    }
    break;
    case LispFunc::Argv: {
        return LString::Create(start_argv[lnumber_value(CAR(args)->Eval())]);
    }
    break;
    case LispFunc::JoyStat: {
        int xv, yv, b1, b2, b3;
        if (has_joystick)
            joy_status(b1, b2, b3, xv, yv);
        else
            b1 = b2 = b3 = xv = yv = 0;

        void *ret = NULL;
        PtrRef r1(ret);
        push_onto_list(LNumber::Create(b3), ret);
        push_onto_list(LNumber::Create(b2), ret);
        push_onto_list(LNumber::Create(b1), ret);
        push_onto_list(LNumber::Create(yv), ret);
        push_onto_list(LNumber::Create(xv), ret);
        return ret;
    }
    break;
    case LispFunc::MouseStat: {
        void *ret = NULL;
        {
            PtrRef r1(ret);
            push_onto_list(LNumber::Create((last_demo_mbut & 4) == 4), ret);
            push_onto_list(LNumber::Create((last_demo_mbut & 2) == 2), ret);
            push_onto_list(LNumber::Create((last_demo_mbut & 1) == 1), ret);
            push_onto_list(LNumber::Create(last_demo_mpos.y), ret);
            push_onto_list(LNumber::Create(last_demo_mpos.x), ret);
        }
        return ret;
    }
    break;
    case LispFunc::MouseToGame: {
        int x = lnumber_value(CAR(args)->Eval());
        args = CDR(args);
        int y = lnumber_value(CAR(args)->Eval());
        args = CDR(args);

        ivec2 pos = the_game->MouseToGame(ivec2(x, y));
        void *ret = NULL;
        {
            PtrRef r1(ret);
            push_onto_list(LNumber::Create(pos.y), ret);
            push_onto_list(LNumber::Create(pos.x), ret);
        }
        return ret;
    }
    break;
    case LispFunc::GameToMouse: {
        int x = lnumber_value(CAR(args)->Eval());
        args = CDR(args);
        int y = lnumber_value(CAR(args)->Eval());
        args = CDR(args);

        ivec2 pos = the_game->GameToMouse(ivec2(x, y), current_view);
        void *ret = NULL;
        {
            PtrRef r1(ret);
            push_onto_list(LNumber::Create(pos.y), ret);
            push_onto_list(LNumber::Create(pos.x), ret);
        }
        return ret;
    }
    break;
    case LispFunc::GetMainFont:
        return LPointer::Create(wm->font());
        break;
    case LispFunc::PlayerName: {
        view *c = current_object->controller();
        if (!c)
            lbreak("object is not a player, cannot return name");
        else
            return LString::Create(c->name);
    }
    break;
    case LispFunc::GetCwd: {
#if defined __CELLOS_LV2__
        /* FIXME: retrieve the PS3 account name */
        char const *cd = "Player";
#else
        char cd[150];
        getcwd(cd, 100);
#endif
        return LString::Create(cd);
    }
    break;
    case LispFunc::System:
#if !defined __CELLOS_LV2__
        /* FIXME: this looks rather dangerous */
        system(lstring_value(CAR(args)->Eval()));
#endif
        break;
    case LispFunc::ConvertSlashes: {
        void *fn = CAR(args)->Eval();
        args = CDR(args);
        char tmp[200];
        {
            PtrRef r1(fn);
            char *slash = lstring_value(CAR(args)->Eval());
            char *filename = lstring_value(fn);

            char *s = filename, *tp;

            for (tp = tmp; *s; s++, tp++)
            {
                if (*s == '/' || *s == '\\')
                    *tp = *slash;
                else
                    *tp = *s;
            }
            *tp = 0;
        }
        return LString::Create(tmp);
    }
    break;
    case LispFunc::GetDirectory: {
        char **files, **dirs;
        int tfiles, tdirs, i;

        get_directory(lstring_value(CAR(args)->Eval()), files, tfiles, dirs, tdirs);
        void *fl = NULL, *dl = NULL, *rl = NULL;
        {
            PtrRef r1(fl), r2(dl);

            for (i = tfiles - 1; i >= 0; i--)
            {
                push_onto_list(LString::Create(files[i]), fl);
                free(files[i]);
            }
            free(files);

            for (i = tdirs - 1; i >= 0; i--)
            {
                push_onto_list(LString::Create(dirs[i]), dl);
                free(dirs[i]);
            }
            free(dirs);

            push_onto_list(dl, rl);
            push_onto_list(fl, rl);
        }

        return rl;
    }
    break;
    case LispFunc::RespawnAi:
        return respawn_ai();
        break;
    case LispFunc::ScoreDraw:
        return score_draw();
        break;
    case LispFunc::ShowKills:
        return show_kills();
        break;
    case LispFunc::MkPtr: {
        long x;
        sscanf(lstring_value(CAR(args)->Eval()), "%lx", &x);
        return LPointer::Create((void *)(intptr_t)x);
    }
    break;
    case LispFunc::Seq: {
        char name[256], name2[256];
        strcpy(name, lstring_value(CAR(args)->Eval()));
        args = CDR(args);
        long first = lnumber_value(CAR(args)->Eval());
        args = CDR(args);
        long last = lnumber_value(CAR(args)->Eval());
        long i;
        void *ret = NULL;
        PtrRef r1(ret);

        if (last >= first)
        {
            for (i = last; i >= first; i--)
            {
                sprintf(name2, "%s%04ld.pcx", name, i);
                push_onto_list(LString::Create(name2), ret);
            }
        }
        else
        {
            for (i = last; i <= first; i++)
            {
                sprintf(name2, "%s%04ld.pcx", name, i);
                push_onto_list(LString::Create(name2), ret);
            }
        }
        return ret;
    }
    }
    return NULL;
}

//extern bFILE *rcheck,*rcheck_lp;

// arguments have already been evaled..
long c_caller(CFunc number, void *args)
{
    PtrRef r1(args);
    switch (number)
    {
    case CFunc::DistX: {
        return abs(current_object->x - current_level->attacker(current_object)->x);
    }
    break;
    case CFunc::DistY: {
        return abs(current_object->y - current_level->attacker(current_object)->y);
    }
    break;
    case CFunc::KeyPressed: {
        if (!current_object->controller())
        {
            lbreak("object is not a player, cannot determine keypresses");
        }
        else
        {
            return current_object->controller()->key_down(lnumber_value(CAR(args)));
        }
    }
    break;
    case CFunc::LocalKeyPressed: {
        return the_game->key_down(lnumber_value(CAR(args)));
    }
    break;
    case CFunc::BgState: {
        return current_level->attacker(current_object)->state;
    }
    break;
    case CFunc::AiType: {
        return current_object->aitype();
    }
    break;
    case CFunc::AiState: {
        if (!current_object->keep_ai_info())
            current_object->set_aistate(0);
        return current_object->aistate();
    }
    break;
    case CFunc::SetAiState: {
        int ns = lnumber_value(CAR(args));
        current_object->set_aistate_time(0);
        current_object->set_aistate(ns);
        return 1;
    }
    break;
    case CFunc::Random: {
        /*      if (rcheck_lp)
      {
    char str[100];
    sprintf(str,"\n\nTick %d, Rand_on %d\n",current_level->tick_counter(),rand_on);
    rcheck_lp->write(str,strlen(str)+1);
    current_print_file=rcheck_lp;
    print_trace_stack(6);
    current_print_file=NULL;
      }*/

        return jrandom(lnumber_value(CAR(args)));
    }
    break;
    case CFunc::StateTime:
        return current_object->aistate_time();
        break;
    case CFunc::State:
        return current_object->state;
        break;
    case CFunc::Toward: {
        if (current_level->attacker(current_object)->x > current_object->x)
            return 1;
        else
            return -1;
    }
    break;
    case CFunc::Move: {
        return current_object->move(lnumber_value(CAR(args)), lnumber_value(CAR(CDR(args))),
                                    lnumber_value(CAR(CDR(CDR(args)))));
    }
    break;
    case CFunc::Facing: {
        if (current_object->direction > 0)
            return 1;
        else
            return -1;
    }
    break;
    case CFunc::OType:
        return current_object->otype;
        break;
    case CFunc::NextPicture:
        return current_object->next_picture();
        break;
    case CFunc::SetFadeDir:
        current_object->set_fade_dir(lnumber_value(CAR(args)));
        return 1;
        break;
    case CFunc::Mover: {
        int cx = lnumber_value(CAR(args));
        args = CDR(args);
        int cy = lnumber_value(CAR(args));
        args = CDR(args);
        int but = lnumber_value(CAR(args));
        return current_object->mover(cx, cy, but);
    }
    break;
    case CFunc::SetFadeCount:
        current_object->set_fade_count(lnumber_value(CAR(args)));
        return 1;
        break;
    case CFunc::FadeCount:
        return current_object->fade_count();
        break;
    case CFunc::FadeDir:
        return current_object->fade_dir();
        break;
    case CFunc::TouchingBg: {
        int32_t x1, y1, x2, y2, xp1, yp1, xp2, yp2;
        current_level->attacker(current_object)->picture_space(x1, y1, x2, y2);
        current_object->picture_space(xp1, yp1, xp2, yp2);
        if (xp1 > x2 || xp2 < x1 || yp1 > y2 || yp2 < y1)
            return 0;
        else
        {
            //AR enable quick save if player is touching the save console
            if (current_level->attacker(current_object)->otype == TYPE_PLAYER_BOTTOM &&
                current_object->otype == TYPE_SAVE_CONSOLE)
                settings.player_touching_console = true;
            return 1;
        }
    }
    break;
    case CFunc::AddPower:
        current_object->add_power(lnumber_value(CAR(args)));
        break;
    case CFunc::AddHp:
        current_object->add_hp(lnumber_value(CAR(args)));
        break;

    case CFunc::Draw: {
        current_object->drawer();
        return 1;
    }
    break;
    case CFunc::EditMode: {
        return (dev & EDIT_MODE);
    }
    break;
    case CFunc::DrawAbove: {
        current_object->draw_above(current_view);
        return 1;
    }
    break;
    case CFunc::X:
        return current_object->x;
        break;
    case CFunc::Y:
        return current_object->y;
        break;
    case CFunc::SetX: {
        int32_t v = lnumber_value(CAR(args));
        current_object->x = v;
        return 1;
    }
    break;
    case CFunc::SetY: {
        int32_t v = lnumber_value(CAR(args));
        current_object->y = v;
        return 1;
    }
    break;
    case CFunc::SetLastX: {
        int32_t v = lnumber_value(CAR(args));
        current_object->last_x = v;
        return 1;
    }
    break;
    case CFunc::SetLastY: {
        int32_t v = lnumber_value(CAR(args));
        current_object->last_y = v;
        return 1;
    }
    break;

    case CFunc::PushCharacters: {
        return current_level->push_characters(current_object, lnumber_value(CAR(args)), lnumber_value(CAR(CDR(args))));
    }
    break;

    case CFunc::SetState: {
        int32_t s = lnumber_value(CAR(args));
        current_object->set_state((character_state)s);
        return (s == current_object->state);
    }
    break;

    case CFunc::BgX:
        return current_level->attacker(current_object)->x;
        break;
    case CFunc::BgY:
        return current_level->attacker(current_object)->y;
        break;
    case CFunc::SetAiType:
        current_object->change_aitype(lnumber_value(CAR(args)));
        return 1;
        break;

    case CFunc::XVel:
        return current_object->xvel();
        break;
    case CFunc::YVel:
        return current_object->yvel();
        break;
    case CFunc::SetXVel:
        current_object->set_xvel(lnumber_value(CAR(args)));
        return 1;
        break;
    case CFunc::SetYVel:
        current_object->set_yvel(lnumber_value(CAR(args)));
        return 1;
        break;
    case CFunc::Away: {
        if (current_level->attacker(current_object)->x > current_object->x)
            return -1;
        else
            return 1;
        break;
    }
    case CFunc::BlockedLeft:
        return lnumber_value(CAR(args)) & BLOCKED_LEFT;
        break;
    case CFunc::BlockedRight:
        return lnumber_value(CAR(args)) & BLOCKED_RIGHT;
        break;

    case CFunc::AddPalette:
        dev_cont->add_palette(args);
        break;
    case CFunc::ScreenShot:
        write_PCX(main_screen, pal, lstring_value(CAR(args)));
        break;

    case CFunc::SetZoom:
        the_game->zoom = lnumber_value(CAR(args));
        the_game->draw();
        break;
    case CFunc::ShowHelp:
        the_game->show_help(lstring_value(CAR(args)));
        break;

    case CFunc::Direction:
        return current_object->direction;
        break;
    case CFunc::SetDirection:
        current_object->direction = lnumber_value(CAR(args));
        break;
    case CFunc::FreezePlayer: {
        int x1 = lnumber_value(CAR(args));
        if (!current_object->controller())
        {
            lbreak("set_freeze_time : object is not a focus\n");
        }
        else
            current_object->controller()->freeze_time = x1;
        return 1;
    }
    break;
    case CFunc::DoCommand: {
        Event ev;
        dev_cont->do_command(lstring_value(CAR(args)), ev);
        return 1;
    }
    break;
    case CFunc::SetGameState:
        the_game->set_state(lnumber_value(CAR(args)));
        break;

    case CFunc::SceneSetTextRegion: {
        int x1 = lnumber_value(CAR(args));
        args = CDR(args);
        int y1 = lnumber_value(CAR(args));
        args = CDR(args);
        int x2 = lnumber_value(CAR(args));
        args = CDR(args);
        int y2 = lnumber_value(CAR(args));
        scene_director.set_text_region(x1, y1, x2, y2);
    }
    break;
    case CFunc::SceneSetFrameSpeed:
        scene_director.set_frame_speed(lnumber_value(CAR(args)));
        break;
    case CFunc::SceneSetScrollSpeed:
        scene_director.set_scroll_speed(lnumber_value(CAR(args)));
        break;
    case CFunc::SceneSetPanSpeed:
        scene_director.set_pan_speed(lnumber_value(CAR(args)));
        break;
    case CFunc::SceneScrollText:
        scene_director.scroll_text(lstring_value(CAR(args)));
        break;
    case CFunc::ScenePan:
        scene_director.set_pan(lnumber_value(CAR(args)), lnumber_value(CAR(CDR(args))),
                               lnumber_value(CAR(CDR(CDR(args)))));
        break;
    case CFunc::SceneWait:
        scene_director.wait(CAR(args));
        break;

    case CFunc::LevelNew:
        the_game->set_level(
            new level(lnumber_value(CAR(args)), lnumber_value(CAR(CDR(args))), lstring_value(CAR(CDR(CDR(args))))));
        break;
    case CFunc::DoDamage: {
        int amount = lnumber_value(CAR(args));
        args = CDR(args);
        game_object *o = ((game_object *)lpointer_value(CAR(args)));
        args = CDR(args);
        int xv = 0, yv = 0;
        if (args)
        {
            xv = lnumber_value(CAR(args));
            args = CDR(args);
            yv = lnumber_value(CAR(args));
        }
        o->do_damage(amount, current_object, current_object->x, current_object->y, xv, yv);
    }
    break;
    case CFunc::Hp:
        return current_object->hp();
        break;
    case CFunc::SetShiftDown: {
        game_object *o = (game_object *)lpointer_value(CAR(args));
        if (!o->controller())
            printf("set shift: object is not a focus\n");
        else
            o->controller()->m_shift.y = lnumber_value(CAR(CDR(args)));
        return 1;
    }
    break;
    case CFunc::SetShiftRight: {
        game_object *o = (game_object *)lpointer_value(CAR(args));
        if (!o->controller())
            printf("set shift: object is not a focus\n");
        else
            o->controller()->m_shift.x = lnumber_value(CAR(CDR(args)));
        return 1;
    }
    break;
    case CFunc::SetGravity:
        current_object->set_gravity(lnumber_value(CAR(args)));
        return 1;
        break;
    case CFunc::Tick:
        return current_object->tick();
        break;
    case CFunc::SetXacel:
        current_object->set_xacel((lnumber_value(CAR(args))));
        return 1;
        break;
    case CFunc::SetYacel:
        current_object->set_yacel((lnumber_value(CAR(args))));
        return 1;
        break;
    case CFunc::SetLocalPlayers:
        set_local_players(lnumber_value(CAR(args)));
        return 1;
        break;
    case CFunc::LocalPlayers:
        return total_local_players();
        break;
    case CFunc::SetLightDetail:
        light_detail = lnumber_value(CAR(args));
        return 1;
        break;
    case CFunc::LightDetail:
        return light_detail;
        break;
    case CFunc::SetMorphDetail:
        morph_detail = lnumber_value(CAR(args));
        return 1;
        break;
    case CFunc::MorphDetail:
        return morph_detail;
        break;
    case CFunc::MorphInto:
        current_object->morph_into(lnumber_value(CAR(args)), NULL, lnumber_value(CAR(CDR(args))),
                                   lnumber_value(CAR(CDR(CDR(args)))));
        return 1;
        break;
    case CFunc::LinkObject:
        current_object->add_object((game_object *)lpointer_value(CAR(args)));
        return 1;
        break;
    case CFunc::DrawLine: {
        int32_t x1 = lnumber_value(CAR(args));
        args = lcdr(args);
        int32_t y1 = lnumber_value(CAR(args));
        args = lcdr(args);
        int32_t x2 = lnumber_value(CAR(args));
        args = lcdr(args);
        int32_t y2 = lnumber_value(CAR(args));
        args = lcdr(args);
        int32_t color = lnumber_value(CAR(args));
        ivec2 pos1 = the_game->GameToMouse(ivec2(x1, y1), current_view);
        ivec2 pos2 = the_game->GameToMouse(ivec2(x2, y2), current_view);
        main_screen->Line(pos1, pos2, color);
        return 1;
    }
    break;
    case CFunc::DrawLaser: {
        int32_t origin_x = lnumber_value(CAR(args));
        args = lcdr(args);
        int32_t origin_y = lnumber_value(CAR(args));
        args = lcdr(args);
        int32_t angle_deg = lnumber_value(CAR(args));
        args = lcdr(args);
        int32_t medium_color = lnumber_value(CAR(args));
        args = lcdr(args);
        int32_t bright_color = lnumber_value(CAR(args));

        int32_t x1 = current_object->x;
        int32_t y1 = current_object->y;

        double angle_rad = angle_deg * (M_PI / 180.0);

        int32_t dx = x1 - origin_x;
        int32_t dy = y1 - origin_y;
        double dist = sqrt(dx * dx + dy * dy);

        if (dist > 20.0)
        {
            dist = 20.0;
        }

        int32_t x2 = x1 - std::lround(dist * cos(angle_rad));
        int32_t y2 = y1 + std::lround(dist * sin(angle_rad));

        ivec2 pos1 = the_game->GameToMouse(ivec2(x1, y1 - 1), current_view);
        ivec2 pos2 = the_game->GameToMouse(ivec2(x2, y2 - 1), current_view);
        main_screen->Line(pos1, pos2, medium_color);

        pos1 = the_game->GameToMouse(ivec2(x1, y1 + 1), current_view);
        pos2 = the_game->GameToMouse(ivec2(x2, y2 + 1), current_view);
        main_screen->Line(pos1, pos2, medium_color);

        pos1 = the_game->GameToMouse(ivec2(x1 - 1, y1), current_view);
        pos2 = the_game->GameToMouse(ivec2(x2 - 1, y2), current_view);
        main_screen->Line(pos1, pos2, medium_color);

        pos1 = the_game->GameToMouse(ivec2(x1 + 1, y1), current_view);
        pos2 = the_game->GameToMouse(ivec2(x2 + 1, y2), current_view);
        main_screen->Line(pos1, pos2, medium_color);

        pos1 = the_game->GameToMouse(ivec2(x1, y1), current_view);
        pos2 = the_game->GameToMouse(ivec2(x2, y2), current_view);
        main_screen->Line(pos1, pos2, bright_color);
        return 1;
    }
    case CFunc::DarkColor:
        return wm->dark_color();
        break;
    case CFunc::MediumColor:
        return wm->medium_color();
        break;
    case CFunc::BrightColor:
        return wm->bright_color();
        break;

    case CFunc::RemoveObject:
        current_object->remove_object((game_object *)lpointer_value(CAR(args)));
        return 1;
        break;
    case CFunc::LinkLight:
        current_object->add_light((light_source *)lpointer_value(CAR(args)));
        return 1;
        break;
    case CFunc::RemoveLight:
        current_object->remove_light((light_source *)lpointer_value(CAR(args)));
        return 1;
        break;
    case CFunc::TotalObjects:
        return current_object->total_objects();
        break;
    case CFunc::TotalLights:
        return current_object->total_lights();
        break;

    case CFunc::SetLightR1: {
        light_source *l = (light_source *)lpointer_value(CAR(args));
        int32_t x = lnumber_value(CAR(CDR(args)));
        if (x >= 1)
            l->inner_radius = x;
        l->calc_range();
        return 1;
    }
    break;
    case CFunc::SetLightR2: {
        light_source *l = (light_source *)lpointer_value(CAR(args));
        int32_t x = lnumber_value(CAR(CDR(args)));
        if (x > l->inner_radius)
            l->outer_radius = x;
        l->calc_range();
        return 1;
    }
    break;
    case CFunc::SetLightX: {
        light_source *l = (light_source *)lpointer_value(CAR(args));
        l->x = lnumber_value(CAR(CDR(args)));
        l->calc_range();
        return 1;
    }
    break;
    case CFunc::SetLightY: {
        light_source *l = (light_source *)lpointer_value(CAR(args));
        l->y = lnumber_value(CAR(CDR(args)));
        l->calc_range();
        return 1;
    }
    break;
    case CFunc::SetLightXShift: {
        light_source *l = (light_source *)lpointer_value(CAR(args));
        l->xshift = lnumber_value(CAR(CDR(args)));
        l->calc_range();
        return 1;
    }
    break;
    case CFunc::SetLightYShift: {
        light_source *l = (light_source *)lpointer_value(CAR(args));
        l->yshift = lnumber_value(CAR(CDR(args)));
        l->calc_range();
        return 1;
    }
    break;
    case CFunc::LightR1:
        return ((light_source *)lpointer_value(CAR(args)))->inner_radius;
        break;
    case CFunc::LightR2:
        return ((light_source *)lpointer_value(CAR(args)))->outer_radius;
        break;
    case CFunc::LightX:
        return ((light_source *)lpointer_value(CAR(args)))->x;
        break;
    case CFunc::LightY:
        return ((light_source *)lpointer_value(CAR(args)))->y;
        break;
    case CFunc::LightXShift:
        return ((light_source *)lpointer_value(CAR(args)))->xshift;
        break;
    case CFunc::LightYShift:
        return ((light_source *)lpointer_value(CAR(args)))->yshift;
        break;
    case CFunc::Xacel:
        return current_object->xacel();
        break;
    case CFunc::Yacel:
        return current_object->yacel();
        break;
    case CFunc::DeleteLight:
        current_level->remove_light((light_source *)lpointer_value(CAR(args)));
        break;
    case CFunc::SetFx:
        current_object->set_fx(lnumber_value(CAR(args)));
        break;
    case CFunc::SetFy:
        current_object->set_fy(lnumber_value(CAR(args)));
        break;
    case CFunc::SetFxvel:
        current_object->set_fxvel(lnumber_value(CAR(args)));
        break;
    case CFunc::SetFyvel:
        current_object->set_fyvel(lnumber_value(CAR(args)));
        break;
    case CFunc::SetFxacel:
        current_object->set_fxacel(lnumber_value(CAR(args)));
        break;
    case CFunc::SetFyacel:
        current_object->set_fyacel(lnumber_value(CAR(args)));
        break;
    case CFunc::PictureWidth:
        return current_object->picture()->Size().x;
        break;
    case CFunc::PictureHeight:
        return current_object->picture()->Size().y;
        break;
    case CFunc::Trap: {
        printf("trap\n");
    }
    break; // I use this to set gdb break points
    case CFunc::PlatformPush: {
        return current_level->platform_push(current_object, lnumber_value(CAR(args)), lnumber_value(CAR(CDR(args))));
    }
    break;
    case CFunc::DefSound: {
        LSymbol *sym = NULL;
        if (CDR(args))
        {
            sym = (LSymbol *)lcar(args);
            if (item_type(sym) != L_SYMBOL)
            {
                lbreak("expecting first arg to def-character to be a symbol!\n");
                exit(EXIT_SUCCESS);
            }
            args = CDR(args);
        }

        LSpace *sp = LSpace::Current;
        LSpace::Current = &LSpace::Perm;
        int id = cache.reg(lstring_value(lcar(args)), NULL, SPEC_EXTERN_SFX, 1);
        if (sym)
            sym->SetNumber(id); // set the symbol value to sfx id
        LSpace::Current = sp;
        return id;
    }
    break;
    case CFunc::PlaySound: {
        void *a = args;
        PtrRef r1(a);
        int id = lnumber_value(lcar(a));
        if (id < 0)
            return 0;
        a = CDR(a);
        if (!a)
            cache.sfx(id)->play(127);
        else
        {
            int vol = lnumber_value(lcar(a));
            a = CDR(a);
            if (a)
            {
                int32_t x = lnumber_value(lcar(a));
                a = CDR(a);
                if (!a)
                {
                    ((LObject *)args)->Print();
                    lbreak("expecting y after x in play_sound\n");
                    exit(EXIT_FAILURE);
                }
                int32_t y = lnumber_value(lcar(a));
                the_game->play_sound(id, vol, x, y);
            }
            else
                cache.sfx(id)->play(vol);
        }
    }
    break;

    case CFunc::DefParticle:
        return defun_pseq(args);
        break;
    case CFunc::AddPanim: {
        int id = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t x = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t y = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t dir = lnumber_value(CAR(args));
        add_panim(id, x, y, dir);
    }
    break;
    case CFunc::WeaponToType: {
        int32_t x = lnumber_value(CAR(args));
        args = CDR(args);
        if (x < 0 || x >= total_weapons)
        {
            lbreak("weapon out of range (%d)\n", x);
            exit(EXIT_SUCCESS);
        }
        return weapon_types[x];
    }
    break;
    case CFunc::HurtRadius: {
        int32_t x = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t y = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t r = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t m = lnumber_value(CAR(args));
        args = CDR(args);
        game_object *o = (game_object *)lpointer_value(CAR(args));
        args = CDR(args);
        int32_t mp = lnumber_value(CAR(args));
        current_level->hurt_radius(x, y, r, m, current_object, o, mp);
    }
    break;

    case CFunc::AddAmmo: {
        view *v = current_object->controller();
        if (!v)
            printf("Can't add weapons for non-players\n");
        else
        {
            int32_t x = lnumber_value(CAR(args));
            args = CDR(args);
            int32_t y = lnumber_value(CAR(args));
            args = CDR(args);
            if (x < 0 || x >= total_weapons)
            {
                lbreak("weapon out of range (%d)\n", x);
                exit(EXIT_SUCCESS);
            }
            v->add_ammo(x, y);
        }
    }
    break;
    case CFunc::AmmoTotal: {
        view *v = current_object->controller();
        if (!v)
            return 0;
        else
            return v->weapon_total(lnumber_value(CAR(args)));
    }
    break;
    case CFunc::CurrentWeapon: {
        view *v = current_object->controller();
        if (!v)
            return 0;
        else
            return v->current_weapon;
    }
    break;
    case CFunc::CurrentWeaponType: {
        view *v = current_object->controller();
        if (!v)
        {
            lbreak("current_weapon_type : object cannot hold weapons\n");
            return 0;
        }
        else
            return v->current_weapon;
    }
    break;
    case CFunc::BlockedUp:
        return lnumber_value(CAR(args)) & BLOCKED_UP;
        break;
    case CFunc::BlockedDown:
        return lnumber_value(CAR(args)) & BLOCKED_DOWN;
        break;
    case CFunc::GiveWeapon: {
        view *v = current_object->controller();
        int x = lnumber_value(CAR(args));
        if (x < 0 || x >= total_weapons)
        {
            lbreak("weapon out of range (%d)\n", x);
            exit(EXIT_SUCCESS);
        }
        if (v)
            v->give_weapon(x);
    }
    break;
    case CFunc::GetAbility: {
        int a = lnumber_value(CAR(args));
        if (a < 0 || a >= TOTAL_ABILITIES)
        {
            ((LObject *)args)->Print();
            lbreak("bad ability number for get_ability, should be 0..%d, not %d\n", TOTAL_ABILITIES, a);
            exit(EXIT_SUCCESS);
        }
        return get_ability(current_object->otype, (ability)a);
    }
    break;
    case CFunc::ResetPlayer: {
        view *v = current_object->controller();
        if (!v)
            printf("Can't use reset_player on non-players\n");
        else
            v->reset_player();
    }
    break;
    case CFunc::SiteAngle: {
        game_object *o = (game_object *)lpointer_value(CAR(args));
        int32_t x = o->x - current_object->x, y = -(o->y - o->picture()->Size().y / 2 -
                                                    (current_object->y - (current_object->picture()->Size().y / 2)));
        return lisp_atan2(y, x);
    }
    break;
    case CFunc::SetCourse: {
        int32_t ang = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t mag = lfixed_point_value(CAR(args));
        int32_t xvel = (lisp_cos(ang) >> 8) * (mag >> 8);
        current_object->set_xvel(xvel >> 16);
        current_object->set_fxvel((xvel & 0xffff) >> 8);
        int32_t yvel = -(lisp_sin(ang) >> 8) * (mag >> 8);
        current_object->set_yvel(yvel >> 16);
        current_object->set_fyvel((yvel & 0xffff) >> 8);
    }
    break;
    case CFunc::SetFrameAngle: {
        int total_frames = current_object->total_frames();
        int fraction;

        int32_t ang1 = lnumber_value(CAR(args));
        args = CDR(args);
        if (ang1 < 0)
            ang1 = (ang1 % 360) + 360;
        else if (ang1 >= 360)
            ang1 = ang1 % 360;
        int32_t ang2 = lnumber_value(CAR(args));
        args = CDR(args);
        if (ang2 < 0)
            ang2 = (ang2 % 360) + 360;
        else if (ang2 >= 360)
            ang2 = ang2 % 360;

        int32_t ang = (lnumber_value(CAR(args)) + 90 / total_frames) % 360;
        if (ang1 > ang2)
        {
            if (ang < ang1 && ang > ang2)
                return 0;
            else if (ang >= ang1)
                fraction = (ang - ang1) * total_frames / (359 - ang1 + ang2 + 1);
            else
                fraction = (359 - ang1 + ang) * total_frames / (359 - ang1 + ang2 + 1);
        }
        else if (ang < ang1 || ang > ang2)
            return 0;
        else
            fraction = (ang - ang1) * total_frames / (ang2 - ang1 + 1);
        if (current_object->direction > 0)
            current_object->current_frame = fraction;
        else
            current_object->current_frame = total_frames - fraction - 1;
        return 1;
    }
    break;
    case CFunc::JumpState: {
        int x = current_object->current_frame;
        current_object->set_state((character_state)lnumber_value(CAR(args)));
        current_object->current_frame = x;
    }
    break;

    case CFunc::Morphing:
        if (current_object->morph_status())
            return 1;
        else
            return 0;
        break;
    case CFunc::DamageFun: {
        int32_t am = lnumber_value(CAR(args));
        args = CDR(args);
        game_object *from = (game_object *)lpointer_value(CAR(args));
        args = CDR(args);
        int32_t hitx = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t hity = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t px = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t py = lnumber_value(CAR(args));
        args = CDR(args);
        current_object->damage_fun(am, from, hitx, hity, px, py);
    }
    break;
    case CFunc::Gravity:
        return current_object->gravity();
        break;
    case CFunc::MakeViewSolid: {
        view *v = current_object->controller();
        if (!v)
            printf("make_view_solid : object has no view\n");
        else
            v->draw_solid = lnumber_value(CAR(args));
    }
    break;
    case CFunc::FindRgb: {
        void *a = args;
        int r = lnumber_value(CAR(a));
        a = CDR(a);
        int g = lnumber_value(CAR(a));
        a = CDR(a);
        int b = lnumber_value(CAR(a));
        if (r < 0 || b < 0 || g < 0 || r > 255 || g > 255 || b > 255)
        {
            ((LObject *)args)->Print();
            lbreak("color out of range (0..255) in color lookup\n");
            exit(EXIT_SUCCESS);
        }
        return color_table->Lookup(r >> 3, g >> 3, b >> 3);
    }
    break;
    case CFunc::PlayerXSuggest: {
        view *v = current_object->controller();
        if (!v)
        {
            ((LObject *)args)->Print();
            printf("get_player_inputs : object has no view!\n");
        }
        else
            return v->x_suggestion;
    }
    break;
    case CFunc::PlayerYSuggest: {
        //AR get use/down key state
        view *v = current_object->controller();
        if (!v)
        {
            ((LObject *)args)->Print();
            printf("get_player_inputs : object has no view!\n");
        }
        else
            return v->y_suggestion;
    }
    break;
    case CFunc::PlayerB1Suggest: {
        view *v = current_object->controller();
        if (!v)
        {
            ((LObject *)args)->Print();
            printf("get_player_inputs : object has no view!\n");
        }
        else
            return v->b1_suggestion;
    }
    break;
    case CFunc::PlayerB2Suggest: {
        view *v = current_object->controller();
        if (!v)
        {
            ((LObject *)args)->Print();
            printf("get_player_inputs : object has no view!\n");
        }
        else
            return v->b2_suggestion;
    }
    break;
    case CFunc::PlayerB3Suggest: {
        view *v = current_object->controller();
        if (!v)
        {
            ((LObject *)args)->Print();
            printf("get_player_inputs : object has no view!\n");
        }
        else
            return v->b3_suggestion;
    }
    break;
    case CFunc::SetBgScroll: {
        bg_xmul = lnumber_value(CAR(args));
        args = CDR(args);
        bg_xdiv = lnumber_value(CAR(args));
        args = CDR(args);
        bg_ymul = lnumber_value(CAR(args));
        args = CDR(args);
        bg_ydiv = lnumber_value(CAR(args));
        if (bg_xdiv == 0)
        {
            bg_xdiv = 1;
            ((LObject *)args)->Print();
            printf("bg_set_scroll : cannot set xdiv to 0\n");
        }
        if (bg_ydiv == 0)
        {
            bg_ydiv = 1;
            ((LObject *)args)->Print();
            printf("bg_set_scroll : cannot set ydiv to 0\n");
        }
    }
    break;
    case CFunc::SetAmbientLight: {
        view *v = lget_view(CAR(args), "set_ambient_light");
        args = CDR(args);
        int32_t x = lnumber_value(CAR(args));
        if (x >= 0 && x < 64)
            v->ambient = x;
    }
    break;
    case CFunc::AmbientLight:
        return lget_view(CAR(args), "ambient_light")->ambient;
        break;
    case CFunc::HasObject: {
        int x = current_object->total_objects();
        game_object *who = (game_object *)lpointer_value(CAR(args));
        for (int i = 0; i < x; i++)
            if (current_object->get_object(i) == who)
                return 1;
        return 0;
    }
    break;
    case CFunc::SetOtype:
        current_object->change_type(lnumber_value(CAR(args)));
        break;
    case CFunc::CurrentFrame:
        return current_object->current_frame;
        break;

    case CFunc::Fx:
        return current_object->fx();
        break;
    case CFunc::Fy:
        return current_object->fy();
        break;
    case CFunc::Fxvel:
        return current_object->fxvel();
        break;
    case CFunc::Fyvel:
        return current_object->fyvel();
        break;
    case CFunc::Fxacel:
        return current_object->fxacel();
        break;
    case CFunc::Fyacel:
        return current_object->fyacel();
        break;
    case CFunc::SetStatBar: {
        //      char *fn=lstring_value(CAR(args)); args=CDR(args);
        //      stat_bar=cache.reg_object(fn,CAR(args),SPEC_IMAGE,1);
    }
    break;
    case CFunc::SetFgTile: {
        int32_t x = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t y = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t type = lnumber_value(CAR(args));
        if (x < 0 || y < 0 || x >= current_level->foreground_width() || y >= current_level->foreground_width())
            lbreak("%d %d is out of range of fg map", x, y);
        else
            current_level->PutFg(ivec2(x, y), type);
    }
    break;
    case CFunc::FgTile: {
        int32_t x = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t y = lnumber_value(CAR(args));
        if (x < 0 || y < 0 || x >= current_level->foreground_width() || y >= current_level->foreground_width())
            lbreak("%d %d is out of range of fg map", x, y);
        else
            return current_level->GetFg(ivec2(x, y));
    }
    break;
    case CFunc::SetBgTile: {
        int32_t x = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t y = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t type = lnumber_value(CAR(args));
        if (x < 0 || y < 0 || x >= current_level->background_width() || y >= current_level->background_width())
            lbreak("%d %d is out of range of fg map", x, y);
        else
            current_level->PutBg(ivec2(x, y), type);
    }
    break;
    case CFunc::BgTile: {
        int32_t x = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t y = lnumber_value(CAR(args));
        if (x < 0 || y < 0 || x >= current_level->background_width() || y >= current_level->background_width())
            lbreak("%d %d is out of range of fg map", x, y);
        else
            return current_level->GetBg(ivec2(x, y));
    }
    break;
    case CFunc::LoadTiles:
        load_tiles(args);
        break;
    case CFunc::LoadPalette: {
        bFILE *fp = open_file(lstring_value(CAR(args)), "rb");
        if (fp->open_failure())
        {
            delete fp;
            lbreak("load_palette : could not open file %s for reading", lstring_value(CAR(args)));
            exit(EXIT_FAILURE);
        }
        else
        {
            spec_directory sd(fp);
            spec_entry *se = sd.find(SPEC_PALETTE);
            if (!se)
                lbreak("File %s has no palette!\n", lstring_value(CAR(args)));
            else
            {
                if (pal)
                    delete pal;
                pal = new palette(se, fp);
            }
            delete fp;
        }
    }
    break;
    case CFunc::LoadColorFilter: {
        bFILE *fp = open_file(lstring_value(CAR(args)), "rb");
        if (fp->open_failure())
        {
            delete fp;
            lbreak("load_color_filter : could not open file %s for reading", lstring_value(CAR(args)));
            exit(EXIT_FAILURE);
        }
        else
        {
            spec_directory sd(fp);
            spec_entry *se = sd.find(SPEC_COLOR_TABLE);
            if (!se)
                lbreak("File %s has no color filter!", lstring_value(CAR(args)));
            else
            {
                delete color_table;
                color_table = new ColorFilter(se, fp);
            }
            delete fp;
        }
    }
    break;
    case CFunc::CreatePlayers: {
        current_start_type = lnumber_value(CAR(args));
        set_local_players(1);
    }
    break;
    case CFunc::TryMove: {
        int32_t xv = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t yv = lnumber_value(CAR(args));
        args = CDR(args);
        int top = 2;
        if (args)
            if (!CAR(args))
                top = 0;

        int32_t oxv = xv, oyv = yv;
        current_object->try_move(current_object->x, current_object->y, xv, yv, 1 | top);
        current_object->x += xv;
        current_object->y += yv;
        return (oxv == xv && oyv == yv);
    }
    break;
    case CFunc::SequenceLength: {
        int32_t x = lnumber_value(CAR(args));
        return figures[current_object->otype]->get_sequence((character_state)x)->length();
    }
    break;
    case CFunc::CanSee: {
        int32_t x1 = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t y1 = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t x2 = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t y2 = lnumber_value(CAR(args));
        args = CDR(args);
        void *block_all = CAR(args);
        int32_t nx2 = x2, ny2 = y2;
        current_level->foreground_intersect(x1, y1, x2, y2);
        if (x2 != nx2 || y2 != ny2)
            return 0;

        if (block_all)
            current_level->all_boundary_setback(current_object, x1, y1, x2, y2);
        else
            current_level->boundary_setback(current_object, x1, y1, x2, y2);
        return (x2 == nx2 && y2 == ny2);
    }
    break;
    case CFunc::LoadBigFont: {
        char *fn = lstring_value(CAR(args));
        args = CDR(args);
        char *name = lstring_value(CAR(args));
        big_font_pict = cache.reg(fn, name, SPEC_IMAGE, 1);
    }
    break;
    case CFunc::LoadSmallFont: {
        char *fn = lstring_value(CAR(args));
        args = CDR(args);
        char *name = lstring_value(CAR(args));
        small_font_pict = cache.reg(fn, name, SPEC_IMAGE, 1);
    }
    break;
    case CFunc::LoadConsoleFont: {
        char *fn = lstring_value(CAR(args));
        args = CDR(args);
        char *name = lstring_value(CAR(args));
        console_font_pict = cache.reg(fn, name, SPEC_IMAGE, 1);
    }
    break;
    case CFunc::SetCurrentFrame: {
        int32_t x = lnumber_value(CAR(args));
        if (x < current_object->total_frames())
            current_object->current_frame = x;
        else
            lbreak("%d out of range for set_current_frame", x);
    }
    break;

    case CFunc::DrawTransparent: {
        current_object->draw_trans(lnumber_value(CAR(args)), lnumber_value(CAR(CDR(args))));
    }
    break;
    case CFunc::DrawTint: {
        current_object->draw_tint(lnumber_value(CAR(args)));
    }
    break;
    case CFunc::DrawPredator: {
        current_object->draw_predator();
    }
    break;
    case CFunc::ShiftDown: {
        return lget_view(CAR(args), "shift_down")->m_shift.y;
    }
    break;
    case CFunc::ShiftRight: {
        return lget_view(CAR(args), "shift_right")->m_shift.x;
    }
    break;
    case CFunc::SetNoScrollRange: {
        view *v = lget_view(CAR(args), "set_no_scroll_range");
        args = CDR(args);
        v->no_xleft = lnumber_value(CAR(args));
        args = CDR(args);
        v->no_ytop = lnumber_value(CAR(args));
        args = CDR(args);
        v->no_xright = lnumber_value(CAR(args));
        args = CDR(args);
        v->no_ybottom = lnumber_value(CAR(args));
    }
    break;
    case CFunc::DefImage: {
        char *fn = lstring_value(CAR(args));
        args = CDR(args);
        char *name = lstring_value(CAR(args));
        args = CDR(args);
        return cache.reg(fn, name, SPEC_IMAGE, 1);
    }
    break;
    case CFunc::PutImage: {
        int32_t x1 = lnumber_value(CAR(args));
        args = lcdr(args);
        int32_t y1 = lnumber_value(CAR(args));
        args = lcdr(args);
        int32_t id = lnumber_value(CAR(args));
        main_screen->PutImage(cache.img(id), ivec2(x1, y1), 1);
    }
    break;
    case CFunc::ViewX1: {
        view *v = current_object->controller();
        if (!v)
            lbreak("object has no view : view_x1");
        else
            return v->m_aa.x;
    }
    break;
    case CFunc::ViewY1: {
        view *v = current_object->controller();
        if (!v)
            lbreak("object has no view : view_x1");
        else
            return v->m_aa.y;
    }
    break;
    case CFunc::ViewX2: {
        view *v = current_object->controller();
        if (!v)
            lbreak("object has no view : view_x1");
        else
            return v->m_bb.x;
    }
    break;
    case CFunc::ViewY2: {
        view *v = current_object->controller();
        if (!v)
            lbreak("object has no view : view_x1");
        else
            return v->m_bb.y;
    }
    break;
    case CFunc::ViewPushDown: {
        view *v = current_object->controller();
        if (!v)
            lbreak("object has no view : view_push_down");
        else
            v->m_lastpos.y -= lnumber_value(CAR(args));
    }
    break;
    case CFunc::LocalPlayer: {
        view *v = current_object->controller();
        if (!v)
            lbreak("object has no view : local_player");
        else
            return v->local_player();
    }
    break;
    case CFunc::SaveGame: {
        char *fn = lstring_value(CAR(args));
        current_level->save(fn, 1);

        //AR
        settings.quick_load = get_save_filename_prefix();
        settings.quick_load += fn;
    }
    break;
    case CFunc::SetHp: {
        current_object->set_hp(lnumber_value(CAR(args)));
    }
    break;
    case CFunc::RequestLevelLoad: {
        char fn[255];
        // If a save filename is requested, prepend the savegame directory.
        if (strncmp(lstring_value(CAR(args)), "save", 4) == 0)
        {
            sprintf(fn, "%s%s", get_save_filename_prefix(), lstring_value(CAR(args)));
        }
        else
        {
            strcpy(fn, lstring_value(CAR(args)));
        }
        the_game->request_level_load(fn);

        settings.quick_load = fn; //AR
    }
    break;
    case CFunc::SetFirstLevel: {
        strcpy(level_file, lstring_value(CAR(args)));
    }
    break;
    case CFunc::DefTint: {
        return cache.reg(lstring_value(CAR(args)), "palette", SPEC_PALETTE, 1);
    }
    break;
    case CFunc::TintPalette: {
        palette *p = pal->copy();
        uint8_t *addr = (uint8_t *)p->addr();
        int r, g, b;
        int ra = lnumber_value(CAR(args));
        args = CDR(args);
        int ga = lnumber_value(CAR(args));
        args = CDR(args);
        int ba = lnumber_value(CAR(args));
        for (int i = 0; i < 256; i++)
        {
            r = (int)*addr + ra;
            if (r > 255)
                r = 255;
            else if (r < 0)
                r = 0;
            *addr = (uint8_t)r;
            addr++;
            g = (int)*addr + ga;
            if (g > 255)
                g = 255;
            else if (g < 0)
                g = 0;
            *addr = (uint8_t)g;
            addr++;
            b = (int)*addr + ba;
            if (b > 255)
                b = 255;
            else if (b < 0)
                b = 0;
            *addr = (uint8_t)b;
            addr++;
        }
        p->load();
        delete p;
    }
    break;
    case CFunc::PlayerNumber: {
        view *v = current_object->controller();
        if (!v)
            lbreak("object has no view : local_player");
        else
            return v->player_number;
    }
    break;
    case CFunc::SetCurrentWeapon:
        // The code removed here automatically switched the weapon when finding
        // a new weapon. This was annoying.
        break;
    case CFunc::HasWeapon: {
        view *v = current_object->controller();
        if (!v)
            lbreak("object has no view : local_player");
        else
            return v->has_weapon(lnumber_value(CAR(args)));
    }
    break;
    case CFunc::AmbientRamp: {
        ambient_ramp += lnumber_value(CAR(args));
    }
    break;

    case CFunc::TotalPlayers: {
        int x = 0;
        view *v = player_list;
        for (; v; v = v->next, x++)
            ;
        return x;
    }
    break;

    case CFunc::ScatterLine: {
        int32_t x1 = lnumber_value(CAR(args));
        args = lcdr(args);
        int32_t y1 = lnumber_value(CAR(args));
        args = lcdr(args);
        int32_t x2 = lnumber_value(CAR(args));
        args = lcdr(args);
        int32_t y2 = lnumber_value(CAR(args));
        args = lcdr(args);
        int32_t c = lnumber_value(CAR(args));
        args = lcdr(args);
        int32_t s = lnumber_value(CAR(args));
        ivec2 pos1 = the_game->GameToMouse(ivec2(x1, y1), current_view);
        ivec2 pos2 = the_game->GameToMouse(ivec2(x2, y2), current_view);
        ScatterLine(pos1, pos2, c, s);
        return 1;
    }
    break;
    case CFunc::GameTick: {
        if (current_level)
            return current_level->tick_counter();
        else
            return 0;
    }
    break;
    case CFunc::IsaPlayer: {
        return current_object->controller() != NULL;
    }
    break;
    case CFunc::ShiftRandTable: {
        rand_on += lnumber_value(CAR(args));
        return 1;
    }
    break;
    case CFunc::TotalFrames: {
        return current_object->total_frames();
    }
    break;
    case CFunc::Raise: {
        current_level->to_front(current_object);
    }
    break;
    case CFunc::Lower: {
        current_level->to_back(current_object);
    }
    break;
    case CFunc::PlayerPointerX: {
        view *v = current_object->controller();
        if (!v)
        {
            ((LObject *)args)->Print();
            printf("get_player_inputs : object has no view!\n");
        }
        else
            return v->pointer_x;
    }
    break;
    case CFunc::PlayerPointerY: {
        view *v = current_object->controller();
        if (!v)
        {
            ((LObject *)args)->Print();
            printf("get_player_inputs : object has no view!\n");
        }
        else
            return v->pointer_y;
    }
    break;
    case CFunc::FramePanic: {
        if (player_list->next || demo_man.current_state() != demo_manager::NORMAL)
            return 0;
        else
            return (frame_panic > 10);
    }
    break;
    case CFunc::AscatterLine: {
        int32_t x1 = lnumber_value(CAR(args));
        args = lcdr(args);
        int32_t y1 = lnumber_value(CAR(args));
        args = lcdr(args);
        int32_t x2 = lnumber_value(CAR(args));
        args = lcdr(args);
        int32_t y2 = lnumber_value(CAR(args));
        args = lcdr(args);
        int32_t c1 = lnumber_value(CAR(args));
        args = lcdr(args);
        int32_t c2 = lnumber_value(CAR(args));
        args = lcdr(args);
        int32_t s = lnumber_value(CAR(args));
        ivec2 pos1 = the_game->GameToMouse(ivec2(x1, y1), current_view);
        ivec2 pos2 = the_game->GameToMouse(ivec2(x2, y2), current_view);
        AScatterLine(pos1, pos2, c1, c2, s);
        return 1;
    }
    break;
    case CFunc::RandOn: {
        return rand_on;
    }
    break;
    case CFunc::SetRandOn: {
        rand_on = lnumber_value(CAR(args));
    }
    break;
    case CFunc::Bar: {
        int32_t cx1 = lnumber_value(CAR(args));
        args = lcdr(args);
        int32_t cy1 = lnumber_value(CAR(args));
        args = lcdr(args);
        int32_t cx2 = lnumber_value(CAR(args));
        args = lcdr(args);
        int32_t cy2 = lnumber_value(CAR(args));
        args = lcdr(args);
        int32_t c1 = lnumber_value(CAR(args));
        args = lcdr(args);
        main_screen->Bar(ivec2(cx1, cy1), ivec2(cx2, cy2), c1);
    }
    break;
    case CFunc::Argc: {
        return start_argc;
    }
    break;
    case CFunc::PlaySong: {
        if ((sound_avail & MUSIC_INITIALIZED))
        {
            char *fn = lstring_value(CAR(args));
            if (current_song)
            {
                if (current_song->playing())
                    current_song->stop();
                delete current_song;
            }
            current_song = new song(fn);
            current_song->play(music_volume);
            printf("Playing %s at volume %d\n", fn, music_volume);
        }
    }
    break;
    case CFunc::StopSong: {
        if (current_song && current_song->playing())
            current_song->stop();
        delete current_song;
        current_song = NULL;
    }
    break;
    case CFunc::Targetable:
        return current_object->targetable();
        break;
    case CFunc::SetTargetable:
        current_object->set_targetable(CAR(args) == NULL ? 0 : 1);
        break;
    case CFunc::ShowStats:
        show_stats();
        break;
    case CFunc::Kills: {
        view *v = current_object->controller();
        if (!v)
        {
            ((LObject *)args)->Print();
            printf("get_player_inputs : object has no view!\n");
        }
        else
            return v->kills;
    }
    break;
    case CFunc::TKills: {
        view *v = current_object->controller();
        if (!v)
        {
            ((LObject *)args)->Print();
            printf("get_player_inputs : object has no view!\n");
        }
        else
            return v->tkills;
    }
    break;
    case CFunc::Secrets: {
        view *v = current_object->controller();
        if (!v)
        {
            ((LObject *)args)->Print();
            printf("get_player_inputs : object has no view!\n");
        }
        else
            return v->secrets;
    }
    break;
    case CFunc::TSecrets: {
        view *v = current_object->controller();
        if (!v)
        {
            ((LObject *)args)->Print();
            printf("get_player_inputs : object has no view!\n");
        }
        else
            return v->tsecrets;
    }
    break;
    case CFunc::SetKills: {
        view *v = current_object->controller();
        if (!v)
        {
            ((LObject *)args)->Print();
            printf("get_player_inputs : object has no view!\n");
        }
        else
            v->kills = lnumber_value(CAR(args));
    }
    break;
    case CFunc::SetTKills: {
        view *v = current_object->controller();
        if (!v)
        {
            ((LObject *)args)->Print();
            printf("get_player_inputs : object has no view!\n");
        }
        else
            v->tkills = lnumber_value(CAR(args));
    }
    break;
    case CFunc::SetSecrets: {
        view *v = current_object->controller();
        if (!v)
        {
            ((LObject *)args)->Print();
            printf("get_player_inputs : object has no view!\n");
        }
        else
            v->secrets = lnumber_value(CAR(args));
    }
    break;
    case CFunc::SetTSecrets: {
        view *v = current_object->controller();
        if (!v)
        {
            ((LObject *)args)->Print();
            printf("get_player_inputs : object has no view!\n");
        }
        else
            v->tsecrets = lnumber_value(CAR(args));
    }
    break;
    case CFunc::RequestEndGame: {
        the_game->request_end();
    }
    break;
    case CFunc::GetSaveSlot: {
        the_game->reset_keymap();
        return load_game(1, symbol_str("SAVE"));
    }
    break;
    case CFunc::MemReport: {
        printf("mem_report is deprecated\n");
    }
    break;
    case CFunc::MajorVersion: {
        return ENGINE_MAJOR;
    }
    break;
    case CFunc::MinorVersion: {
        return ENGINE_MINOR;
    }
    break;
    case CFunc::DrawDoubleTint: {
        current_object->draw_double_tint(lnumber_value(CAR(args)), lnumber_value(CAR(CDR(args))));
    }
    break;
    case CFunc::ImageWidth: {
        return cache.img(lnumber_value(CAR(args)))->Size().x;
    }
    break;
    case CFunc::ImageHeight: {
        return cache.img(lnumber_value(CAR(args)))->Size().y;
    }
    break;
    case CFunc::ForegroundWidth: {
        return current_level->foreground_width();
    }
    break;
    case CFunc::ForegroundHeight: {
        return current_level->foreground_height();
    }
    break;
    case CFunc::BackgroundWidth: {
        return current_level->background_width();
    }
    break;
    case CFunc::BackgroundHeight: {
        return current_level->background_height();
    }
    break;
    case CFunc::GetKeyCode: {
        return get_keycode(lstring_value(CAR(args)));
    }
    case CFunc::SetCursorShape: {
        int id = lnumber_value(CAR(args));
        args = CDR(args);
        int x = lnumber_value(CAR(args));
        args = CDR(args);
        int y = lnumber_value(CAR(args));
        c_target = id;
        if (main_screen)
            wm->SetMouseShape(cache.img(c_target)->copy(), ivec2(x, y));
    }
    break;
    case CFunc::StartServer: {
        if (!main_net_cfg)
            return 0;
        return become_server(game_name);
    }
    break;
    case CFunc::PutString: {
        JCFont *fnt = (JCFont *)lpointer_value(CAR(args));
        args = CDR(args);
        int32_t x = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t y = lnumber_value(CAR(args));
        args = CDR(args);
        char *st = lstring_value(CAR(args));
        args = CDR(args);
        int color = -1;
        if (args)
            color = lnumber_value(CAR(args));
        fnt->PutString(main_screen, ivec2(x, y), st, color);
    }
    break;
    case CFunc::FontWidth:
        return ((JCFont *)lpointer_value(CAR(args)))->Size().x;
        break;
    case CFunc::FontHeight:
        return ((JCFont *)lpointer_value(CAR(args)))->Size().y;
        break;
    case CFunc::ChatPrint:
        if (chat)
            chat->put_all(lstring_value(CAR(args)));
        break;
    case CFunc::SetPlayerName: {
        view *v = current_object->controller();
        if (!v)
        {
            lbreak("get_player_name : object has no view!\n");
        }
        else
            strcpy(v->name, lstring_value(CAR(args)));
    }
    break;
    case CFunc::DrawBar: {
        int32_t x1 = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t y1 = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t x2 = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t y2 = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t c = lnumber_value(CAR(args));
        main_screen->Bar(ivec2(x1, y1), ivec2(x2, y2), c);
    }
    break;
    case CFunc::DrawRect: {
        int32_t x1 = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t y1 = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t x2 = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t y2 = lnumber_value(CAR(args));
        args = CDR(args);
        int32_t c = lnumber_value(CAR(args));
        main_screen->Rectangle(ivec2(x1, y1), ivec2(x2, y2), c);
    }
    break;
    case CFunc::GetOption: {
        if (get_option(lstring_value(CAR(args))))
            return 1;
        else
            return 0;
    }
    break;
    case CFunc::SetDelayOn: {
        if (CAR(args))
            the_game->set_delay(1);
        else
            the_game->set_delay(0);
    }
    break;
    case CFunc::SetLogin: {
        set_login(lstring_value(CAR(args)));
    }
    break;
    case CFunc::EnableChatting: {
        chatting_enabled = 1;
    }
    break;
    case CFunc::DemoBreakEnable: {
        demo_start = 1;
    }
    break;
    case CFunc::AmAClient: {
        if (main_net_cfg && main_net_cfg->state == net_configuration::CLIENT)
            return 1;
    }
    break;
    case CFunc::TimeForNextLevel: {
        if (main_net_cfg &&
            (main_net_cfg->state == net_configuration::CLIENT || main_net_cfg->state == net_configuration::SERVER))
        {
            view *v = player_list;
            for (; v; v = v->next)
                if (v->kills >= main_net_cfg->kills)
                    return 1;
        }
        else
            return 0;
    }
    break;
    case CFunc::ResetKills: {
        view *v = player_list;
        for (; v; v = v->next)
        {
            v->tkills += v->kills;

            v->kills = 0;
            game_object *o = current_object;
            current_object = v->m_focus;

            ((LSymbol *)l_restart_player)->EvalFunction(NULL);
            v->reset_player();
            v->m_focus->set_aistate(0);
            current_object = o;
        }
    }
    break;
    case CFunc::SetGameName: {
        strncpy(game_name, lstring_value(CAR(args)), sizeof(game_name));
        game_name[sizeof(game_name) - 1] = 0;
    }
    break;
    case CFunc::SetNetMinPlayers: {
        if (main_net_cfg)
            main_net_cfg->min_players = lnumber_value(CAR(args));
    }
    break;
    case CFunc::SetObjectTint:
        if (current_object->Controller)
            current_object->Controller->set_tint(lnumber_value(CAR(args)));
        else
            current_object->set_tint(lnumber_value(CAR(args)));
        break;
    case CFunc::GetObjectTint:
        if (current_object->Controller)
            return current_object->Controller->get_tint();
        else
            return current_object->get_tint();
        break;
    case CFunc::SetObjectTeam:
        if (current_object->Controller)
            current_object->Controller->set_team(lnumber_value(CAR(args)));
        else
            current_object->set_team(lnumber_value(CAR(args)));
        break;
    case CFunc::GetObjectTeam:
        if (current_object->Controller)
            return current_object->Controller->get_team();
        else
            return current_object->get_team();
        break;
    default:
        printf("Undefined c function %ld\n", number);
        return 0;
    }
    return 0;
}