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

#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_timer.h>

#include "common.h"

#include "cop.h"
#include "specs.h"
#include "level.h"
#include "game.h"
#include "dev.h"
#include "timing.h"
#include "net/netface.h"

#include "fileman.h"
#include "net/sock.h"
#include "net/ghandler.h"
#include "net/gserver.h"
#include "net/gclient.h"
#include "netcfg.h"
#include "net/webrtc.h"
#include "sdlport/setup.h"

#include <string>
#include <vector>

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
extern Settings settings;

namespace
{
constexpr int ID_NET_KICK_PLAYER_FIRST = 0x7e00;
constexpr int ID_NET_KICK_PLAYER_END = ID_NET_KICK_PLAYER_FIRST + MAX_JOINERS;
constexpr int ID_NET_COPY_ROOM_CODE = 0x7f01;

struct player_status_row
{
    int client_id;
    info_field *field;
};

struct tab_player_status_row
{
    int player_number;
    info_field *field;
};

class tab_action_button : public button
{
  public:
    using button::button;

    void handle_event(Event &event, image *screen, InputManager *manager) override
    {
        if (event.type != EV_MOUSE_BUTTON)
        {
            button::handle_event(event, screen, manager);
            return;
        }

        if (event.mouse_button & LEFT_BUTTON)
        {
            if (!left_action_sent)
            {
                left_action_sent = true;
                button::handle_event(event, screen, manager);
                wm->PushMessage(id, this);
            }
            return;
        }

        if (left_action_sent)
        {
            // Cancel button's deferred release action: this overlay dispatches
            // left clicks immediately so a refresh cannot swallow the release.
            left_action_sent = false;
            button::draw(0, screen);
            button::draw(1, screen);
            return;
        }

        button::handle_event(event, screen, manager);
    }

    void draw(int active, image *screen) override
    {
        if (!active)
            left_action_sent = false;
        button::draw(active, screen);
    }

  private:
    bool left_action_sent = false;
};

Jwindow *tab_player_status_window = nullptr;
std::vector<tab_player_status_row> tab_player_status_rows;
std::uint64_t tab_player_status_refresh = 0;

std::string player_status_text(char const *name, std::uint64_t milliseconds_since_packet)
{
    char text[256];
    const std::uint64_t display_ms = std::min<std::uint64_t>(milliseconds_since_packet, 9999999999ULL);
    snprintf(text, sizeof(text), "%s  %10llu ms", name, static_cast<unsigned long long>(display_ms));
    return text;
}

std::string player_status_text(game_server::client_status const &status)
{
    return player_status_text(status.name.c_str(), status.milliseconds_since_packet);
}

Jwindow *create_player_status_window(game_server *server, std::vector<player_status_row> &rows, char const *message,
                                     char const *title)
{
    const std::vector<game_server::client_status> statuses = server->client_statuses();
    const int row_height = wm->font()->Size().y + 7;
    const int kick_x = wm->font()->Size().x * 34;
    ifield *fields = new info_field(0, 0, ID_NULL, message, nullptr);
    fields = new info_field(0, row_height, ID_NULL, symbol_str("last_packet_age"), fields);

    rows.clear();
    int y = row_height * 2;
    const std::string host_text = player_status_text(symbol_str("host_player"), 0);
    fields = new info_field(0, y + 3, ID_NULL, host_text.c_str(), fields);
    y += row_height;
    for (game_server::client_status const &status : statuses)
    {
        const std::string text = player_status_text(status);
        info_field *field = new info_field(0, y + 3, ID_NULL, text.c_str(), fields);
        fields = new button(kick_x, y, ID_NET_KICK_PLAYER_FIRST + status.client_id, symbol_str("kick_player"), field);
        rows.push_back({status.client_id, field});
        y += row_height;
    }

    Jwindow *window = wm->CreateWindow(ivec2(0), ivec2(-1), fields, title);
    wm->move_window(window, std::max(0, (xres - window->m_size.x) / 2),
                    std::max(0, (yres - window->m_size.y) / 2));
    return window;
}

Jwindow *create_client_status_window(game_client *client, info_field *&status_field, char const *message,
                                     char const *title)
{
    const int row_height = wm->font()->Size().y + 7;
    ifield *fields = new info_field(0, 0, ID_NULL, message, nullptr);
    fields = new info_field(0, row_height, ID_NULL, symbol_str("last_packet_age"), fields);
    const std::string text = player_status_text(symbol_str("host_player"), client->milliseconds_since_last_packet());
    status_field = new info_field(0, row_height * 2 + 3, ID_NULL, text.c_str(), fields);

    Jwindow *window = wm->CreateWindow(ivec2(0), ivec2(-1), status_field, title);
    wm->move_window(window, std::max(0, (xres - window->m_size.x) / 2),
                    std::max(0, (yres - window->m_size.y) / 2));
    return window;
}

void refresh_client_status_window(Jwindow *window, game_client *client, info_field *status_field)
{
    const std::string text = player_status_text(symbol_str("host_player"), client->milliseconds_since_last_packet());
    status_field->change_text(text.c_str());
    window->redraw();
    wm->flush_screen();
}

bool refresh_player_status_window(Jwindow *window, game_server *server,
                                  std::vector<player_status_row> const &rows)
{
    const std::vector<game_server::client_status> statuses = server->client_statuses();
    if (statuses.size() != rows.size())
        return false;
    for (player_status_row const &row : rows)
    {
        bool found = false;
        for (game_server::client_status const &status : statuses)
        {
            if (status.client_id == row.client_id)
            {
                const std::string text = player_status_text(status);
                row.field->change_text(text.c_str());
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }
    window->redraw();
    wm->flush_screen();
    return true;
}

std::vector<view *> sorted_score_players()
{
    std::vector<view *> players;
    for (view *player = player_list; player; player = player->next)
        players.push_back(player);
    std::sort(players.begin(), players.end(), [](view const *left, view const *right) {
        if (left->kills != right->kills)
            return left->kills > right->kills;
        return left->player_number < right->player_number;
    });
    return players;
}

std::string tab_player_status_text(view *player, game_server *server, game_client *client)
{
    char packet_age[32] = "-";
    if (player->local_player())
        snprintf(packet_age, sizeof(packet_age), "%7d ms", 0);
    else if (server)
    {
        for (game_server::client_status const &status : server->client_statuses())
            if (status.client_id == player->player_number)
            {
                const std::uint64_t display_ms = std::min<std::uint64_t>(status.milliseconds_since_packet, 9999999);
                snprintf(packet_age, sizeof(packet_age), "%7llu ms",
                         static_cast<unsigned long long>(display_ms));
                break;
            }
    }
    else if (client && player->player_number == 0)
    {
        const std::uint64_t display_ms =
            std::min<std::uint64_t>(client->milliseconds_since_last_packet(), 9999999);
        snprintf(packet_age, sizeof(packet_age), "%7llu ms", static_cast<unsigned long long>(display_ms));
    }

    char text[256];
    snprintf(text, sizeof(text), "%-18s %5ld %10s", player->name, static_cast<long>(player->kills), packet_age);
    return text;
}

bool refresh_tab_player_status_window(game_server *server, game_client *client)
{
    const std::vector<view *> players = sorted_score_players();
    if (players.size() != tab_player_status_rows.size())
        return false;

    for (std::size_t i = 0; i < players.size(); ++i)
    {
        if (players[i]->player_number != tab_player_status_rows[i].player_number)
            return false;
        const std::string text = tab_player_status_text(players[i], server, client);
        tab_player_status_rows[i].field->change_text(text.c_str());
    }

    tab_player_status_window->redraw();
    wm->flush_screen();
    return true;
}

void close_tab_player_status_window()
{
    if (tab_player_status_window)
        wm->close_window(tab_player_status_window);
    tab_player_status_window = nullptr;
    tab_player_status_rows.clear();
    tab_player_status_refresh = 0;
}

void create_tab_player_status_window()
{
    game_server *server = dynamic_cast<game_server *>(game_face);
    game_client *client = dynamic_cast<game_client *>(game_face);
    if (!server && !client)
        return;

    const int row_height = wm->font()->Size().y + 7;
    const int kick_x = wm->font()->Size().x * 36;
    ifield *fields = nullptr;
    int y = 0;

    const bool show_room = main_net_cfg && main_net_cfg->online && main_net_cfg->room_code[0];
    if (show_room)
    {
        char room[128];
        snprintf(room, sizeof(room), "%s: %s", symbol_str("room_code"),
                 main_net_cfg->streamer_mode ? "******" : main_net_cfg->room_code);
        fields = new info_field(0, y + 3, ID_NULL, room, fields);
        y += row_height;
        fields = new tab_action_button(0, y, ID_NET_COPY_ROOM_CODE, symbol_str("copy_room_code"), fields);
        y += row_height;
    }

    fields = new info_field(0, y + 3, ID_NULL, symbol_str("player_status_columns"), fields);
    y += row_height;

    const std::vector<game_server::client_status> statuses = server ? server->client_statuses()
                                                                    : std::vector<game_server::client_status>();
    for (view *player : sorted_score_players())
    {
        const std::string text = tab_player_status_text(player, server, client);
        info_field *field = new info_field(0, y + 3, ID_NULL, text.c_str(), fields);
        fields = field;

        const bool can_kick = server && !player->local_player() &&
                              std::any_of(statuses.begin(), statuses.end(), [player](auto const &status) {
                                  return status.client_id == player->player_number;
                              });
        if (can_kick)
            fields = new tab_action_button(kick_x, y, ID_NET_KICK_PLAYER_FIRST + player->player_number,
                                           symbol_str("kick_player"), fields);

        tab_player_status_rows.push_back({player->player_number, field});
        y += row_height;
    }

    tab_player_status_window =
        wm->CreateWindow(ivec2(0), ivec2(-1), fields, symbol_str("player_status"));
    wm->move_window(tab_player_status_window, std::max(0, (xres - tab_player_status_window->m_size.x) / 2),
                    std::max(0, (yres - tab_player_status_window->m_size.y) / 2));

    if (tab_player_status_window)
    {
        tab_player_status_refresh = SDL_GetTicks();
        wm->flush_screen();
    }
}
}

bool handle_net_player_status_event(Event const &event)
{
    if (!tab_player_status_window)
        return false;

    if (event.type == EV_CLOSE_WINDOW && event.window == tab_player_status_window)
        return true;

    if (event.type == EV_MESSAGE && event.message.id == ID_NET_COPY_ROOM_CODE)
    {
        if (main_net_cfg && main_net_cfg->online && main_net_cfg->room_code[0] &&
            !SDL_SetClipboardText(main_net_cfg->room_code))
            DEBUG_LOG("Unable to copy room code: %s", SDL_GetError());
        return true;
    }

    game_server *server = dynamic_cast<game_server *>(game_face);
    if (event.type != EV_MESSAGE || !server || event.message.id < ID_NET_KICK_PLAYER_FIRST ||
        event.message.id >= ID_NET_KICK_PLAYER_END)
    {
        // WindowManager has already delivered these events to the status
        // window. Do not pass its mouse input on to the local player as well.
        return event.window == tab_player_status_window &&
               (event.type == EV_MOUSE_BUTTON || event.type == EV_MOUSE_MOVE);
    }

    const int client_id = event.message.id - ID_NET_KICK_PLAYER_FIRST;
    if (server->kick_client(client_id))
    {
        close_tab_player_status_window();
        create_tab_player_status_window();
    }
    return true;
}

void update_net_player_status(bool show)
{
    game_server *server = dynamic_cast<game_server *>(game_face);
    game_client *client = dynamic_cast<game_client *>(game_face);
    if (!show || (!server && !client))
    {
        close_tab_player_status_window();
        return;
    }

    if (!tab_player_status_window)
    {
        create_tab_player_status_window();
        return;
    }

    const std::uint64_t now = SDL_GetTicks();
    if (now - tab_player_status_refresh < 100)
        return;
    tab_player_status_refresh = now;

    if (!refresh_tab_player_status_window(server, client))
    {
        close_tab_player_status_window();
        create_tab_player_status_window();
    }
}

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
    constexpr char default_signaling_url[] = "wss://abusecoop.com";
    enum class online_mode
    {
        none,
        host,
        client
    };
    online_mode online = online_mode::none;
    std::string signaling_url = ABUSE_SIGNALING_URL;
    if (signaling_url.empty())
        signaling_url = default_signaling_url;
    std::string online_room;
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
            DEBUG_LOG("Setting server host to %s", argv[i]);
            strncpy(main_net_cfg->server_host, argv[i], sizeof(main_net_cfg->server_host) - 1);
            main_net_cfg->server_host[sizeof(main_net_cfg->server_host) - 1] = '\0';
            main_net_cfg->online = false;
            main_net_cfg->room_code[0] = '\0';
            main_net_cfg->state = net_configuration::CLIENT;
        }
        else if (!strcmp(argv[i], "-signal-server") && i < argc - 1)
        {
            signaling_url = argv[++i];
        }
        else if (!strcmp(argv[i], "-online-join") && i < argc - 1)
        {
            online_room = argv[++i];
            if (main_net_cfg->join_failed)
                continue;
            online = online_mode::client;
            strncpy(main_net_cfg->server_host, online_room.c_str(), sizeof(main_net_cfg->server_host) - 1);
            main_net_cfg->server_host[sizeof(main_net_cfg->server_host) - 1] = '\0';
            strncpy(main_net_cfg->room_code, online_room.c_str(), sizeof(main_net_cfg->room_code) - 1);
            main_net_cfg->room_code[sizeof(main_net_cfg->room_code) - 1] = '\0';
            main_net_cfg->online = true;
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
            main_net_cfg->online = false;
            main_net_cfg->room_code[0] = '\0';
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

    if (online == online_mode::none && main_net_cfg->online)
    {
        if (main_net_cfg->state == net_configuration::SERVER)
            online = online_mode::host;
        else if (main_net_cfg->state == net_configuration::CLIENT)
        {
            online = online_mode::client;
            online_room = main_net_cfg->room_code;
        }
    }

    if (online != online_mode::none)
    {
        if (signaling_url.empty() || (signaling_url.compare(0, 5, "ws://") && signaling_url.compare(0, 6, "wss://")))
        {
            fprintf(stderr, "Net: Invalid signaling URL; expected ws:// or wss://\n");
            return 0;
        }
        if (online == online_mode::host)
            webrtc.configure_host(signaling_url);
        else
            webrtc.configure_client(signaling_url, online_room);
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
            if (!webrtc.requested() || n == &webrtc)
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

    if (main_net_cfg->state == net_configuration::SERVER || main_net_cfg->state == net_configuration::CLIENT)
    {
        DEBUG_LOG("Using configured player name: %s", main_net_cfg->name);
        set_login(main_net_cfg->name);
    }

    comm_sock = game_sock = NULL;
    if (main_net_cfg->state == net_configuration::CLIENT)
    {
        DEBUG_LOG("Initializing as client, looking for server: %s", main_net_cfg->server_host);
        printf("Attempting to locate server %s, please wait\n", main_net_cfg->server_host);
        char const *sn = main_net_cfg->server_host;
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

    DEBUG_LOG("Initializing file manager");
    fman = new file_manager(argc, argv, prot);
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

bool net_game_active()
{
    return dynamic_cast<game_server *>(game_face) || dynamic_cast<game_client *>(game_face);
}

bool net_input_ready()
{
    return !prot || !base || base->input_state == INPUT_PROCESSING;
}

void request_net_input_resend()
{
    if (prot && game_face)
        game_face->input_missing();
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

    DEBUG_LOG("Cleaning up file manager");
    delete fman;
    fman = NULL;

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
    close_tab_player_status_window();
    kill_net();
}

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

void service_net_request()
{
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
            fman->process_net();
        }
    }
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
        main_net_cfg->waiting_for_host = false;
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

        // SDL3_net normally allows UDP sockets to share an address. That made
        // several clients on one machine all bind to the same port, so game
        // packets could be delivered to the wrong process. Use the first free
        // port in the client range and advertise the selected port below.
        int client_port = main_net_cfg->port + 2;
        const int last_client_port = std::min(65535, client_port + MAX_JOINERS - 1);
        for (; !game_sock && client_port <= last_client_port; ++client_port)
        {
            DEBUG_LOG("Creating game socket on port %d", client_port);
            game_sock = prot->create_listen_socket(client_port, net_socket::SOCKET_FAST);
        }
        if (!game_sock)
        {
            DEBUG_LOG("Failed to create game socket");
            if (comm_sock)
                delete comm_sock;
            comm_sock = NULL;
            prot = NULL;
            return 0;
        }
        --client_port;
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
        uint16_t port = lstl(client_port), cnum;
        uint8_t reg, lobby, connected_players, max_players;

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
            main_net_cfg->server_full = true;
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
        uint8_t lower_skin = static_cast<uint8_t>(settings.player_lower_skin);
        uint8_t upper_skin = static_cast<uint8_t>(settings.player_upper_skin);
        uint16_t our_port = lstl(client_port), cport;
        int16_t nkills;

        DEBUG_LOG("Sending client info - username: %s", uname);
        if (sock->write(/* client_name_length */ &len, 1) != 1 ||
            sock->write(/* client_name_data */ uname, len) != len ||
            sock->write(/* client_lower_skin */ &lower_skin, 1) != 1 ||
            sock->write(/* client_upper_skin */ &upper_skin, 1) != 1 ||
            sock->write(/* client_port */ &our_port, 2) != 2 || sock->read(/* server_port */ &port, 2) != 2 ||
            sock->read(/* server_kills */ &nkills, 2) != 2 || sock->read(/* server_game_mode */ &ctype, 1) != 1 ||
            sock->read(/* server_lobby_state */ &lobby, 1) != 1 ||
            sock->read(/* server_lobby_players */ &connected_players, 1) != 1 ||
            sock->read(/* server_max_players */ &max_players, 1) != 1 ||
            sock->read(/* server_client_id */ &cnum, 2) != 2 || cnum == 0)
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
        main_net_cfg->waiting_for_host = lobby != 0;
        main_net_cfg->lobby_players = connected_players;
        main_net_cfg->max_players = max_players;
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

void wait_for_server_lobby()
{
    int displayed_players = -1;
    Jwindow *status = NULL;

    while (main_net_cfg && main_net_cfg->waiting_for_host && !main_net_cfg->restart_state() &&
           !application_quit_requested())
    {
        if (displayed_players != main_net_cfg->lobby_players)
        {
            if (status)
                wm->close_window(status);

            char message[512];
            snprintf(message, sizeof(message), symbol_str("client_lobby_players"), main_net_cfg->lobby_players);
            const bool cooperative = main_net_cfg->game_mode == net_configuration::COOP;
            char instructions[512];
            if (cooperative)
                snprintf(instructions, sizeof(instructions), "%s", symbol_str("coop_lobby_instructions"));
            else
                snprintf(instructions, sizeof(instructions), symbol_str("deathmatch_lobby_instructions"),
                         main_net_cfg->kills);
            const size_t message_length = strlen(message);
            snprintf(message + message_length, sizeof(message) - message_length, "\n\n%s", instructions);
            status = wm->CreateWindow(ivec2(0), ivec2(-1), new info_field(0, 0, ID_NULL, message, NULL),
                                      symbol_str(cooperative ? "coop_lobby_title" : "deathmatch_lobby_title"));
            wm->move_window(status, std::max(0, (xres - status->m_size.x) / 2),
                            std::max(0, (yres - status->m_size.y) / 2));
            displayed_players = main_net_cfg->lobby_players;
        }

        service_net_request();
        while (wm->IsPending())
        {
            Event event;
            wm->get_event(event);
        }
        wm->flush_screen();
        SDL_Delay(1);
    }

    if (status)
    {
        wm->close_window(status);
        wm->flush_screen();
    }
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
            bool use_coop_checkpoint = false;
            ivec2 coop_checkpoint;
            if (main_net_cfg && main_net_cfg->game_mode == net_configuration::COOP)
            {
                for (view *checkpoint_view = player_list; checkpoint_view; checkpoint_view = checkpoint_view->next)
                {
                    game_object *player = checkpoint_view->m_focus;
                    if (player && figures[player->otype]->tv > coop_checkpoint_y &&
                        player->lvars[coop_checkpoint_active])
                    {
                        coop_checkpoint = ivec2(player->lvars[coop_checkpoint_x], player->lvars[coop_checkpoint_y]);
                        use_coop_checkpoint = true;
                        break;
                    }
                }
            }

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
                game_object *start = NULL;
                if (use_coop_checkpoint)
                {
                    // The checkpoint determines where the joining player spawns,
                    // but the START object still determines its render order.
                    // Falling back to add_object() would prepend the player and
                    // draw it behind consoles and other level objects.
                    start = current_level->find_type(st, join_list->client_id);
                    o->x = coop_checkpoint.x;
                    o->y = coop_checkpoint.y;
                    o->lvars[coop_checkpoint_active] = 1;
                    o->lvars[coop_checkpoint_x] = coop_checkpoint.x;
                    o->lvars[coop_checkpoint_y] = coop_checkpoint.y;
                }
                else if ((start = current_level->get_random_start(320, NULL)))
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
                copy_player_name(f->next->name, sizeof(f->next->name), join_list->name);
                o->set_controller(f->next);
                f->next->set_tint(join_list->lower_skin);
                f->next->set_upper_tint(join_list->upper_skin);
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
            game_server *host_server = dynamic_cast<game_server *>(game_face);
            std::vector<player_status_row> player_rows;
            Jwindow *j;
            if (host_server)
                j = create_player_status_window(host_server, player_rows, symbol_str("resync"), symbol_str("hold!"));
            else
                j = wm->CreateWindow(ivec2(0, yres / 2), ivec2(-1),
                                     new info_field(0, 0, 0, symbol_str("resync"), NULL), symbol_str("hold!"));

            wm->flush_screen();

            if (!reload_start())
            {
                DEBUG_LOG("Reload start failed");
                wm->close_window(j);
                return;
            }

            base->input_state = INPUT_RELOAD;

            DEBUG_LOG("Waiting for clients to reload");
            do
            {
                service_net_request();
                if (host_server && !refresh_player_status_window(j, host_server, player_rows))
                {
                    wm->close_window(j);
                    j = create_player_status_window(host_server, player_rows, symbol_str("resync"),
                                                    symbol_str("hold!"));
                }
                if (wm->IsPending())
                {
                    Event ev;
                    do
                    {
                        wm->get_event(ev);
                        if (ev.type == EV_MESSAGE && host_server && ev.message.id >= ID_NET_KICK_PLAYER_FIRST &&
                            ev.message.id < ID_NET_KICK_PLAYER_END)
                        {
                            const int client_id = ev.message.id - ID_NET_KICK_PLAYER_FIRST;
                            if (host_server->kick_client(client_id))
                            {
                                wm->close_window(j);
                                j = create_player_status_window(host_server, player_rows, symbol_str("resync"),
                                                                symbol_str("hold!"));
                            }
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
        game_server *host_server = dynamic_cast<game_server *>(game_face);
        game_client *network_client = dynamic_cast<game_client *>(game_face);
        std::vector<player_status_row> player_rows;
        info_field *client_status_field = nullptr;

        // One lockstep tick normally costs a round trip. Give it a reasonable
        // loss-detection window before adding reliable resend traffic.
        constexpr double resend_timeout_sec = 0.25;
        constexpr int disconnect_prompt_retries = 20;
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
            if (now.diff_time(&start) > resend_timeout_sec)
            {
                DEBUG_LOG("Missed packet, requesting resend");
                if (prot->debug_level(net_protocol::DB_IMPORTANT_EVENT))
                    fprintf(stderr, "(missed packet)\n");

                game_face->input_missing();
                start.get_time();

                total_retry++;
                if (total_retry == disconnect_prompt_retries)
                {
                    DEBUG_LOG("Connection appears dead, showing abort dialog");
                    if (host_server)
                    {
                        abort = create_player_status_window(host_server, player_rows, symbol_str("waiting"),
                                                            symbol_str("Error"));
                    }
                    else if (network_client)
                        abort = create_client_status_window(network_client, client_status_field,
                                                            symbol_str("waiting"), symbol_str("Error"));
                    wm->flush_screen();
                }
            }
            if (abort)
            {
                if (host_server)
                {
                    if (!refresh_player_status_window(abort, host_server, player_rows))
                    {
                        wm->close_window(abort);
                        abort = create_player_status_window(host_server, player_rows, symbol_str("waiting"),
                                                            symbol_str("Error"));
                    }
                }
                else if (network_client)
                    refresh_client_status_window(abort, network_client, client_status_field);
                if (wm->IsPending())
                {
                    Event ev;
                    do
                    {
                        wm->get_event(ev);
                        if (ev.type == EV_MESSAGE && host_server && ev.message.id >= ID_NET_KICK_PLAYER_FIRST &&
                            ev.message.id < ID_NET_KICK_PLAYER_END)
                        {
                            const int client_id = ev.message.id - ID_NET_KICK_PLAYER_FIRST;
                            if (host_server->kick_client(client_id) && base->input_state != INPUT_PROCESSING)
                            {
                                wm->close_window(abort);
                                abort = create_player_status_window(host_server, player_rows, symbol_str("waiting"),
                                                                    symbol_str("Error"));
                            }
                        }
                    } while (wm->IsPending());

                    wm->flush_screen();
                }
            }

            // SDL3_net readiness checks above are deliberately non-blocking.
            // Yield here so packet loss cannot turn this lockstep wait into a
            // full-core busy loop.
            SDL_Delay(1);
        }

        if (abort)
        {
            DEBUG_LOG("Cleaning up abort dialog");
            wm->close_window(abort);
            the_game->reset_keymap();
        }
    }

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
        if (main_net_cfg->online && prot == &webrtc)
        {
            const std::string code = webrtc.room_code();
            strncpy(main_net_cfg->room_code, code.c_str(), sizeof(main_net_cfg->room_code) - 1);
            main_net_cfg->room_code[sizeof(main_net_cfg->room_code) - 1] = '\0';
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

void wait_min_players()
{
    DEBUG_LOG("Waiting for minimum players");
    if (game_face)
        game_face->game_start_wait();
}
