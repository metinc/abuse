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
#include "timing.h"
#include "netcfg.h"
#include "id.h"
#include "jwindow.h"
#include "input.h"
#include "dev.h"
#include "game.h"

extern base_memory_struct *base;
extern net_socket *comm_sock, *game_sock;
extern net_protocol *prot;
extern join_struct *join_array;
extern void service_net_request();

// Game server class implementation
game_server::game_server()
{
    DEBUG_LOG("Initializing game server");
    player_list = NULL;
    waiting_server_input = 1;
    reload_state = 0;
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

// Wait for minimum number of players to join before starting game
void game_server::game_start_wait()
{
    DEBUG_LOG("Waiting for minimum players (%d needed)", main_net_cfg->min_players);

    int last_count = 0;
    Jwindow *stat = NULL;
    Event ev;
    int abort = 0;

    while (!abort && total_players() < main_net_cfg->min_players)
    {
        if (last_count != total_players())
        {
            if (stat)
                wm->close_window(stat);
            char msg[100];
            sprintf(msg, symbol_str("min_wait"), main_net_cfg->min_players - total_players());
            stat = wm->CreateWindow(
                ivec2(100, 50), ivec2(-1),
                new info_field(0, 0, ID_NULL, msg,
                               new button(0, wm->font()->Size().y * 2, ID_CANCEL, symbol_str("cancel_button"), NULL)));
            wm->flush_screen();
            last_count = total_players();
            DEBUG_LOG("Updated player count to %d", last_count);
        }

        if (wm->IsPending())
        {
            do
            {
                wm->get_event(ev);
            } while (ev.type == EV_MOUSE_MOVE && wm->IsPending());
            wm->flush_screen();
            if (ev.type == EV_MESSAGE && ev.message.id == ID_CANCEL)
            {
                DEBUG_LOG("Game start wait canceled by user");
                abort = 1;
            }
        }

        service_net_request();
    }

    if (stat)
    {
        wm->close_window(stat);
        wm->flush_screen();
    }

    DEBUG_LOG("Game start wait complete");
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
    DEBUG_LOG("Checking input collection status");

    player_client *c;
    int got_all = waiting_server_input == 0;
    int add_deletes = 0;

    // Check for deleted clients and missing input
    for (c = player_list; c && got_all; c = c->next)
    {
        if (c->delete_me())
        {
            add_deletes = 1;
            DEBUG_LOG("Client %d marked for deletion", c->client_id);
        }
        else if (c->has_joined() && c->wait_input())
        {
            got_all = 0;
            DEBUG_LOG("Still waiting for input from client %d", c->client_id);
        }
    }

    if (got_all)
    {
        DEBUG_LOG("All clients have submitted input, moving on");
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
        DEBUG_LOG("Got all client inputs, broadcasting game state");
        base->packet.calc_checksum();

        for (c = player_list; c; c = c->next)
        {
            if (c->has_joined())
            {
                c->set_wait_input(1);
                game_sock->write(/* server_game_state */ base->packet.data,
                                 base->packet.packet_size() + base->packet.packet_prefix_size(), c->data_address);
                DEBUG_LOG("Sent state to client %d", c->client_id);
            }
        }

        base->input_state = INPUT_PROCESSING; // tell engine to start processing
        game_sock
            ->read_unselectable(); // don't listen to this socket until we are prepared to read next tick's game data
        waiting_server_input = 1;
        DEBUG_LOG("Processing state complete");
    }
}

// Add server's own input to the game state
void game_server::add_engine_input()
{
    DEBUG_LOG("Adding server engine input for tick %d", base->current_tick);
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
        DEBUG_LOG("Adding input from client %d, size %d bytes", c->client_id, size);
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
        }
        else
        {
            DEBUG_LOG("Client %d marked for reload start", c->client_id);
            c->set_need_reload_start_ok(1);
        }

        uint8_t ack = SRVCMD_RELOAD_START_OK;
        if (c->comm->write(/* server_command */ &ack, 1) != 1)
        {
            DEBUG_LOG("Failed to acknowledge reload to client %d", c->client_id);
            c->set_delete_me(1);
            return 0;
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
    DEBUG_LOG("Processing network activity");
    int ret = 0;

    // Handle incoming game data
    if ((base->input_state == INPUT_COLLECTING || base->input_state == INPUT_RELOAD) && game_sock->ready_to_read())
    {
        DEBUG_LOG("Game data available");
        net_packet tmp;
        net_packet *use = &tmp;
        net_address *from;
        int bytes_received = game_sock->read(/* client_input_data */ use->data, PACKET_MAX_SIZE, &from);

        if (from && bytes_received)
        {
            DEBUG_LOG("Received %d bytes of game data", bytes_received);

            if (bytes_received == use->packet_size() + use->packet_prefix_size())
            {
                uint16_t rec_crc = use->get_checksum();
                if (rec_crc == use->calc_checksum())
                {
                    player_client *f = player_list, *found = NULL;
                    for (; !found && f; f = f->next)
                    {
                        if (f->has_joined() && from->equal(f->data_address))
                            found = f;
                    }

                    if (found)
                    {
                        if (base->current_tick == use->tick_received())
                        {
                            DEBUG_LOG("Valid game data from client %d for current tick (%d)", found->client_id,
                                      base->current_tick);

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

    return 1;
}

int game_server::input_missing()
{
    DEBUG_LOG("Server requesting input resend");
    return 1;
}

// Handle level reload completion
int game_server::end_reload(int disconnect)
{
    DEBUG_LOG("Ending reload (disconnect=%d)", disconnect);
    player_client *c = player_list;
    prot->select(0);

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
    prot->select(0);

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
        uint8_t len;
        int16_t nkills = lstl(main_net_cfg->kills);

        if (sock->read(/* client_name_length */ &len, 1) != 1 || sock->read(/* client_name_data */ name, len) != len ||
            sock->read(/* client_port */ &cport, 2) != 2 || sock->write(/* server_port */ &our_port, 2) != 2 ||
            sock->write(/* server_kills */ &nkills, 2) != 2)
        {
            DEBUG_LOG("Failed to exchange connection data");
            return 0;
        }

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
        strcpy(join_array[client_id].name, name);
        player_list = new player_client(f, sock, from, player_list);

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