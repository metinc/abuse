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
#include "nfserver.h"
#include "netcfg.h"
#include "file_utils.h"

demo_manager demo_man;
ivec2 last_demo_mpos;
int last_demo_mbut;
extern base_memory_struct *base; // points to shm_addr

void get_event(Event &ev)
{
    wm->get_event(ev);
    if (demo_man.state == demo_manager::PLAYING)
    {
        if (ev.type == EV_KEY && ev.key == JK_ESC)
        {
            demo_man.set_state(demo_manager::NORMAL);
            ev.type = EV_SPURIOUS;
            return;
        }

        if (ev.type == EV_MOUSE_MOVE || ev.type == EV_MOUSE_BUTTON || ev.type == EV_KEY ||
            ev.type == EV_KEYRELEASE || ev.type == EV_TEXT_INPUT)
        {
            // Replay packets own all player input. Only a physical Escape key
            // is allowed to leave playback.
            ev.type = EV_SPURIOUS;
            return;
        }
    }

    switch (ev.type)
    {
    case EV_KEY: {
        if (ev.key == JK_ENTER && demo_man.state == demo_manager::RECORDING &&
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

std::string automatic_replay_filename()
{
    // Count in decimal from zero; reserve both files and keep DOS 8.3 names.
    char filename[32];
    for (uint32_t number = 0; number <= 99999999; ++number)
    {
        std::snprintf(filename, sizeof(filename), "replays/%08lu.dat", static_cast<unsigned long>(number));
        if (!std::filesystem::exists(replay_write_path(filename)) &&
            !std::filesystem::exists(replay_write_path(filename).replace_extension(".spe")))
            return filename;
    }
    return {};
}

std::filesystem::path temporary_replay_checkpoint_path()
{
    std::error_code error;
    const std::filesystem::path directory = std::filesystem::temp_directory_path(error);
    if (error)
        return {};

    // Temporary snapshots need no descriptive name. Keep them usable on 8.3
    // filesystems, and skip existing files when the shortened nonce collides.
    uint32_t nonce = static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    char filename[13];
    std::filesystem::path path;
    do
    {
        std::snprintf(filename, sizeof(filename), "%08lx.tmp", static_cast<unsigned long>(nonce++));
        path = directory / filename;
        const bool exists = std::filesystem::exists(path, error);
        if (error)
            return {};
        if (!exists)
            return path;
    } while (true);
}

// Reload snapshots live inside the replay, not in the shared netstart.spe.
// A temporary file lets the existing level serializer read and write them.
struct temporary_replay_snapshot
{
    std::filesystem::path path = temporary_replay_checkpoint_path();

    ~temporary_replay_snapshot()
    {
        std::error_code error;
        if (!path.empty())
            std::filesystem::remove(path, error);
    }
};

constexpr uint32_t max_replay_snapshot_size = 64 * 1024 * 1024;

bool copy_replay_snapshot(bFILE &source, bFILE &destination, uint32_t size)
{
    uint8_t buffer[16384];
    while (size)
    {
        const uint32_t count = std::min<uint32_t>(size, sizeof(buffer));
        if (source.read(buffer, count) != count || destination.write(buffer, count) != count)
            return false;
        size -= count;
    }
    return true;
}
}

void demo_manager::clear_playback_checkpoint()
{
    if (playback_checkpoint_path.empty())
        return;

    std::error_code error;
    std::filesystem::remove(playback_checkpoint_path, error);
    playback_checkpoint_path.clear();
}

bool demo_manager::save_playback_checkpoint()
{
    if (state != PLAYING || !current_level)
        return false;

    const std::filesystem::path path = temporary_replay_checkpoint_path();
    if (path.empty())
        return false;

    if (!current_level->save(path.string().c_str(), 1, NULL, false))
    {
        std::error_code error;
        std::filesystem::remove(path, error);
        return false;
    }

    std::error_code error;
    if (!playback_checkpoint_path.empty())
        std::filesystem::remove(playback_checkpoint_path, error);
    playback_checkpoint_path = path.string();

    return true;
}

bool demo_manager::load_playback_checkpoint()
{
    if (state != PLAYING || playback_checkpoint_path.empty())
        return false;

    the_game->request_level_load(playback_checkpoint_path.c_str());
    return true;
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
        !current_level->save(replay_write_path(snapshot.c_str()).string().c_str(), 1, NULL, false, true))
    {
        delete record_file;
        record_file = NULL;
        std::error_code error;
        std::filesystem::remove(replay_write_path(filename), error);
        return 0;
    }

    record_file->write((void *)"DEMO,VERSION:5", 14);
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

    const bool cooperative = main_net_cfg && main_net_cfg->game_mode == net_configuration::COOP;
    record_file->write_uint8(cooperative ? 1 : 0);
    record_file->write_uint8(client_number());

    network_reloaded = false;
    recording_level_changed = false;
    state = RECORDING;
    std::printf("Recording replay to %s\n", replay_write_path(filename).string().c_str());

    return 1;
}

int demo_manager::start_automatic_recording()
{
    if (state != NORMAL)
        return 0;

    const std::string filename = automatic_replay_filename();
    automatic_recording = true;
    if (!start_recording(filename.c_str()))
    {
        automatic_recording = false;
        return 0;
    }
    return 1;
}

void demo_manager::prepare_recording()
{
    if (state != RECORDING || !recording_level_changed)
        return;

    // Level loading can occur between input processing and the world tick.
    // Capture the next segment only at the next packet boundary, after that
    // tick, so its snapshot and first input describe the same simulation state.
    const bool was_automatic = automatic_recording;
    set_state(NORMAL);
    const std::string filename = automatic_replay_filename();
    if (start_recording(filename.c_str()))
        automatic_recording = was_automatic;
    else
        std::fprintf(stderr, "Unable to start replay recording for the loaded level\n");
}

void demo_manager::do_inputs()
{
    switch (state)
    {
    case RECORDING: {
        prepare_recording();
        base->packet.packet_reset(); // reset input buffer
        view *p = player_list; // get current inputs
        for (; p; p = p->next)
            if (p->local_player())
                p->get_input();

        base->packet.write_uint8(SCMD_SYNC);
        base->packet.write_uint16(make_sync());
        process_packet_commands(base->packet.packet_data(), base->packet.packet_size());
        demo_man.save_packet(base->packet.packet_data(), base->packet.packet_size());
    }
    break;
    case PLAYING: {
        uint8_t buf[PACKET_MAX_SIZE + 1];
        int size;
        if (get_packet(buf, size)) // get starting inputs
        {
            process_packet_commands(buf, size);
            for (view *p = player_list; p; p = p->next)
                if (p->local_player())
                {
                    ivec2 mouse = the_game->GameToMouse(ivec2(p->pointer_x, p->pointer_y), p);
                    wm->SetMousePos((small_render ? 2 : 1) * mouse);
                    break;
                }
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

    reload_snapshots = memcmp(sig, "DEMO,VERSION:5", 14) == 0;
    const bool mode_replay = reload_snapshots || memcmp(sig, "DEMO,VERSION:4", 14) == 0;
    const bool snapshot_replay = mode_replay || memcmp(sig, "DEMO,VERSION:3", 14) == 0;
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

    uint8_t recorded_game_mode = 0;
    if (mode_replay && record_file->read(&recorded_game_mode, 1) != 1)
    {
        delete record_file;
        record_file = NULL;
        return 0;
    }

    uint8_t player_number = 0;
    if (reload_snapshots && record_file->read(&player_number, 1) != 1)
    {
        delete record_file;
        record_file = NULL;
        return 0;
    }
    recorded_player_number = reload_snapshots ? player_number : client_number();

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

    game_mode_overridden = mode_replay && main_net_cfg;
    if (game_mode_overridden)
    {
        initial_game_mode = main_net_cfg->game_mode;
        initial_ant_multiplier = main_net_cfg->ant_multiplier;
        main_net_cfg->game_mode = recorded_game_mode == 1 ? net_configuration::COOP : net_configuration::DEATHMATCH;
    }

    // Snapshot loading recalculates the local viewport, so select the recorded
    // player before loading it (a client recording need not follow player 0).
    state = PLAYING;
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

    if (!save_playback_checkpoint())
        std::fprintf(stderr, "Unable to create the replay checkpoint\n");

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
        recording_level_changed = false;
    }
    break;
    case PLAYING: {
        delete record_file;
        record_file = NULL;
        clear_playback_checkpoint();
        l_difficulty->SetValue(initial_difficulty);
        if (game_mode_overridden && main_net_cfg)
        {
            main_net_cfg->game_mode = initial_game_mode == net_configuration::COOP ? net_configuration::COOP
                                                                                   : net_configuration::DEATHMATCH;
            main_net_cfg->ant_multiplier = initial_ant_multiplier;
        }
        game_mode_overridden = false;
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
        if (record_file->write(&ps, 2) != 2 || record_file->write(packet, packet_size) != packet_size ||
            !write_reload_snapshot())
        {
            std::fprintf(stderr, "Unable to write replay packet or network reload snapshot\n");
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

        if (ps > PACKET_MAX_SIZE || record_file->read(packet, ps) != ps)
        {
            set_state(NORMAL);
            return 0;
        }

        packet_size = ps;
        if (reload_snapshots)
        {
            bool loaded = false;
            if (!read_reload_snapshot(loaded))
            {
                std::fprintf(stderr, "Unable to read replay network reload snapshot\n");
                set_state(NORMAL);
                return 0;
            }
            // This snapshot already includes every command in the packet. It
            // replaces that tick's input processing, before the world advances.
            if (loaded)
                packet_size = 0;
        }
        return 1;
    }
    return 0;
}

bool demo_manager::write_reload_snapshot()
{
    // Version 5 appends a uint32 byte count and optional .spe contents to each
    // input packet. Capture after processing, including joins and sync reloads.
    if (!network_reloaded)
    {
        const uint32_t size = 0;
        return record_file->write(&size, sizeof(size)) == sizeof(size);
    }

    temporary_replay_snapshot snapshot;
    if (snapshot.path.empty() || !current_level ||
        !current_level->save(snapshot.path.string().c_str(), 1, NULL, false, true))
        return false;

    jFILE source(snapshot.path.string().c_str(), "rb");
    if (source.open_failure() || source.file_size() <= 0 || source.file_size() > max_replay_snapshot_size)
        return false;

    const uint32_t size = source.file_size();
    const uint32_t encoded_size = lltl(size);
    if (record_file->write(&encoded_size, sizeof(encoded_size)) != sizeof(encoded_size) ||
        !copy_replay_snapshot(source, *record_file, size))
        return false;

    network_reloaded = false;
    return true;
}

bool demo_manager::read_reload_snapshot(bool &loaded)
{
    uint32_t size;
    if (record_file->read(&size, sizeof(size)) != sizeof(size))
        return false;
    size = lltl(size);
    if (!size)
        return true;
    if (size > max_replay_snapshot_size)
        return false;

    temporary_replay_snapshot snapshot;
    if (snapshot.path.empty())
        return false;
    {
        jFILE destination(snapshot.path.string().c_str(), "wb");
        if (destination.open_failure() || !copy_replay_snapshot(*record_file, destination, size))
            return false;
    }

    jFILE source(snapshot.path.string().c_str(), "rb");
    if (source.open_failure())
        return false;
    spec_directory directory(&source);
    if (!directory.find("player_info"))
        return false;

    // Match net_reload(): ordinary level loading also runs level-loaded hooks
    // and clears transient state, which would change the recorded simulation.
    delete current_level;
    current_level = new level(&directory, &source, snapshot.path.string().c_str());
    base->current_tick = current_level->tick_counter() & 0xff;
    loaded = true;
    return true;
}
