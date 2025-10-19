/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *  Copyright (c) 2024 Andrej Pancik
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, by Sam Hocevar, or Andrej Pancik.
 */
#pragma once

// Platform-specific includes
#ifdef WIN32
#include <WinSock2.h> // Must come before Windows.h
#include <Windows.h>
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#endif

#include "sock.h"
#include "isllist.h"
#include <cstring>

// Forward declarations
class tcpip_protocol;
extern tcpip_protocol tcpip;

/**
 * Represents an IP address and port combination
 */
class ip_address final : public net_address
{
  public:
    sockaddr_in addr{};

    ip_address() = default;
    explicit ip_address(const sockaddr_in *addr);

    // net_address interface implementation
    protocol protocol_type() const override;
    int equal(const net_address *who) const override;
    int set_port(int port) override;
    void print() override;
    int get_port() override;
    net_address *copy() override;
    void store_string(char *st, int st_length) override;
};

/**
 * TCP/IP protocol implementation
 */
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

    // Notification handling
    net_socket *notifier{nullptr};
    char notify_data[512]{};
    int notify_len{0};

    net_socket *responder{nullptr};
    ip_address *bcast{nullptr};

    int handle_notification() const;
    int handle_responder();

  public:
    fd_set master_set, master_write_set, read_set, exception_set, write_set;

    tcpip_protocol();
    ~tcpip_protocol() override
    {
        tcpip_protocol::cleanup();
    }

    // Protocol operations
    net_address *get_local_address() override;
    net_address *get_node_address(char const *&server_host, int def_port, int force_port) override;
    net_socket *connect_to_server(net_address *addr,
                                  net_socket::socket_type sock_type = net_socket::SOCKET_SECURE) override;
    net_socket *create_listen_socket(int port, net_socket::socket_type sock_type) override;

    // Protocol information
    int installed() override
    {
        return 1;
    }
    const char *name() override
    {
        return "UNIX generic TCPIP";
    }

    // State management
    void cleanup() override;
    int select(bool block) override;

    // Notification methods
    net_socket *start_notify(int port, void *data, int len) override;
    void end_notify() override;

    // Server discovery
    net_address *find_address(int port, char *name) override;
    void reset_find_list() override;
};

/**
 * Base class for UNIX file descriptors
 */
class unix_fd : public net_socket
{
  protected:
    int fd;

  public:
    explicit unix_fd(const int fd) : fd(fd)
    {
    }
    ~unix_fd() override;

    // Socket operations
    int error() override
    {
        return FD_ISSET(fd, &tcpip.exception_set);
    }
    int ready_to_read() override
    {
        return FD_ISSET(fd, &tcpip.read_set);
    }
    int ready_to_write() override;
    int write(void const *buf, int size, net_address *addr) override;
    int read(void *buf, int size, net_address **addr) override;
    int get_fd() override
    {
        return fd;
    }

    // Socket state management
    void read_selectable() override
    {
        FD_SET(fd, &tcpip.master_set);
    }
    void read_unselectable() override
    {
        FD_CLR(fd, &tcpip.master_set);
    }
    void write_selectable() override
    {
        FD_SET(fd, &tcpip.master_write_set);
    }
    void write_unselectable() override
    {
        FD_CLR(fd, &tcpip.master_write_set);
    }
    void broadcastable() const;
};

/**
 * TCP socket implementation
 */
class tcp_socket final : public unix_fd
{
    bool listening{false};

  public:
    explicit tcp_socket(const int fd) : unix_fd(fd)
    {
    }

    int listen(int port) override;
    net_socket *accept(net_address *&addr) override;
};

/**
 * UDP socket implementation
 */
class udp_socket final : public unix_fd
{
  public:
    explicit udp_socket(const int fd) : unix_fd(fd)
    {
    }

    int read(void *buf, int size, net_address **addr) override;
    int write(void const *buf, int size, net_address *addr = nullptr) override;
    int listen(int port) override;
};