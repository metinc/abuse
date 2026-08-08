/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *  Copyright (c) 2024 Andrej Pancik
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, by Sam Hocevar, or Andrej Pancik.
 *
 *  SDL3_net transport for Abuse's legacy networking interface.
 */
#pragma once

#include "isllist.h"
#include "sock.h"

#include <SDL3_net/SDL_net.h>

#include <cstdint>
#include <vector>

class tcpip_protocol;
class sdl_net_socket;
extern tcpip_protocol tcpip;

class ip_address final : public net_address
{
  public:
    NET_Address *address;
    uint16_t port;

    // retain_address is false when ownership of an existing reference is transferred.
    ip_address(NET_Address *address, uint16_t port, bool retain_address = true);
    ~ip_address() override;

    protocol protocol_type() const override;
    int equal(const net_address *who) const override;
    int set_port(int port) override;
    int get_port() override;
    void print() override;
    net_address *copy() override;
    void store_string(char *st, int st_length) override;
};

class tcpip_protocol final : public net_protocol
{
  protected:
    struct RequestItem
    {
        ip_address *addr;
        char name[256];
    };

    using p_request = isllist<RequestItem *>::iterator;
    isllist<RequestItem *> servers, returned;

    struct DiscoveryResponder
    {
        net_socket *socket;
        ip_address *local_target;
        bool broadcasts;
    };

    net_socket *notifier{nullptr};
    char notify_data[512]{};
    int notify_len{0};
    std::vector<DiscoveryResponder> responders;

    bool initialized{false};
    std::vector<sdl_net_socket *> sockets;

    bool ensure_initialized();
    int handle_notification() const;
    int handle_responder();
    int handle_responder(net_socket *responder);
    bool create_responders();
    void add_socket(sdl_net_socket *socket);
    void remove_socket(sdl_net_socket *socket);

    friend class sdl_net_socket;

  public:
    tcpip_protocol() = default;
    ~tcpip_protocol() override;

    net_address *get_local_address() override;
    net_address *get_node_address(char const *&server_host, int def_port, int force_port) override;
    net_socket *connect_to_server(net_address *addr,
                                  net_socket::socket_type sock_type = net_socket::SOCKET_SECURE) override;
    net_socket *create_listen_socket(int port, net_socket::socket_type sock_type) override;

    int installed() override;
    const char *name() override
    {
        return "SDL3_net TCP/IP";
    }

    void cleanup() override;
    int select(bool block) override;

    net_socket *start_notify(int port, void *data, int len) override;
    void end_notify() override;
    net_address *find_address(int port, char *name) override;
    void reset_find_list() override;
};

class sdl_net_socket : public net_socket
{
  protected:
    bool failed{false};
    bool read_selected{false};
    bool write_selected{false};
    int socket_id;

    virtual void *waitable() const = 0;

  public:
    sdl_net_socket();
    ~sdl_net_socket() override;

    int error() override
    {
        return failed;
    }
    int get_fd() override
    {
        // SDL3_net intentionally hides native descriptors. The file service only
        // needs a stable per-socket identifier, so expose a synthetic one.
        return socket_id;
    }
    int ready_to_write() override
    {
        return !failed;
    }
    void read_selectable() override
    {
        read_selected = true;
    }
    void read_unselectable() override
    {
        read_selected = false;
    }
    void write_selectable() override
    {
        write_selected = true;
    }
    void write_unselectable() override
    {
        write_selected = false;
    }

    bool monitors_read() const
    {
        return read_selected;
    }
    bool monitors_write() const
    {
        return write_selected;
    }
    void *waitable_object() const
    {
        return waitable();
    }
};

class sdl_stream_socket final : public sdl_net_socket
{
    NET_StreamSocket *socket;

    void *waitable() const override
    {
        return socket;
    }

  public:
    explicit sdl_stream_socket(NET_StreamSocket *socket) : socket(socket)
    {
    }
    ~sdl_stream_socket() override;

    int ready_to_read() override;
    int write(void const *buf, int size, net_address *addr = nullptr) override;
    int read(void *buf, int size, net_address **addr = nullptr) override;
};

class sdl_server_socket final : public sdl_net_socket
{
    NET_Server *server;

    void *waitable() const override
    {
        return server;
    }

  public:
    explicit sdl_server_socket(NET_Server *server) : server(server)
    {
    }
    ~sdl_server_socket() override;

    int ready_to_read() override;
    int write(void const *, int, net_address * = nullptr) override
    {
        return -1;
    }
    int read(void *, int, net_address ** = nullptr) override
    {
        return -1;
    }
    net_socket *accept(net_address *&from) override;
};

class sdl_datagram_socket final : public sdl_net_socket
{
    NET_DatagramSocket *socket;
    ip_address *default_peer;

    void *waitable() const override
    {
        return socket;
    }

  public:
    sdl_datagram_socket(uint16_t port, bool allow_broadcast, ip_address *default_peer = nullptr,
                        NET_Address *local_address = nullptr);
    ~sdl_datagram_socket() override;

    bool valid() const
    {
        return socket != nullptr;
    }
    int ready_to_read() override;
    int write(void const *buf, int size, net_address *addr = nullptr) override;
    int read(void *buf, int size, net_address **addr = nullptr) override;
};
