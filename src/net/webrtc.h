/*
 * WebRTC transport for Abuse room-code multiplayer.
 *
 * The public interface deliberately mirrors the legacy TCP/IP protocol. Each
 * reliable legacy connection becomes a reliable DataChannel, while the game
 * datagrams share one unordered, non-retransmitting DataChannel per peer.
 */
#pragma once

#include "sock.h"

#include <memory>
#include <string>

class webrtc_protocol final : public net_protocol
{
  public:
    struct impl;

  private:
    std::unique_ptr<impl> data;

  public:
    webrtc_protocol();
    ~webrtc_protocol() override;

    void configure_host(std::string signaling_url);
    void configure_client(std::string signaling_url, std::string room_code);
    bool requested() const;
    std::string room_code() const;

    net_address *get_local_address() override;
    net_address *get_node_address(char const *&server_host, int def_port, int force_port) override;
    net_socket *connect_to_server(net_address *addr,
                                  net_socket::socket_type sock_type = net_socket::SOCKET_SECURE) override;
    net_socket *create_listen_socket(int port, net_socket::socket_type sock_type) override;
    int installed() override;
    char const *name() override;
    int select(bool block) override;
    void cleanup() override;

    net_socket *start_notify(int, void *, int) override
    {
        return nullptr;
    }
    void end_notify() override
    {
    }
};

extern webrtc_protocol webrtc;
