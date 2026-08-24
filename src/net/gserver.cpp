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
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#if defined HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>

#include "common.h"

#include "gserver.h"
#include "netface.h"
#include "netcfg.h"
#include "id.h"
#include "jwindow.h"
#include "input.h"
#include "dev.h"
#include "game.h"
#include "sdlport/setup.h"
#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_timer.h>

namespace
{
constexpr int ID_COPY_ROOM_CODE = 0x7f00;
}

extern base_memory_struct *base;
extern net_socket *comm_sock, *game_sock;
extern net_protocol *prot;
extern join_struct *join_array;
extern void service_net_request();
extern char lsf[256];
extern int start_running;

// Game server class implementation
game_server::game_server()
{
    DEBUG_LOG("Initializing game server");
    player_list = NULL;
    waiting_server_input = 1;
    reload_state = 0;
    lobby_open = false;
}

void game_server::restart_single_player()
{
    main_net_cfg->state = net_configuration::RESTART_SINGLE;
    start_running = 0;
    strcpy(lsf, "abuse.lsp");
    base->input_state = INPUT_PROCESSING;
}

int game_server::total_players()
{
    player_client *fl = player_list;
    int total = 1;
    for (; fl; fl = fl->next)
    {
        total++;
    }
    return total;
}

std::vector<game_server::client_status> game_server::client_statuses()
{
    const std::uint64_t now = SDL_GetTicksNS();
    std::vector<client_status> statuses;
    for (player_client *client = player_list; client; client = client->next)
    {
        if (!client->delete_me())
        {
            const std::uint64_t elapsed = now >= client->last_packet_ticks ? now - client->last_packet_ticks : 0;
            statuses.push_back({client->client_id, client->name, elapsed / 1000000});
        }
    }
    return statuses;
}

bool game_server::kick_client(int client_id)
{
    for (player_client *client = player_list; client; client = client->next)
    {
        if (client->client_id == client_id && !client->delete_me())
        {
            DEBUG_LOG("Host is kicking client %d", client_id);
            client->set_delete_me(1);
            if (base->input_state == INPUT_RELOAD)
            {
                if (client->wait_reload())
                    client->set_wait_reload(0);
                check_reload_wait();
            }
            else
                check_collection_complete();
            return true;
        }
    }
    return false;
}

// Keep every multiplayer game in a shared lobby until the host starts it.
void game_server::game_start_wait()
{
    DEBUG_LOG("Opening multiplayer lobby");

    int last_count = -1;
    Jwindow *stat = NULL;
    Event ev;
    int done = 0;
    bool start_game = false;
    lobby_open = true;

    while (!done && !application_quit_requested())
    {
        const int player_count = total_players();
        if (last_count != player_count)
        {
            if (stat)
                wm->close_window(stat);
            char msg[256];
            const bool show_room = main_net_cfg->online && main_net_cfg->room_code[0];
            if (show_room && main_net_cfg->streamer_mode)
                snprintf(msg, sizeof(msg), symbol_str("online_lobby_players_streamer"), player_count);
            else if (show_room)
                snprintf(msg, sizeof(msg), symbol_str("online_lobby_players"), main_net_cfg->room_code, player_count);
            else
                snprintf(msg, sizeof(msg), symbol_str("lobby_players"), player_count);

            ifield *controls;
            int x1, y1, message_width, message_bottom;
            info_field message_bounds(0, 0, ID_NULL, msg, NULL);
            message_bounds.area(x1, y1, message_width, message_bottom);
            const int button_y = message_bottom + 5;
            if (show_room)
            {
                char const *copy_text = symbol_str("copy_room_code");
                char const *action_text = symbol_str("start_game_button");
                const int font_width = wm->font()->Size().x;
                const int copy_width = strlen(copy_text) * font_width + 6;
                const int controls_width = (strlen(action_text) + strlen(copy_text)) * font_width + 18;
                if (message_width < controls_width)
                    message_width = controls_width;
                const int copy_x = message_width - copy_width;
                controls = new button(0, button_y, ID_START_GAME, action_text,
                                      new button(copy_x, button_y, ID_COPY_ROOM_CODE, copy_text, NULL));
            }
            else
            {
                controls = new button(0, button_y, ID_START_GAME, symbol_str("start_game_button"), NULL);
            }

            stat = wm->CreateWindow(ivec2(0), ivec2(-1), new info_field(0, 0, ID_NULL, msg, controls),
                                    symbol_str("lobby_title"));
            wm->move_window(stat, std::max(0, (xres - stat->m_size.x) / 2),
                            std::max(0, (yres - stat->m_size.y) / 2));
            wm->flush_screen();
            last_count = player_count;
            send_lobby_status();
            DEBUG_LOG("Updated player count to %d", last_count);
        }

        if (wm->IsPending())
        {
            do
            {
                wm->get_event(ev);
            } while (ev.type == EV_MOUSE_MOVE && wm->IsPending());
            wm->flush_screen();
            if (ev.type == EV_QUIT)
            {
                done = 1;
                continue;
            }
            if (ev.type == EV_MESSAGE && ev.message.id == ID_START_GAME)
            {
                DEBUG_LOG("Game start wait ended by host");
                start_game = true;
                done = 1;
            }
            else if (ev.type == EV_MESSAGE && ev.message.id == ID_COPY_ROOM_CODE)
            {
                if (!SDL_SetClipboardText(main_net_cfg->room_code))
                    DEBUG_LOG("Unable to copy room code: %s", SDL_GetError());
            }
        }

        service_net_request();

        SDL_Delay(1);
    }

    if (start_game)
        send_lobby_start();
    lobby_open = false;

    if (stat)
    {
        wm->close_window(stat);
        wm->flush_screen();
    }

    DEBUG_LOG("Game start wait complete");
}

void game_server::send_lobby_status()
{
    const uint8_t message[] = {SRVCMD_LOBBY_STATUS, static_cast<uint8_t>(total_players()),
                               static_cast<uint8_t>(main_net_cfg->max_players)};
    for (player_client *client = player_list; client; client = client->next)
    {
        if (!client->delete_me() && client->comm->write(/* server_lobby_status */ message, sizeof(message)) !=
                                        static_cast<int>(sizeof(message)))
            client->set_delete_me(1);
    }
}

void game_server::send_lobby_start()
{
    const uint8_t command = SRVCMD_LOBBY_START;
    for (player_client *client = player_list; client; client = client->next)
    {
        if (!client->delete_me() && client->comm->write(/* server_lobby_start */ &command, 1) != 1)
            client->set_delete_me(1);
    }
}

game_server::player_client::player_client(int client_id, char const *name, net_socket *comm,
                                         net_address *data_address, player_client *next)
    : flags(0), client_id(client_id), name(name ? name : ""), last_packet_ticks(SDL_GetTicksNS()), comm(comm),
      data_address(data_address), next(next)
{
    set_wait_input(1);
    comm->read_selectable();
}

game_server::player_client::~player_client()
{
    DEBUG_LOG("Destroying player client");
    delete comm;
    delete data_address;
}

// Check if all players have submitted input for current tick
void game_server::check_collection_complete()
{
    player_client *c;
    int got_all = waiting_server_input == 0;
    int add_deletes = 0;

    // Check for deleted clients and missing input
    for (c = player_list; c; c = c->next)
    {
        if (c->delete_me())
        {
            add_deletes = 1;
            DEBUG_LOG("Client %d marked for deletion", c->client_id);
        }
        else if (c->has_joined() && c->wait_input())
        {
            got_all = 0;
        }
    }

    // Remove deleted clients
    if (add_deletes)
    {
        DEBUG_LOG("Processing client deletions");
        player_client *last = NULL;
        for (c = player_list; c;)
        {
            if (c->delete_me())
            {
                base->packet.write_uint8(SCMD_DELETE_CLIENT);
                base->packet.write_uint8(c->client_id);
                DEBUG_LOG("Removing client %d", c->client_id);

                if (c->wait_reload())
                {
                    c->set_wait_reload(0);
                    check_reload_wait();
                }

                if (last)
                    last->next = c->next;
                else
                    player_list = c->next;
                player_client *d = c;
                c = c->next;
                delete d;
            }
            else
            {
                last = c;
                c = c->next;
            }
        }
    }

    // If we have all inputs, send packet to all clients
    if (got_all)
    {
        base->packet.calc_checksum();

        for (c = player_list; c; c = c->next)
        {
            if (c->has_joined())
            {
                c->set_wait_input(1);
                game_sock->write(/* server_game_state */ base->packet.data,
                                 base->packet.packet_size() + base->packet.packet_prefix_size(), c->data_address);
            }
        }

        base->input_state = INPUT_PROCESSING; // tell engine to start processing
        game_sock
            ->read_unselectable(); // don't listen to this socket until we are prepared to read next tick's game data
        waiting_server_input = 1;
    }
}

// Add server's own input to the game state
void game_server::add_engine_input()
{
    waiting_server_input = 0;
    base->input_state = INPUT_COLLECTING;
    base->packet.set_tick_received(base->current_tick);
    game_sock->read_selectable(); // we can listen for game data now that we have server input
    check_collection_complete();
}

// Add input from a client to the game state
void game_server::add_client_input(char *buf, int size, player_client *c)
{
    if (c->wait_input()) // don't add if we already have it
    {
        base->packet.add_to_packet(buf, size);
        c->set_wait_input(0);
        check_collection_complete();
    }
    else
    {
        DEBUG_LOG("Ignored duplicate input from client %d", c->client_id);
    }
}

// Check if all clients have completed reloading
void game_server::check_reload_wait()
{
    DEBUG_LOG("Checking reload wait status");
    player_client *d = player_list;
    for (; d; d = d->next)
    {
        if (d->wait_reload())
        {
            DEBUG_LOG("Still waiting for client %d to reload", d->client_id);
            return;
        }
    }
    DEBUG_LOG("All clients finished reloading");
    base->wait_reload = 0;
}

// Process commands received from a client
int game_server::process_client_command(player_client *c)
{
    uint8_t cmd;
    if (c->comm->read(/* client_command */ &cmd, 1) != 1)
    {
        DEBUG_LOG("Failed to read command from client %d", c->client_id);
        return 0;
    }

    c->last_packet_ticks = SDL_GetTicksNS();

    DEBUG_LOG("Processing command %d from client %d", cmd, c->client_id);

    switch (cmd)
    {
    case CLCMD_REQUEST_RESEND: {
        uint8_t tick;
        if (c->comm->read(/* client_resend_request_tick */ &tick, 1) != 1)
        {
            DEBUG_LOG("Failed to read tick for resend request");
            return 0;
        }

        DEBUG_LOG("Client %d requested resend of tick %d", c->client_id, tick);

        if (tick == base->last_packet.tick_received())
        {
            DEBUG_LOG("Resending last packet to client %d", c->client_id);
            net_packet *pack = &base->last_packet;
            game_sock->write(/* server_game_state */ pack->data, pack->packet_size() + pack->packet_prefix_size(),
                             c->data_address);
        }
        else
        {
            DEBUG_LOG("Tick not resent - requested:%d current:%d packet:%d last_packet:%d", tick, base->current_tick,
                      base->packet.tick_received(), base->last_packet.tick_received());
        }
        return 1;
    }
    break;

    case CLCMD_RELOAD_START: {
        if (reload_state)
        {
            DEBUG_LOG("Client %d requesting reload while reload in progress", c->client_id);
            const uint8_t ack = SRVCMD_RELOAD_START_OK;
            if (c->comm->write(/* server_command */ &ack, 1) != 1)
            {
                DEBUG_LOG("Failed to acknowledge reload to client %d", c->client_id);
                c->set_delete_me(1);
                return 0;
            }
        }
        else
        {
            // The snapshot does not exist yet. start_reload() sends the ACK
            // after the server has saved the authoritative level state.
            DEBUG_LOG("Client %d waiting for server reload to start", c->client_id);
            c->set_need_reload_start_ok(1);
        }

        return 1;
    }
    break;

    case CLCMD_RELOAD_END: {
        DEBUG_LOG("Client %d finished reloading", c->client_id);
        c->set_wait_reload(0);
        return 1;
    }
    break;

    case CLCMD_UNJOIN: {
        DEBUG_LOG("Client %d requesting disconnect", c->client_id);
        c->comm->write(/* server_disconnect_ack */ &cmd, 1);
        c->set_delete_me(1);
        if (base->input_state == INPUT_COLLECTING)
            check_collection_complete();
    }
    break;
    }
    return 0;
}

// Process all network activity for the server
int game_server::process_net()
{
    if (!game_sock || game_sock->error())
    {
        DEBUG_LOG("Server game socket error detected");
        restart_single_player();
        return 0;
    }

    int ret = 0;

    // Handle incoming game data
    if ((base->input_state == INPUT_COLLECTING || base->input_state == INPUT_RELOAD) && game_sock->ready_to_read())
    {
        net_packet tmp;
        net_packet *use = &tmp;
        net_address *from;
        int bytes_received = game_sock->read(/* client_input_data */ use->data, PACKET_MAX_SIZE, &from);

        if (from && bytes_received)
        {
            if (bytes_received == use->packet_size() + use->packet_prefix_size())
            {
                uint16_t rec_crc = use->get_checksum();
                if (rec_crc == use->calc_checksum())
                {
                    player_client *f = player_list, *found = NULL;
                    for (; !found && f; f = f->next)
                    {
                        // More than one client can legitimately have the same
                        // IP address. The UDP source port is part of the game
                        // endpoint and distinguishes those clients.
                        if (f->has_joined() && from->equal(f->data_address) &&
                            from->get_port() == f->data_address->get_port())
                            found = f;
                    }

                    if (found)
                    {
                        found->last_packet_ticks = SDL_GetTicksNS();
                        if (base->current_tick == use->tick_received())
                        {
                            if (base->input_state != INPUT_RELOAD)
                                add_client_input((char *)use->packet_data(), use->packet_size(), found);
                        }
                        else if (use->tick_received() == base->last_packet.tick_received())
                        {
                            DEBUG_LOG("Received stale data from client %d, resending last packet", found->client_id);
                            net_packet *pack = &base->last_packet;
                            game_sock->write(/* server_game_state */ pack->data,
                                             pack->packet_size() + pack->packet_prefix_size(), found->data_address);
                        }
                        else
                        {
                            DEBUG_LOG("Received out of sequence data from client %d (got %d, expected %d)",
                                      found->client_id, use->tick_received(), base->current_tick);
                        }
                    }
                    else
                    {
                        DEBUG_LOG("Received data from unknown client");
                        fprintf(stderr, "received data from unknown client\n");
                        printf("from address ");
                        from->print();
                        printf(" first addr ");
                        player_list->data_address->print();
                        printf("\n");
                    }
                }
                else
                {
                    DEBUG_LOG("Received packet with invalid checksum");
                }
            }
            else
            {
                DEBUG_LOG("Received incomplete packet");
            }
        }
        else if (!from)
        {
            DEBUG_LOG("Received data with no sender address");
        }
        else if (!bytes_received)
        {
            DEBUG_LOG("Received empty packet");
        }

        ret = 1;
        if (from)
            delete from;
    }
    else
    {
        DEBUG_LOG("No game data available");
    }

    // Process client commands
    player_client *c;
    for (c = player_list; c; c = c->next)
    {
        if (c->comm->error() || (c->comm->ready_to_read() && !process_client_command(c)))
        {
            DEBUG_LOG("Communication error with client %d, marking for deletion", c->client_id);
            c->set_delete_me(1);
        }
        else
            ret = 1;
    }

    check_collection_complete();

    if (game_sock->error())
    {
        DEBUG_LOG("Server game socket failed while processing network data");
        restart_single_player();
        return 0;
    }

    return 1;
}

int game_server::input_missing()
{
    // The simulation cannot advance until every joined client contributes its
    // input for this tick. Ask only the clients still missing, over the reliable
    // control channel; their actual tick data remains a low-latency datagram.
    const uint8_t cmd = SRVCMD_REQUEST_RESEND;
    const uint8_t tick = static_cast<uint8_t>(base->current_tick);
    for (player_client *client = player_list; client; client = client->next)
    {
        if (client->has_joined() && client->wait_input() && !client->delete_me())
        {
            DEBUG_LOG("Requesting tick %d resend from client %d", tick, client->client_id);
            if (client->comm->write(/* server_command */ &cmd, 1) != 1 ||
                client->comm->write(/* server_resend_request_tick */ &tick, 1) != 1)
                client->set_delete_me(1);
        }
    }
    return 1;
}

// Handle level reload completion
int game_server::end_reload(int disconnect)
{
    DEBUG_LOG("Ending reload (disconnect=%d)", disconnect);
    player_client *c = player_list;
    prot->select(false);

    // Check if any clients still haven't reloaded
    for (; c; c = c->next)
    {
        if (!c->delete_me() && c->wait_reload())
        {
            if (disconnect)
            {
                DEBUG_LOG("Disconnecting client %d who hasn't finished reloading", c->client_id);
                c->set_delete_me(1);
            }
            else
            {
                DEBUG_LOG("Still waiting for client %d to finish reload", c->client_id);
                return 0;
            }
        }
    }

    // Mark all clients as joined and clear reload state
    DEBUG_LOG("All clients finished reloading, marking as joined");
    for (c = player_list; c; c = c->next)
    {
        c->set_has_joined(1);
    }
    reload_state = 0;

    return 1;
}

// Initiate level reload for all clients
int game_server::start_reload()
{
    DEBUG_LOG("Starting level reload");
    player_client *c = player_list;
    reload_state = 1;
    prot->select(false);

    for (; c; c = c->next)
    {
        if (!c->delete_me() &&
            c->need_reload_start_ok()) // if the client is already waiting for reload state to start, send ok
        {
            DEBUG_LOG("Sending reload start OK to waiting client %d", c->client_id);
            uint8_t cmd = SRVCMD_RELOAD_START_OK;
            if (c->comm->write(/* server_command */ &cmd, 1) != 1)
            {
                DEBUG_LOG("Failed to send reload OK to client %d", c->client_id);
                c->set_delete_me(1);
            }
            c->set_need_reload_start_ok(0);
        }
        c->set_wait_reload(1);
    }
    return 1;
}

// Check if a client ID is valid
int game_server::isa_client(int client_id)
{
    if (!client_id)
    {
        DEBUG_LOG("Client ID 0 is always valid (server)");
        return 1;
    }

    player_client *c = player_list;
    for (; c; c = c->next)
    {
        if (c->client_id == client_id)
        {
            DEBUG_LOG("Found valid client ID %d", client_id);
            return 1;
        }
    }

    DEBUG_LOG("Invalid client ID %d", client_id);
    return 0;
}

// Add a new client connection
int game_server::add_client(int type, net_socket *sock, net_address *from)
{
    DEBUG_LOG("Adding new client connection type %d", type);

    if (type == CLIENT_ABUSE)
    {
        if (total_players() >= main_net_cfg->max_players)
        {
            DEBUG_LOG("Rejecting client - server full (%d/%d players)", total_players(), main_net_cfg->max_players);
            uint8_t too_many = SRVCMD_TOO_MANY;
            sock->write(/* server_registration_response */ &too_many, 1);
            return 0;
        }

        // Write registration confirmation
        uint8_t reg = SRVCMD_REGISTRATION_OK;
        if (sock->write(/* server_registration_response */ &reg, 1) != 1)
        {
            DEBUG_LOG("Failed to send registration confirmation");
            return 0;
        }

        // Exchange initial connection data
        uint16_t our_port = lstl(main_net_cfg->port + 1), cport;
        char name[256];
        uint8_t len, skin;
        int16_t nkills = lstl(main_net_cfg->kills);
        uint8_t gmode = (uint8_t)main_net_cfg->game_mode;
        uint8_t lobby = lobby_open ? 1 : 0;
        uint8_t connected_players = static_cast<uint8_t>(total_players() + 1);
        uint8_t max_players = static_cast<uint8_t>(main_net_cfg->max_players);

        if (sock->read(/* client_name_length */ &len, 1) != 1 || sock->read(/* client_name_data */ name, len) != len ||
            sock->read(/* client_skin */ &skin, 1) != 1 || sock->read(/* client_port */ &cport, 2) != 2 ||
            sock->write(/* server_port */ &our_port, 2) != 2 ||
            sock->write(/* server_kills */ &nkills, 2) != 2 || sock->write(/* server_game_mode */ &gmode, 1) != 1 ||
            sock->write(/* server_lobby_state */ &lobby, 1) != 1 ||
            sock->write(/* server_lobby_players */ &connected_players, 1) != 1 ||
            sock->write(/* server_max_players */ &max_players, 1) != 1)
        {
            DEBUG_LOG("Failed to exchange connection data");
            return 0;
        }
        name[len] = '\0';

        cport = lstl(cport);
        DEBUG_LOG("Client connection data - Name: %s, Port: %d", name, cport);

        // Find available client ID
        int f = -1, i;
        for (i = 0; f == -1 && i < MAX_JOINERS; i++)
        {
            if (!isa_client(i))
            {
                f = i;
                join_struct *j = base->join_list;
                for (; j; j = j->next)
                {
                    if (j->client_id == i)
                        f = -1;
                }
            }
        }

        if (f == -1)
        {
            DEBUG_LOG("No available client IDs");
            return 0;
        }

        // Set up client connection
        from->set_port(cport);
        uint16_t client_id = lstl(f);
        if (sock->write(/* server_client_id */ &client_id, 2) != 2)
        {
            DEBUG_LOG("Failed to send client ID");
            return 0;
        }

        client_id = f;
        DEBUG_LOG("Assigned client ID %d", client_id);

        // Add to join list and create player client
        join_array[client_id].next = base->join_list;
        base->join_list = &join_array[client_id];
        join_array[client_id].client_id = client_id;
        join_array[client_id].skin = static_cast<uint8_t>(std::clamp<int>(skin, 0, PLAYER_SKIN_COUNT - 1));
        copy_player_name(join_array[client_id].name, sizeof(join_array[client_id].name), name);
        player_list = new player_client(f, join_array[client_id].name, sock, from, player_list);

        DEBUG_LOG("Client %d successfully added", client_id);
        return 1;
    }
    else
    {
        DEBUG_LOG("Rejecting unknown client type %d", type);
        return 0;
    }
}

// Mark non-responsive clients for deletion
int game_server::kill_slackers()
{
    DEBUG_LOG("Checking for non-responsive clients");
    player_client *c = player_list;
    for (; c; c = c->next)
    {
        if (c->wait_input())
        {
            DEBUG_LOG("Marking non-responsive client %d for deletion", c->client_id);
            c->set_delete_me(1);
        }
    }
    check_collection_complete();
    return 1;
}

// Clean shutdown of server
int game_server::quit()
{
    DEBUG_LOG("Shutting down game server");
    player_client *c = player_list;
    while (c)
    {
        player_client *d = c;
        c = c->next;
        DEBUG_LOG("Deleting client %d", d->client_id);
        delete d;
    }
    player_list = NULL;
    return 1;
}

game_server::~game_server()
{
    DEBUG_LOG("Destroying game server");
    quit();
}
