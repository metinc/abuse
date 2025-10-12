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

#include "common.h"

#include "demo.h"
#include "specs.h"
#include "level.h"
#include "game.h"
#include "dev.h"
#include "timing.h"
#include "net/netface.h"

#if HAVE_NETWORK
#include "fileman.h"
#endif
#include "net/sock.h"
#include "net/ghandler.h"
#include "net/gserver.h"
#include "net/gclient.h"
#include "netcfg.h"

/*
 
   This file is a combination of :
     src/net/unix/unixnfc.c
     src/net/unix/netdrv.c
     src/net/unix/undrv.c
 
     netdrv & undrv compile to a stand-alone program with talk with unixnfc
 via a FIFO in /tmp, using a RPC-like scheme.  This versions runs inside
 of a abuse and therefore is a bit simpler.
 */

// Global state variables
base_memory_struct *base; // Points to shared memory address
base_memory_struct local_base; // Local memory structure for game state
net_address *net_server = NULL; // Server address for clients
net_protocol *prot = NULL; // Active network protocol
net_socket *comm_sock = NULL; // Socket for general communication
net_socket *game_sock = NULL; // Socket for game-specific data
game_handler *game_face = NULL; // Interface for game networking
extern char lsf[256]; // Level file name
int local_client_number = 0; // Client ID (0 = server)
join_struct *join_array = NULL; // Array of joining clients
extern char const *get_login();
extern void set_login(char const *name);

int net_init(int argc, char **argv)
{
    DEBUG_LOG("Initializing network system");
    int i, x, db_level = 0;
    base = &local_base;

    local_client_number = 0;

    if (!main_net_cfg)
    {
        DEBUG_LOG("Creating new network configuration");
        main_net_cfg = new net_configuration;
    }

    // Parse command line arguments
    for (i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "-nonet"))
        {
            DEBUG_LOG("Network disabled via -nonet flag");
            printf("Net: Disabled (-nonet)\n");
            return 0;
        }
        else if (!strcmp(argv[i], "-port"))
        {
            if (i == argc - 1 || !sscanf(argv[i + 1], "%d", &x) || x < 1 || x > 0x7fff)
            {
                DEBUG_LOG("Invalid port specified: %s", (i < argc - 1) ? argv[i + 1] : "missing value");
                fprintf(stderr, "Net: Bad value following -port, use 1..32000\n");
                return 0;
            }
            else
            {
                DEBUG_LOG("Setting network port to %d", x);
                main_net_cfg->port = x;
            }
        }
        else if (!strcmp(argv[i], "-net") && i < argc - 1)
        {
            i++;
            DEBUG_LOG("Setting server name to %s", argv[i]);
            strncpy(main_net_cfg->server_name, argv[i], sizeof(main_net_cfg->server_name) - 1);
            main_net_cfg->server_name[sizeof(main_net_cfg->server_name) - 1] = '\0';
            main_net_cfg->state = net_configuration::CLIENT;
        }
        else if (!strcmp(argv[i], "-ndb"))
        {
            if (i == argc - 1 || !sscanf(argv[i + 1], "%d", &x) || x < 1 || x > 3)
            {
                DEBUG_LOG("Invalid debug level specified");
                fprintf(stderr, "Net: Bad value following -ndb, use 1..3\n");
                return 0;
            }
            else
            {
                DEBUG_LOG("Setting debug level to %d", x);
                db_level = x;
            }
        }
        else if (!strcmp(argv[i], "-server"))
        {
            DEBUG_LOG("Setting state to SERVER");
            main_net_cfg->state = net_configuration::SERVER;
        }
        else if (!strcmp(argv[i], "-min_players"))
        {
            i++;
            int x = atoi(argv[i]);
            if (x >= 1 && x <= 8)
            {
                DEBUG_LOG("Setting minimum players to %d", x);
                main_net_cfg->min_players = x;
            }
            else
            {
                DEBUG_LOG("Invalid minimum players value: %d", x);
                fprintf(stderr, "bad value for min_players use 1..8\n");
            }
        }
    }

    // Find available network protocols
    DEBUG_LOG("Searching for usable network protocols");
    net_protocol *n = net_protocol::first, *usable = NULL;
    int total_usable = 0;
    for (; n; n = n->next)
    {
        DEBUG_LOG("Protocol %s: %s", n->name(), n->installed() ? "Installed" : "Not installed");
        fprintf(stderr, "Protocol %s : ", n->installed() ? "Installed" : "Not_installed");
        fprintf(stderr, "%s\n", n->name());
        if (n->installed())
        {
            total_usable++;
            usable = n;
        }
    }

    if (!usable)
    {
        DEBUG_LOG("No network protocols available");
        fprintf(stderr, "Net: No network protocols installed\n");
        return 0;
    }

    DEBUG_LOG("Selected protocol: %s", usable->name());
    prot = usable;
    prot->set_debug_printing((net_protocol::debug_type)db_level);

    if (main_net_cfg->state == net_configuration::SERVER)
    {
        DEBUG_LOG("Initializing as server");
        set_login(main_net_cfg->name);
    }

    comm_sock = game_sock = NULL;
    if (main_net_cfg->state == net_configuration::CLIENT)
    {
        DEBUG_LOG("Initializing as client, looking for server: %s", main_net_cfg->server_name);
        printf("Attempting to locate server %s, please wait\n", main_net_cfg->server_name);
        char const *sn = main_net_cfg->server_name;
        net_server = prot->get_node_address(sn, DEFAULT_COMM_PORT, 0);
        if (!net_server)
        {
            DEBUG_LOG("Failed to locate server");
            printf(symbol_str("unable_locate"));
            exit(EXIT_SUCCESS);
        }
        DEBUG_LOG("Server located successfully");
        printf("Server located!  Please wait while data loads....\n");
    }

#if HAVE_NETWORK
    DEBUG_LOG("Initializing file manager");
    fman = new file_manager(argc, argv, prot);
#endif
    DEBUG_LOG("Creating game handler");
    game_face = new game_handler;
    join_array = (join_struct *)malloc(sizeof(join_struct) * MAX_JOINERS);

    // Initialize base memory structure
    DEBUG_LOG("Initializing base memory structure");
    base->join_list = NULL;
    base->mem_lock = 0;
    base->calc_crcs = 0;
    base->get_lsf = 0;
    base->wait_reload = 0;
    base->need_reload = 0;
    base->input_state = INPUT_COLLECTING;
    base->current_tick = 0;
    base->packet.packet_reset();

    DEBUG_LOG("Network initialization complete");
    return 1;
}

int net_start() // is the game starting up off the net? (i.e. -net hostname)
{
    DEBUG_LOG("Checking if game is starting from network: %d",
              (main_net_cfg && main_net_cfg->state == net_configuration::CLIENT));
    return main_net_cfg && main_net_cfg->state == net_configuration::CLIENT;
}

int kill_net()
{
    DEBUG_LOG("Shutting down network");
    if (game_face)
    {
        DEBUG_LOG("Deleting game handler");
        delete game_face;
    }
    game_face = NULL;

    if (join_array)
    {
        DEBUG_LOG("Freeing join array");
        free(join_array);
    }
    join_array = NULL;

    if (game_sock)
    {
        DEBUG_LOG("Closing game socket");
        delete game_sock;
        game_sock = NULL;
    }

    if (comm_sock)
    {
        DEBUG_LOG("Closing communication socket");
        delete comm_sock;
        comm_sock = NULL;
    }

#if HAVE_NETWORK
    DEBUG_LOG("Cleaning up file manager");
    delete fman;
    fman = NULL;
#endif

    if (net_server)
    {
        DEBUG_LOG("Cleaning up server address");
        delete net_server;
        net_server = NULL;
    }

    if (prot)
    {
        DEBUG_LOG("Cleaning up protocol");
        prot->cleanup();
        prot = NULL;
        return 1;
    }
    return 0;
}

void net_uninit()
{
    DEBUG_LOG("Uninitializing network");
    kill_net();
}

#if HAVE_NETWORK
int NF_set_file_server(net_address *addr)
{
    DEBUG_LOG("Setting file server address");
    if (prot)
    {
        fman->set_default_fs(addr);
        DEBUG_LOG("Connecting to file server");
        net_socket *sock = prot->connect_to_server(addr, net_socket::SOCKET_SECURE);

        if (!sock)
        {
            DEBUG_LOG("Failed to connect to file server");
            printf("set_file_server::connect failed\n");
            return 0;
        }

        uint8_t cmd = CLIENT_CRC_WAITER;
        if ((sock->write(/* client_type_request */ &cmd, 1) != 1 && printf("set_file_server::write failed\n")) ||
            (sock->read(/* server_crc_response */ &cmd, 1) != 1 && printf("set_file_server::read failed\n")))
        {
            DEBUG_LOG("Failed to exchange CRC data with server");
            delete sock;
            return 0;
        }
        delete sock;
        DEBUG_LOG("File server setup complete");
        return cmd;
    }
    else
        return 0;
}

int NF_set_file_server(char const *name)
{
    DEBUG_LOG("Setting file server by name: %s", name);
    if (prot)
    {
        net_address *addr = prot->get_node_address(name, DEFAULT_COMM_PORT, 0);
        if (addr)
        {
            int ret = NF_set_file_server(addr);
            delete addr;
            return ret;
        }
        else
        {
            DEBUG_LOG("Failed to resolve server address");
            return 0;
        }
    }
    return 0;
}

int NF_open_file(char const *filename, char const *mode)
{
    // DEBUG_LOG("Opening network file: %s mode: %s", filename, mode);
    if (prot)
        return fman->rf_open_file(filename, mode);
    return -2;
}

long NF_close(int fd)
{
    DEBUG_LOG("Closing network file handle: %d", fd);
    if (prot)
        return fman->rf_close(fd);
    return 0;
}

long NF_read(int fd, void *buf, long size)
{
    DEBUG_LOG("Reading %ld bytes from network file handle: %d", size, fd);
    if (prot)
        return fman->rf_read(fd, buf, size);
    return 0;
}

long NF_filelength(int fd)
{
    DEBUG_LOG("Getting length of network file handle: %d", fd);
    if (prot)
        return fman->rf_file_size(fd);
    return 0;
}

long NF_seek(int fd, long offset)
{
    DEBUG_LOG("Seeking network file handle: %d to offset: %ld", fd, offset);
    if (prot)
        return fman->rf_seek(fd, offset);
    return 0;
}

long NF_tell(int fd)
{
    DEBUG_LOG("Getting position of network file handle: %d", fd);
    if (prot)
        return fman->rf_tell(fd);
    return 0;
}
#endif

void service_net_request()
{
#if HAVE_NETWORK
    if (prot)
    {
        if (prot->select(false))
        {
            // DEBUG_LOG("Network activity detected");
            if (comm_sock && comm_sock->ready_to_read())
            {
                DEBUG_LOG("New connection incoming");
                net_address *addr;
                net_socket *new_sock = comm_sock->accept(addr);

                if (new_sock)
                {
                    uint8_t client_type;
                    if (new_sock->read(/* client_type_request */ &client_type, 1) != 1)
                    {
                        DEBUG_LOG("Failed to read client type");
                        delete addr;
                        delete new_sock;
                    }
                    else
                    {
                        DEBUG_LOG("Processing new connection, client type: %d", client_type);
                        switch (client_type)
                        {
                        case CLIENT_NFS: {
                            DEBUG_LOG("NFS client connected");
                            delete addr;
                            fman->add_nfs_client(new_sock);
                        }
                        break;
                        case CLIENT_CRC_WAITER: {
                            DEBUG_LOG("CRC waiter client connected");
                            crc_manager.write_crc_file(NET_CRC_FILENAME);
                            client_type = 1;
                            new_sock->write(/* server_crc_response */ &client_type, 1);
                            delete new_sock;
                            delete addr;
                        }
                        break;
                        case CLIENT_LSF_WAITER: {
                            DEBUG_LOG("LSF waiter client connected");
                            uint8_t len = strlen(lsf) + 1;
                            new_sock->write(/* server_lsf_length */ &len, 1);
                            new_sock->write(/* server_lsf_data */ lsf, len);
                            delete new_sock;
                            delete addr;
                        }
                        break;
                        default: {
                            DEBUG_LOG("Game client connected, type: %d", client_type);
                            if (game_face->add_client(client_type, new_sock, addr) == 0)
                            {
                                DEBUG_LOG("Failed to add game client");
                                delete addr;
                                delete new_sock;
                            }
                        }
                        break;
                        }
                    }
                }
            }

            if (!game_face->process_net())
            {
                DEBUG_LOG("Game face processing failed, creating new handler");
                delete game_face;
                game_face = new game_handler;
            }
            DEBUG_LOG("Processing network file manager");
            fman->process_net();
        }
    }
#endif
}

int get_remote_lsf(net_address *addr, char *filename)
{
    DEBUG_LOG("Requesting LSF from remote address");
    if (prot)
    {
        DEBUG_LOG("Connecting to server for LSF");
        net_socket *sock = prot->connect_to_server(addr, net_socket::SOCKET_SECURE);
        if (!sock)
        {
            DEBUG_LOG("Failed to connect to server");
            return 0;
        }

        uint8_t ctype = CLIENT_LSF_WAITER;
        uint8_t len;

        if (sock->write(/* client_type_request */ &ctype, 1) != 1 || sock->read(/* server_lsf_length */ &len, 1) != 1 ||
            len == 0 || sock->read(/* server_lsf_data */ filename, len) != len)
        {
            DEBUG_LOG("Failed to exchange LSF data");
            delete sock;
            return 0;
        }

        DEBUG_LOG("Successfully retrieved LSF");
        delete sock;
        return 1;
    }
    return 0;
}

int request_server_entry()
{
    DEBUG_LOG("Requesting server entry");
    if (prot && main_net_cfg)
    {
        if (!net_server)
        {
            DEBUG_LOG("No server address available");
            return 0;
        }

        if (game_sock)
        {
            DEBUG_LOG("Cleaning up existing game socket");
            delete game_sock;
        }
        printf("Joining game in progress, hang on....\n");

        DEBUG_LOG("Creating game socket on port %d", main_net_cfg->port + 2);
        game_sock = prot->create_listen_socket(main_net_cfg->port + 2, net_socket::SOCKET_FAST);
        if (!game_sock)
        {
            DEBUG_LOG("Failed to create game socket");
            if (comm_sock)
                delete comm_sock;
            comm_sock = NULL;
            prot = NULL;
            return 0;
        }
        game_sock->read_selectable();

        DEBUG_LOG("Connecting to server");
        net_socket *sock = prot->connect_to_server(net_server, net_socket::SOCKET_SECURE);
        if (!sock)
        {
            DEBUG_LOG("Failed to connect to server");
            fprintf(stderr, "unable to connect to server\n");
            return 0;
        }

        uint8_t ctype = CLIENT_ABUSE;
        uint16_t port = lstl(main_net_cfg->port + 2), cnum;
        uint8_t reg;

        // Send client registration with debug ID
        DEBUG_LOG("Sending client type");
        if (sock->write(/* client_type_request */ &ctype, 1) != 1 ||
            sock->read(/* server_registration_response */ &reg, 1) != 1)
        {
            DEBUG_LOG("Failed to exchange registration data");
            delete sock;
            return 0;
        }

        if (reg == SRVCMD_TOO_MANY)
        {
            DEBUG_LOG("Server full - max players reached");
            fprintf(stderr, "%s", symbol_str("max_players"));
            delete sock;
            return 0;
        }

        if (reg != SRVCMD_REGISTRATION_OK)
        {
            DEBUG_LOG("Server not registered");
            fprintf(stderr, "%s", symbol_str("server_not_reg"));
            delete sock;
            return 0;
        }
        else
        {
            DEBUG_LOG("Server registered successfully");
        }

        char uname[256];
        if (get_login())
            strcpy(uname, get_login());
        else
            strcpy(uname, "unknown");
        uint8_t len = strlen(uname) + 1;
        uint16_t our_port = lstl(main_net_cfg->port + 2), cport;
        int16_t nkills;

        DEBUG_LOG("Sending client info - username: %s", uname);
        if (sock->write(/* client_name_length */ &len, 1) != 1 ||
            sock->write(/* client_name_data */ uname, len) != len || sock->write(/* client_port */ &our_port, 2) != 2 ||
            sock->read(/* server_port */ &port, 2) != 2 || sock->read(/* server_kills */ &nkills, 2) != 2 ||
            sock->read(/* server_game_mode */ &ctype, 1) != 1 || sock->read(/* server_client_id */ &cnum, 2) != 2 ||
            cnum == 0)
        {
            DEBUG_LOG("Failed to exchange client information");
            delete sock;
            return 0;
        }

        uint8_t gmode = ctype; // reuse tmp var

        nkills = lstl(nkills);
        port = lstl(port);
        uint16_t client_id = lstl(cnum);

        DEBUG_LOG("Client registration complete - assigned number: %d", client_id);
        main_net_cfg->kills = nkills;
        main_net_cfg->game_mode =
            (gmode == net_configuration::COOP) ? net_configuration::COOP : net_configuration::DEATHMATCH;
        net_address *addr = net_server->copy();
        addr->set_port(port);

        DEBUG_LOG("Creating game client handler");
        delete game_face;
        game_face = new game_client(sock, addr);
        delete addr;

        local_client_number = client_id;
        return client_id;
    }
    return 0;
}

int reload_start()
{
    DEBUG_LOG("Starting reload process");
    if (prot)
        return game_face->start_reload();
    return 0;
}

int reload_end()
{
    DEBUG_LOG("Ending reload process");
    if (prot)
        return game_face->end_reload();
    return 0;
}

void net_reload()
{
    DEBUG_LOG("Beginning network reload");
    if (prot)
    {
        if (net_server) // Client-side reload
        {
            DEBUG_LOG("Client-side reload");
            if (current_level)
                delete current_level;
            bFILE *fp;

            if (!reload_start())
            {
                DEBUG_LOG("Reload start failed");
                return;
            }

            do
            {
                fp = open_file(NET_STARTFILE, "rb");
                if (fp->open_failure())
                {
                    delete fp;
                    fp = NULL;
                }
            } while (!fp);

            DEBUG_LOG("Loading level from network start file");
            spec_directory sd(fp);
            current_level = new level(&sd, fp, NET_STARTFILE);
            delete fp;

            base->current_tick = (current_level->tick_counter() & 0xff);

            reload_end();
        }
        else if (current_level) // Server-side reload
        {
            DEBUG_LOG("Server-side reload");
            join_struct *join_list = base->join_list;

            // Process all joined players
            while (join_list)
            {
                DEBUG_LOG("Processing joined player");
                view *f = player_list;
                for (; f && f->next; f = f->next)
                    ;

                int i, st = 0;
                for (i = 0; i < total_objects; i++)
                    if (!strcmp(object_names[i], "START"))
                        st = i;

                game_object *o = create(current_start_type, 0, 0);
                game_object *start = current_level->get_random_start(320, NULL);
                if (start)
                {
                    o->x = start->x;
                    o->y = start->y;
                }
                else
                {
                    o->x = 100;
                    o->y = 100;
                }

                DEBUG_LOG("Creating new view for player %d", join_list->client_id);
                f->next = new view(o, NULL, join_list->client_id);
                strcpy(f->next->name, join_list->name);
                o->set_controller(f->next);
                f->next->set_tint(f->next->player_number);
                if (start)
                    current_level->add_object_after(o, start);
                else
                    current_level->add_object(o);

                view *v = f->next;
                v->m_aa = ivec2(5);
                v->m_bb = ivec2(319, 199) - ivec2(5);
                join_list = join_list->next;
            }

            DEBUG_LOG("Saving level state");
            base->join_list = NULL;
            current_level->save(NET_STARTFILE, 1);
            base->mem_lock = 0;

            DEBUG_LOG("Creating resync window");
            Jwindow *j = wm->CreateWindow(
                ivec2(0, yres / 2), ivec2(-1),
                new info_field(0, 0, 0, symbol_str("resync"),
                               new button(0, wm->font()->Size().y + 5, ID_NET_DISCONNECT, symbol_str("slack"), NULL)),
                symbol_str("hold!"));

            wm->flush_screen();

            if (!reload_start())
            {
                DEBUG_LOG("Reload start failed");
                return;
            }

            base->input_state = INPUT_RELOAD;

            DEBUG_LOG("Waiting for clients to reload");
            do
            {
                service_net_request();
                if (wm->IsPending())
                {
                    Event ev;
                    do
                    {
                        wm->get_event(ev);
                        if (ev.type == EV_MESSAGE && ev.message.id == ID_NET_DISCONNECT)
                        {
                            DEBUG_LOG("Disconnect requested during reload");
                            game_face->end_reload(1);
                            base->input_state = INPUT_PROCESSING;
                        }
                    } while (wm->IsPending());

                    wm->flush_screen();
                }
            } while (!reload_end());

            DEBUG_LOG("Reload complete, cleaning up");
            wm->close_window(j);
            unlink(NET_STARTFILE);

            the_game->reset_keymap();

            base->input_state = INPUT_COLLECTING;
        }
    }
}

int client_number()
{
    return local_client_number;
}

void send_local_request()
{
    if (prot)
    {
        DEBUG_LOG("Sending local request, tick: %d", current_level ? current_level->tick_counter() & 0xff : 0);
        if (current_level)
            base->current_tick = (current_level->tick_counter() & 0xff);
        game_face->add_engine_input();
    }
    else
        base->input_state = INPUT_PROCESSING;
}

void kill_slackers()
{
    DEBUG_LOG("Killing slack clients");
    if (prot)
    {
        if (!game_face->kill_slackers())
        {
            DEBUG_LOG("Recreating game handler after slack cleanup");
            delete game_face;
            game_face = new game_handler();
        }
    }
}

int get_inputs_from_server(unsigned char *buf)
{
    if (prot && base->input_state != INPUT_PROCESSING)
    {
        time_marker start;
        int total_retry = 0;
        Jwindow *abort = NULL;

        while (base->input_state != INPUT_PROCESSING)
        {
            if (!prot)
            {
                DEBUG_LOG("Protocol lost while waiting for input");
                base->input_state = INPUT_PROCESSING;
                return 1;
            }
            service_net_request();

            time_marker now;
            if (now.diff_time(&start) > 0.05)
            {
                DEBUG_LOG("Missed packet, requesting resend");
                if (prot->debug_level(net_protocol::DB_IMPORTANT_EVENT))
                    fprintf(stderr, "(missed packet)\n");

                game_face->input_missing();
                start.get_time();

                total_retry++;
                if (total_retry == 12000)
                {
                    DEBUG_LOG("Connection appears dead, showing abort dialog");
                    abort = wm->CreateWindow(ivec2(0, yres / 2), ivec2(-1, wm->font()->Size().y * 4),
                                             new info_field(0, 0, 0, symbol_str("waiting"),
                                                            new button(0, wm->font()->Size().y + 5, ID_NET_DISCONNECT,
                                                                       symbol_str("slack"), NULL)),
                                             symbol_str("Error"));
                    wm->flush_screen();
                }
            }
            if (abort)
            {
                if (wm->IsPending())
                {
                    Event ev;
                    do
                    {
                        wm->get_event(ev);
                        if (ev.type == EV_MESSAGE && ev.message.id == ID_NET_DISCONNECT)
                        {
                            DEBUG_LOG("User requested disconnect");
                            kill_slackers();
                            base->input_state = INPUT_PROCESSING;
                        }
                    } while (wm->IsPending());

                    wm->flush_screen();
                }
            }
        }

        if (abort)
        {
            DEBUG_LOG("Cleaning up abort dialog");
            wm->close_window(abort);
            the_game->reset_keymap();
        }
    }

    DEBUG_LOG("Processing received input packet");
    memcpy(base->last_packet.data, base->packet.data, base->packet.packet_size() + base->packet.packet_prefix_size());

    int size = base->packet.packet_size();
    memcpy(buf, base->packet.packet_data(), size);

    base->packet.packet_reset();
    base->mem_lock = 0;

    return size;
}

int become_server(char *name)
{
    DEBUG_LOG("Attempting to become server: %s", name);
    if (prot && main_net_cfg)
    {
        if (comm_sock)
            delete comm_sock;

        DEBUG_LOG("Creating communication socket on port %d", main_net_cfg->port);
        comm_sock = prot->create_listen_socket(main_net_cfg->port, net_socket::SOCKET_SECURE);
        if (!comm_sock)
        {
            DEBUG_LOG("Failed to create communication socket");
            prot = NULL;
            return 0;
        }
        comm_sock->read_selectable();

        DEBUG_LOG("Starting server notification on port 0x9090");
        prot->start_notify(0x9090, name, strlen(name));

        if (game_sock)
            delete game_sock;

        DEBUG_LOG("Creating game socket on port %d", main_net_cfg->port + 1);
        game_sock = prot->create_listen_socket(main_net_cfg->port + 1, net_socket::SOCKET_FAST);
        if (!game_sock)
        {
            DEBUG_LOG("Failed to create game socket");
            if (comm_sock)
                delete comm_sock;
            comm_sock = NULL;
            prot = NULL;
            return 0;
        }
        game_sock->read_selectable();

        DEBUG_LOG("Creating game server handler");
        delete game_face;
        game_face = new game_server;
        local_client_number = 0;
        return 1;
    }
    return 0;
}

void read_new_views()
{
    DEBUG_LOG("Reading new views");
}

void wait_min_players()
{
    DEBUG_LOG("Waiting for minimum players");
    if (game_face)
        game_face->game_start_wait();
}