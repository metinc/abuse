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
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <limits>
#include <string>

#include "common.h"

#include "game.h"

#include "demo.h"
#include "specs.h"
#include "jwindow.h"
#include "dev.h"
#include "jrand.h"
#include "lisp.h"
#include "clisp.h"
#include "net/netface.h"
#include "file_utils.h"

demo_manager demo_man;
ivec2 last_demo_mpos;
int last_demo_mbut;
extern base_memory_struct *base; // points to shm_addr

void get_event(Event &ev)
{
    wm->get_event(ev);
    if (demo_man.state == demo_manager::PLAYING &&
        (ev.type == EV_MOUSE_MOVE || ev.type == EV_MOUSE_BUTTON))
    {
        // Demo packets own the pointer position and buttons. Physical mouse
        // input must not leak into playback between recorded ticks.
        ev.type = EV_SPURIOUS;
        return;
    }

    switch (ev.type)
    {
    case EV_KEY: {
        if (demo_man.state == demo_manager::PLAYING)
            demo_man.set_state(demo_manager::NORMAL);
        else if (ev.key == JK_ENTER && demo_man.state == demo_manager::RECORDING &&
                 !demo_man.is_automatic_recording())
        {
            demo_man.set_state(demo_manager::NORMAL);
            the_game->show_help("Finished recording");
        }
    }
    break;
    }

    last_demo_mpos = ev.mouse_move;
    last_demo_mbut = ev.mouse_button;
}

bool event_waiting()
{
    return wm->IsPending();
}

namespace
{
std::filesystem::path replay_write_path(char const *filename)
{
    std::filesystem::path path(filename);
    if (path.is_relative())
    {
        char const *prefix = get_save_filename_prefix();
        if (prefix && prefix[0])
            path = std::filesystem::path(prefix) / path;
    }
    return path;
}

std::string timestamped_replay_filename()
{
    using namespace std::chrono;
    const system_clock::time_point now = system_clock::now();
    const std::time_t time = system_clock::to_time_t(now);
    std::tm local_time = {};
#ifdef WIN32
    localtime_s(&local_time, &time);
#else
    localtime_r(&time, &local_time);
#endif
    const long milliseconds = duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;

    char filename[80];
    std::snprintf(filename, sizeof(filename), "replays/replay-%04d%02d%02d-%02d%02d%02d-%03ld.dat",
                  local_time.tm_year + 1900, local_time.tm_mon + 1, local_time.tm_mday, local_time.tm_hour,
                  local_time.tm_min, local_time.tm_sec, milliseconds);
    return filename;
}
}

int demo_manager::start_recording(char const *filename)
{
    if (!current_level || !filename || !filename[0])
        return 0;

    record_file = open_file(filename, "wb");
    if (record_file->open_failure())
    {
        delete record_file;
        record_file = NULL;
        return 0;
    }

    std::filesystem::path snapshot_name(filename);
    snapshot_name.replace_extension(".spe");
    const std::string snapshot = snapshot_name.generic_string();
    if (snapshot.size() + 1 > std::numeric_limits<uint16_t>::max() ||
        !current_level->save(replay_write_path(snapshot.c_str()).string().c_str(), 1, NULL, false))
    {
        delete record_file;
        record_file = NULL;
        std::error_code error;
        std::filesystem::remove(replay_write_path(filename), error);
        return 0;
    }

    record_file->write((void *)"DEMO,VERSION:3", 14);
    record_file->write_uint16(static_cast<uint16_t>(snapshot.size() + 1));
    record_file->write(snapshot.c_str(), snapshot.size() + 1);

    if (DEFINEDP(symbol_value(l_difficulty)))
    {
        if (symbol_value(l_difficulty) == l_easy)
            record_file->write_uint8(0);
        else if (symbol_value(l_difficulty) == l_medium)
            record_file->write_uint8(1);
        else if (symbol_value(l_difficulty) == l_hard)
            record_file->write_uint8(2);
        else
            record_file->write_uint8(3);
    }
    else
        record_file->write_uint8(3);

    state = RECORDING;
    std::printf("Recording replay to %s\n", replay_write_path(filename).string().c_str());

    return 1;
}

int demo_manager::start_automatic_recording()
{
    if (state != NORMAL)
        return 0;

    const std::string filename = timestamped_replay_filename();
    automatic_recording = true;
    if (!start_recording(filename.c_str()))
    {
        automatic_recording = false;
        return 0;
    }
    return 1;
}

void demo_manager::do_inputs()
{
    switch (state)
    {
    case RECORDING: {
        base->packet.packet_reset(); // reset input buffer
        view *p = player_list; // get current inputs
        for (; p; p = p->next)
            if (p->local_player())
                p->get_input();

        base->packet.write_uint8(SCMD_SYNC);
        base->packet.write_uint16(make_sync());
        demo_man.save_packet(base->packet.packet_data(), base->packet.packet_size());
        process_packet_commands(base->packet.packet_data(), base->packet.packet_size());
    }
    break;
    case PLAYING: {
        uint8_t buf[1500];
        int size;
        if (get_packet(buf, size)) // get starting inputs
        {
            process_packet_commands(buf, size);
            ivec2 mouse = the_game->GameToMouse(ivec2(player_list->pointer_x, player_list->pointer_y), player_list);
            wm->SetMousePos((small_render ? 2 : 1) * mouse);
        }
        else
        {
            set_state(NORMAL);
            return;
        }
    }
    break;
    default:
        break;
    }
}

void demo_manager::reset_game()
{
    if (dev & EDIT_MODE)
        toggle_edit_mode();
    the_game->set_state(RUN_STATE);
    rand_on = 0;

    view *v = player_list;
    for (; v; v = v->next)
    {
        if (v->m_focus)
            v->reset_player();
    }

    last_demo_mpos = ivec2(0, 0);
    last_demo_mbut = 0;
    current_level->set_tick_counter(0);
}

int demo_manager::start_playing(char const *filename)
{
    uint8_t sig[15];
    record_file = open_file(filename, "rb");
    if (record_file->open_failure())
    {
        delete record_file;
        record_file = NULL;
        return 0;
    }
    if (record_file->read(sig, 14) != 14)
    {
        delete record_file;
        record_file = NULL;
        return 0;
    }

    const bool snapshot_replay = memcmp(sig, "DEMO,VERSION:3", 14) == 0;
    if (!snapshot_replay && memcmp(sig, "DEMO,VERSION:2", 14) != 0)
    {
        delete record_file;
        record_file = NULL;
        return 0;
    }

    uint16_t name_size = snapshot_replay ? record_file->read_uint16() : record_file->read_uint8();
    if (!name_size || name_size > 4096)
    {
        delete record_file;
        record_file = NULL;
        return 0;
    }

    std::string name(name_size, '\0');
    uint8_t diff;
    if (record_file->read(name.data(), name_size) != name_size || name.back() != '\0' ||
        record_file->read(&diff, 1) != 1)
    {
        delete record_file;
        record_file = NULL;
        return 0;
    }
    name.pop_back();

    std::replace(name.begin(), name.end(), '\\', '/');
    std::string tname(name);

    bFILE *probe = open_file(tname.c_str(), "rb"); // see if the level still exists?
    if (probe->open_failure())
    {
        delete probe;

        // Bundled replays keep their original embedded level name for
        // compatibility.  If that path no longer exists, look for the level
        // snapshot beside the replay file instead.
        std::string replay_path(filename);
        std::replace(replay_path.begin(), replay_path.end(), '\\', '/');
        std::string embedded_path(tname);
        size_t replay_slash = replay_path.find_last_of('/');
        size_t embedded_slash = embedded_path.find_last_of('/');

        if (replay_slash != std::string::npos)
        {
            std::string adjacent_level = replay_path.substr(0, replay_slash + 1) +
                                         embedded_path.substr(embedded_slash == std::string::npos
                                                                  ? 0
                                                                  : embedded_slash + 1);
            tname = adjacent_level;
            probe = open_file(tname.c_str(), "rb");
        }
        else
            probe = NULL;

        if (!probe || probe->open_failure())
        {
            delete record_file;
            record_file = NULL;
            delete probe;
            return 0;
        }
    }
    delete probe;

    if ((dev & EDIT_MODE) && !the_game->set_editor_mode(false))
    {
        delete record_file;
        record_file = NULL;
        return 0;
    }

    the_game->load_level(tname.c_str());
    initial_difficulty = l_difficulty->GetValue();

    switch (diff)
    {
    case 0:
        l_difficulty->SetValue(l_easy);
        break;
    case 1:
        l_difficulty->SetValue(l_medium);
        break;
    case 2:
        l_difficulty->SetValue(l_hard);
        break;
    case 3:
        l_difficulty->SetValue(l_extreme);
        break;
    }

    state = PLAYING;
    if (snapshot_replay)
    {
        the_game->set_state(RUN_STATE);
        last_demo_mpos = ivec2(0, 0);
        last_demo_mbut = 0;
    }
    else
        reset_game();

    return 1;
}

int demo_manager::set_state(demo_state new_state, char const *filename)
{
    if (new_state == state)
        return 1;

    switch (state)
    {
    case RECORDING: {
        delete record_file;
        record_file = NULL;
        automatic_recording = false;
    }
    break;
    case PLAYING: {
        delete record_file;
        record_file = NULL;
        l_difficulty->SetValue(initial_difficulty);
        // Playback has ended before we return to the menu.  Game::set_state()
        // uses this state to choose the cursor, and PLAYING selects a blank one.
        state = NORMAL;
        the_game->set_state(MENU_STATE);
        wm->PushMessage(ID_NULL);

        view *v = player_list;
        for (; v; v = v->next) // reset all the players
        {
            if (v->m_focus)
            {
                v->reset_player();
                v->m_focus->set_aistate(0);
            }
        }
        delete current_level;
        current_level = NULL;
        the_game->reset_keymap();
        base->input_state = INPUT_PROCESSING;
    }
    break;
    default:
        break;
    }

    switch (new_state)
    {
    case RECORDING: {
        return start_recording(filename);
    }
    break;
    case PLAYING: {
        return start_playing(filename);
    }
    break;
    case NORMAL: {
        state = NORMAL;
    }
    break;
    }

    return 1;
}

int demo_manager::save_packet(void *packet, int packet_size) // returns non 0 if actually saved
{
    if (state == RECORDING)
    {
        uint16_t ps = lstl(packet_size);
        if (record_file->write(&ps, 2) != 2 || record_file->write(packet, packet_size) != packet_size)
        {
            set_state(NORMAL);
            return 0;
        }
        return 1;
    }
    else
        return 0;
}

int demo_manager::get_packet(void *packet, int &packet_size) // returns non 0 if actually loaded
{
    if (state == PLAYING)
    {
        uint16_t ps;
        if (record_file->read(&ps, 2) != 2)
        {
            set_state(NORMAL);
            return 0;
        }
        ps = lstl(ps);

        if (record_file->read(packet, ps) != ps)
        {
            set_state(NORMAL);
            return 0;
        }

        packet_size = ps;
        return 1;
    }
    return 0;
}
