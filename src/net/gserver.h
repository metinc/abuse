#ifndef __GSERVER_HPP_
#define __GSERVER_HPP_

#define MAX_JOINERS 32 // maximum clients that can join at the same time

#include "sock.h"
#include "ghandler.h"

#include <cstdint>
#include <string>
#include <vector>

class game_server : public game_handler
{
    class player_client
    {
        unsigned char flags;
        enum
        {
            Has_joined = 1,
            Wait_reload = 2,
            Wait_input = 4,
            Need_reload_start_ok = 8,
            Delete_me = 16
        };
        int get_flag(int flag)
        {
            return flags & flag;
        }
        void set_flag(int flag, int x)
        {
            if (x)
                flags |= flag;
            else
                flags &= ~flag;
        }

      public:
        int has_joined()
        {
            return get_flag(Has_joined);
        }
        void set_has_joined(int x)
        {
            set_flag(Has_joined, x);
        }

        int wait_input()
        {
            return get_flag(Wait_input);
        }
        void set_wait_input(int x)
        {
            set_flag(Wait_input, x);
        }

        int wait_reload()
        {
            return get_flag(Wait_reload);
        }
        void set_wait_reload(int x)
        {
            set_flag(Wait_reload, x);
        }

        int delete_me()
        {
            return get_flag(Delete_me);
        }
        void set_delete_me(int x)
        {
            set_flag(Delete_me, x);
        }

        int need_reload_start_ok()
        {
            return get_flag(Need_reload_start_ok);
        }
        void set_need_reload_start_ok(int x)
        {
            set_flag(Need_reload_start_ok, x);
        }

        int client_id;
        std::string name;
        std::uint64_t last_packet_ticks;
        net_socket *comm;
        net_address *data_address;
        player_client *next;
        player_client(int client_id, char const *name, net_socket *comm, net_address *data_address,
                      player_client *next);
        ~player_client();
    };

    player_client *player_list;
    int waiting_server_input, reload_state;
    bool lobby_open;

    void add_client_input(char *buf, int size, player_client *c);
    void check_collection_complete();
    void check_reload_wait();
    void send_lobby_status();
    void send_lobby_start();
    void restart_single_player();
    int process_client_command(player_client *c);
    int isa_client(int client_id);

  public:
    struct client_status
    {
        int client_id;
        std::string name;
        std::uint64_t milliseconds_since_packet;
    };

    virtual void game_start_wait();
    int total_players();
    std::vector<client_status> client_statuses();
    bool kick_client(int client_id);
    int process_net();
    void add_engine_input();
    int input_missing();
    virtual int start_reload();
    virtual int end_reload(int disconnect = 0);
    virtual int add_client(int type, net_socket *sock, net_address *from);
    virtual int kill_slackers();
    virtual int quit();
    game_server();
    ~game_server();
};

#endif
