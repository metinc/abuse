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

#include "netcfg.h"
#include "gclient.h"
#include "netface.h"
#include "timing.h"

extern base_memory_struct *base;
extern net_socket *comm_sock, *game_sock;
extern net_protocol *prot;
extern char lsf[256];
extern int start_running;

// Handles incoming commands from the server like resend requests
int game_client::process_server_command()
{
    uint8_t cmd;
    DEBUG_LOG("Processing server command");

    if (client_sock->read(/* server_command */ &cmd, 1) != 1)
    {
        DEBUG_LOG("Failed to read command byte from server");
        return 0;
    }

    DEBUG_LOG("Received command %d from server", cmd);

    switch (cmd)
    {

    case SRVCMD_RELOAD_START_OK: {
        // I think this happens if the client joined the server that was already in an active game
        DEBUG_LOG("Server confirmed reload start, moving on.");
        return 1;
    }
    break;

    case SRVCMD_REQUEST_RESEND: {
        uint8_t tick;
        DEBUG_LOG("Server requested packet resend");

        if (client_sock->read(/* server_resend_request_tick */ &tick, 1) != 1)
        {
            DEBUG_LOG("Failed to read tick number for resend request");
            return 0;
        }

        fprintf(stderr, "request for resend tick %d (game cur=%d, pack=%d, last=%d)\n", tick, base->current_tick,
                base->packet.tick_received(), base->last_packet.tick_received());

        // Only resend if we have the requested tick and haven't sent new input
        if (tick == base->packet.tick_received() && !wait_local_input)
        {
            DEBUG_LOG("Resending packet %d to server", base->packet.tick_received());
            net_packet *pack = &base->packet;
            game_sock->write(/* client_input_data */ pack->data, pack->packet_size() + pack->packet_prefix_size(),
                             server_data_port);

            // Add artificial delay after resend
            {
                time_marker now, start;
                while (now.diff_time(&start) < 3.0)
                    now.get_time();
            }
        }
        else if (tick == base->last_packet.tick_received())
        {
            DEBUG_LOG("Resending last packet to server");
        }
        else
        {
            DEBUG_LOG("Skipping resend - tick mismatch or new input pending");
        }
        return 1;
    }
    break;

    default: {
        DEBUG_LOG("Unknown command from server: %d", cmd);
        return 0;
    }
    }
    return 0;
}

// Main networking processing loop for client
int game_client::process_net()
{
    if (client_sock->error())
    {
        DEBUG_LOG("Client socket error detected");
        return 0;
    }

    // Check for incoming game state data
    if (game_sock->ready_to_read())
    {
        net_packet tmp;

        int bytes_received = game_sock->read(/* server_game_state */ tmp.data, PACKET_MAX_SIZE);

        if (bytes_received == tmp.packet_size() + tmp.packet_prefix_size())
        {
            uint16_t rec_crc = tmp.get_checksum();
            if (rec_crc == tmp.calc_checksum())
            {
                if (base->current_tick == tmp.tick_received())
                {
                    base->packet = tmp;
                    wait_local_input = 1;
                    base->input_state = INPUT_PROCESSING;
                }
                else
                {
                    DEBUG_LOG("Received stale packet - got tick %d, expected %d", tmp.tick_received(),
                              base->current_tick);
                }
            }
            else
            {
                DEBUG_LOG("Packet checksum verification failed");
                fprintf(stderr, "received packet with bad checksum\n");
            }
        }
        else
        {
            DEBUG_LOG("Incomplete packet received");
            fprintf(stderr, "incomplete packet, read %d, should be %d\n", bytes_received,
                    tmp.packet_size() + tmp.packet_prefix_size());
        }
    }

    // Check for server commands
    if (client_sock->ready_to_read())
    {
        DEBUG_LOG("Server command available");
        if (!process_server_command())
        {
            DEBUG_LOG("Server command processing failed - reverting to single player");
            main_net_cfg->state = net_configuration::RESTART_SINGLE;
            start_running = 0;
            strcpy(lsf, "abuse.lsp");

            wait_local_input = 1;
            base->input_state = INPUT_PROCESSING;
            return 0;
        }
    }

    return 1;
}

// Constructor - initializes client networking state
game_client::game_client(net_socket *client_sock, net_address *server_addr) : client_sock(client_sock)
{
    DEBUG_LOG("Creating new game client");
    server_data_port = server_addr->copy();
    client_sock->read_selectable();
    wait_local_input = 1;
}

// Called when input from server is missing/late
int game_client::input_missing()
{
    DEBUG_LOG("Handling missing input");

    if (prot->debug_level(net_protocol::DB_IMPORTANT_EVENT))
        fprintf(stderr, "(resending %d)\n", base->packet.tick_received());

    uint8_t cmd = CLCMD_REQUEST_RESEND;
    uint8_t tick = base->packet.tick_received();

    // Send resend request to server
    if (client_sock->write(/* client_command */ &cmd, 1) != 1 ||
        client_sock->write(/* client_resend_request_tick */ &tick, 1) != 1)
    {
        DEBUG_LOG("Failed to send resend request");
        return 0;
    }

    DEBUG_LOG("Sent resend request for tick %d", tick);
    return 1;
}

// Add local input to be sent to server
void game_client::add_engine_input()
{
    if (base->input_state == INPUT_RELOAD)
    {
        DEBUG_LOG("Skipping input collection - reload in progress");
        return;
    }

    net_packet *pack = &base->packet;
    base->input_state = INPUT_COLLECTING;
    wait_local_input = 0;
    pack->set_tick_received(base->current_tick);
    pack->calc_checksum();

    game_sock->write(/* client_input_data */ pack->data, pack->packet_size() + pack->packet_prefix_size(),
                     server_data_port);
}

// Notify server that level reload is complete
int game_client::end_reload(int disconnect)
{
    DEBUG_LOG("Sending end reload notification");

    uint8_t cmd = CLCMD_RELOAD_END;
    if (client_sock->write(/* client_command */ &cmd, 1) != 1)
    {
        DEBUG_LOG("Failed to send reload end notification");
        return 0;
    }
    return 1;
}

// Begin level reload process
int game_client::start_reload()
{
    DEBUG_LOG("Starting reload process");

    uint8_t cmd = CLCMD_RELOAD_START;
    if (client_sock->write(/* client_command */ &cmd, 1) != 1)
    {
        DEBUG_LOG("Failed to send reload start request");
        return 0;
    }

    if (client_sock->read(/* server_reload_ack */ &cmd, 1) != 1)
    {
        DEBUG_LOG("Failed to receive reload acknowledgement");
        return 0;
    }

    DEBUG_LOG("Reload process initiated successfully");
    return 1;
}

int kill_net();

// Handle disconnection of inactive clients
int game_client::kill_slackers()
{
    DEBUG_LOG("Killing inactive clients");

    if (base->input_state == INPUT_COLLECTING)
    {
        DEBUG_LOG("Changing input state to processing");
        base->input_state = INPUT_PROCESSING;
    }

    kill_net();
    return 0; // Tell driver to delete us and replace with local client
}

// Clean disconnection from server
int game_client::quit()
{
    DEBUG_LOG("Initiating clean disconnect from server");

    uint8_t cmd = CLCMD_UNJOIN;
    if (client_sock->write(/* client_command */ &cmd, 1) != 1)
    {
        DEBUG_LOG("Failed to send unjoin command");
        return 0;
    }

    if (client_sock->read(/* server_disconnect_ack */ &cmd, 1) != 1)
    {
        DEBUG_LOG("Failed to receive unjoin acknowledgement");
        return 0;
    }

    DEBUG_LOG("Clean disconnect completed");
    return 1;
}

// Destructor - cleanup network resources
game_client::~game_client()
{
    DEBUG_LOG("Destroying game client");
    delete client_sock;
    delete server_data_port;
}