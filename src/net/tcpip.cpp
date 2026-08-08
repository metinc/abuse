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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common.h"
#include "tcpip.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace
{
int next_socket_id = 1;

bool socket_has_input(void *socket)
{
    if (!socket)
        return false;

    void *sockets[] = {socket};
    return NET_WaitUntilInputAvailable(sockets, 1, 0) > 0;
}

bool parse_port(const char *text, uint16_t &port)
{
    if (!text || !*text)
        return false;

    char *end = nullptr;
    errno = 0;
    const long value = std::strtol(text, &end, 10);
    if (errno || end == text || *end || value < 1 || value > UINT16_MAX)
        return false;

    port = static_cast<uint16_t>(value);
    return true;
}
} // namespace

ip_address::ip_address(NET_Address *address, const uint16_t port, const bool retain_address)
    : address(retain_address && address ? NET_RefAddress(address) : address), port(port)
{
}

ip_address::~ip_address()
{
    if (address)
        NET_UnrefAddress(address);
}

net_address::protocol ip_address::protocol_type() const
{
    return IP;
}

int ip_address::equal(const net_address *who) const
{
    if (!who || who->protocol_type() != IP)
        return 0;
    const auto *other = static_cast<const ip_address *>(who);
    return address && other->address && NET_CompareAddresses(address, other->address) == 0;
}

int ip_address::set_port(const int new_port)
{
    if (new_port < 0 || new_port > UINT16_MAX)
        return 0;
    port = static_cast<uint16_t>(new_port);
    return 1;
}

int ip_address::get_port()
{
    return port;
}

void ip_address::print()
{
#ifdef TCPIP_DEBUG
    const char *text = NET_GetAddressString(address);
    DEBUG_LOG("%s:%u", text ? text : "<unresolved>", static_cast<unsigned>(port));
#endif
}

net_address *ip_address::copy()
{
    return new ip_address(address, port);
}

void ip_address::store_string(char *st, const int st_length)
{
    if (!st || st_length <= 0)
        return;

    const char *text = NET_GetAddressString(address);
    if (!text)
        text = "";

    // Brackets keep an IPv6 address distinct from its optional port.
    if (std::strchr(text, ':'))
        std::snprintf(st, st_length, "[%s]:%u", text, static_cast<unsigned>(port));
    else
        std::snprintf(st, st_length, "%s:%u", text, static_cast<unsigned>(port));
}

sdl_net_socket::sdl_net_socket() : socket_id(next_socket_id++)
{
    tcpip.add_socket(this);
}

sdl_net_socket::~sdl_net_socket()
{
    tcpip.remove_socket(this);
}

sdl_stream_socket::~sdl_stream_socket()
{
    if (socket)
    {
        // SDL3_net queues stream writes. Short-lived request sockets often
        // write their reply immediately before destruction, so give that data
        // a chance to reach the kernel instead of silently discarding it.
        if (!failed && NET_GetStreamSocketPendingWrites(socket) > 0)
            NET_WaitUntilStreamSocketDrained(socket, 1000);
        NET_DestroyStreamSocket(socket);
    }
}

int sdl_stream_socket::ready_to_read()
{
    if (failed)
        return 0;
    return socket_has_input(socket);
}

int sdl_stream_socket::write(void const *buf, const int size, net_address *addr)
{
    if (failed || !socket || size < 0 || addr)
        return -1;
    if (!NET_WriteToStreamSocket(socket, buf, size))
    {
        failed = true;
        DEBUG_LOG("SDL3_net stream write failed: %s", SDL_GetError());
        return -1;
    }
    return size;
}

int sdl_stream_socket::read(void *buf, const int size, net_address **addr)
{
    if (addr)
        *addr = nullptr;
    if (failed || !socket || size < 0)
        return -1;

    // The legacy protocol treats each read as an exact field read. TCP itself
    // does not preserve those boundaries, so collect short reads here.
    int total = 0;
    while (total < size)
    {
        const int got = NET_ReadFromStreamSocket(socket, static_cast<uint8_t *>(buf) + total, size - total);
        if (got < 0)
        {
            failed = true;
            DEBUG_LOG("SDL3_net stream read failed: %s", SDL_GetError());
            return total ? total : -1;
        }
        if (got > 0)
        {
            total += got;
            continue;
        }

        void *sockets[] = {socket};
        if (NET_WaitUntilInputAvailable(sockets, 1, -1) < 0)
        {
            failed = true;
            DEBUG_LOG("SDL3_net stream wait failed: %s", SDL_GetError());
            return total ? total : -1;
        }
    }
    return total;
}

sdl_server_socket::~sdl_server_socket()
{
    if (server)
        NET_DestroyServer(server);
}

int sdl_server_socket::ready_to_read()
{
    if (failed)
        return 0;
    return socket_has_input(server);
}

net_socket *sdl_server_socket::accept(net_address *&from)
{
    from = nullptr;
    if (failed || !server)
        return nullptr;

    NET_StreamSocket *client = nullptr;
    if (!NET_AcceptClient(server, &client))
    {
        failed = true;
        DEBUG_LOG("SDL3_net accept failed: %s", SDL_GetError());
        return nullptr;
    }
    if (!client)
        return nullptr;

    NET_Address *peer = NET_GetStreamSocketAddress(client);
    if (!peer)
    {
        NET_DestroyStreamSocket(client);
        return nullptr;
    }
    from = new ip_address(peer, 0, false);
    return new sdl_stream_socket(client);
}

sdl_datagram_socket::sdl_datagram_socket(const uint16_t port, const bool allow_broadcast, ip_address *peer,
                                         NET_Address *local_address)
    : socket(nullptr), default_peer(peer ? static_cast<ip_address *>(peer->copy()) : nullptr)
{
    SDL_PropertiesID props = 0;
    if (allow_broadcast)
    {
        props = SDL_CreateProperties();
        if (!props || !SDL_SetBooleanProperty(props, NET_PROP_DATAGRAM_SOCKET_ALLOW_BROADCAST_BOOLEAN, true))
        {
            if (props)
                SDL_DestroyProperties(props);
            failed = true;
            return;
        }
    }

    socket = NET_CreateDatagramSocket(local_address, port, props);
    if (props)
        SDL_DestroyProperties(props);
    failed = socket == nullptr;
}

sdl_datagram_socket::~sdl_datagram_socket()
{
    if (socket)
        NET_DestroyDatagramSocket(socket);
    delete default_peer;
}

int sdl_datagram_socket::ready_to_read()
{
    if (failed)
        return 0;
    return socket_has_input(socket);
}

int sdl_datagram_socket::write(void const *buf, const int size, net_address *addr)
{
    if (failed || !socket || size < 0)
        return -1;

    auto *destination = addr ? static_cast<ip_address *>(addr) : default_peer;
    NET_Address *address = destination ? destination->address : nullptr;
    const uint16_t port = destination ? destination->port : 0;
    if (!NET_SendDatagram(socket, address, port, buf, size))
    {
        failed = true;
        DEBUG_LOG("SDL3_net datagram write failed: %s", SDL_GetError());
        return -1;
    }
    return size;
}

int sdl_datagram_socket::read(void *buf, const int size, net_address **addr)
{
    if (addr)
        *addr = nullptr;
    if (failed || !socket || size < 0)
        return -1;

    NET_Datagram *datagram = nullptr;
    if (!NET_ReceiveDatagram(socket, &datagram))
    {
        failed = true;
        DEBUG_LOG("SDL3_net datagram read failed: %s", SDL_GetError());
        return -1;
    }
    if (!datagram)
        return 0;

    const int copied = std::min(size, datagram->buflen);
    std::memcpy(buf, datagram->buf, copied);
    if (addr)
        *addr = new ip_address(datagram->addr, datagram->port);
    NET_DestroyDatagram(datagram);
    return copied;
}

tcpip_protocol::~tcpip_protocol()
{
    cleanup();
    if (initialized)
        NET_Quit();
}

bool tcpip_protocol::ensure_initialized()
{
    if (!initialized)
        initialized = NET_Init();
    return initialized;
}

int tcpip_protocol::installed()
{
    if (!ensure_initialized())
        DEBUG_LOG("SDL3_net initialization failed: %s", SDL_GetError());
    return initialized;
}

void tcpip_protocol::add_socket(sdl_net_socket *socket)
{
    sockets.push_back(socket);
}

void tcpip_protocol::remove_socket(sdl_net_socket *socket)
{
    const auto found = std::find(sockets.begin(), sockets.end(), socket);
    if (found != sockets.end())
        sockets.erase(found);
}

net_address *tcpip_protocol::get_local_address()
{
    if (!ensure_initialized())
        return nullptr;

    int count = 0;
    NET_Address **addresses = NET_GetLocalAddresses(&count);
    if (!addresses || count == 0)
    {
        NET_FreeLocalAddresses(addresses);
        return nullptr;
    }

    NET_Address *address = NET_RefAddress(addresses[0]);
    NET_FreeLocalAddresses(addresses);
    return new ip_address(address, 0, false);
}

net_address *tcpip_protocol::get_node_address(char const *&server_host, int def_port, const int force_port)
{
    if (!ensure_initialized() || !server_host || !*server_host)
        return nullptr;

    const char *begin = server_host;
    const char *slash = std::strchr(begin, '/');
    const char *end = slash ? slash : begin + std::strlen(begin);
    std::string authority(begin, end);
    server_host = slash ? slash + 1 : end;

    std::string hostname = authority;
    uint16_t port = static_cast<uint16_t>(def_port);

    if (!authority.empty() && authority.front() == '[')
    {
        const size_t close = authority.find(']');
        if (close == std::string::npos)
            return nullptr;
        hostname = authority.substr(1, close - 1);
        if (!force_port && close + 1 < authority.size())
        {
            if (authority[close + 1] != ':' || !parse_port(authority.c_str() + close + 2, port))
                return nullptr;
        }
    }
    else
    {
        const size_t first_colon = authority.find(':');
        const size_t last_colon = authority.rfind(':');
        if (first_colon != std::string::npos && first_colon == last_colon)
        {
            hostname = authority.substr(0, first_colon);
            if (!force_port && !parse_port(authority.c_str() + first_colon + 1, port))
                return nullptr;
        }
    }

    NET_Address *address = NET_ResolveHostname(hostname.c_str());
    if (!address)
        return nullptr;
    if (NET_WaitUntilResolved(address, -1) != NET_SUCCESS)
    {
        DEBUG_LOG("Unable to resolve server '%s': %s", hostname.c_str(), SDL_GetError());
        NET_UnrefAddress(address);
        return nullptr;
    }
    return new ip_address(address, port, false);
}

net_socket *tcpip_protocol::connect_to_server(net_address *addr, const net_socket::socket_type sock_type)
{
    if (!ensure_initialized() || !addr || addr->protocol_type() != net_address::IP)
        return nullptr;
    auto *ip = static_cast<ip_address *>(addr);

    if (sock_type == net_socket::SOCKET_FAST)
    {
        auto *socket = new sdl_datagram_socket(0, false, ip);
        if (!socket->valid())
        {
            delete socket;
            return nullptr;
        }
        return socket;
    }

    NET_StreamSocket *socket = NET_CreateClient(ip->address, ip->port, 0);
    if (!socket)
        return nullptr;
    if (NET_WaitUntilConnected(socket, -1) != NET_SUCCESS)
    {
        DEBUG_LOG("Unable to connect: %s", SDL_GetError());
        NET_DestroyStreamSocket(socket);
        return nullptr;
    }
    return new sdl_stream_socket(socket);
}

net_socket *tcpip_protocol::create_listen_socket(const int port, const net_socket::socket_type sock_type)
{
    if (!ensure_initialized() || port < 0 || port > UINT16_MAX)
        return nullptr;

    if (sock_type == net_socket::SOCKET_FAST)
    {
        auto *socket = new sdl_datagram_socket(static_cast<uint16_t>(port), false);
        if (!socket->valid())
        {
            delete socket;
            return nullptr;
        }
        return socket;
    }

    NET_Server *server = NET_CreateServer(nullptr, static_cast<uint16_t>(port), 0);
    return server ? static_cast<net_socket *>(new sdl_server_socket(server)) : nullptr;
}

void tcpip_protocol::cleanup()
{
    end_notify();
    reset_find_list();
    for (const DiscoveryResponder &responder : responders)
    {
        delete responder.socket;
        delete responder.local_target;
    }
    responders.clear();
}

net_socket *tcpip_protocol::start_notify(const int port, void *data, const int len)
{
    end_notify();
    const int response_len = static_cast<int>(std::strlen(notify_response));
    if (len < 0 || len > static_cast<int>(sizeof(notify_data)) - response_len - 1)
        return nullptr;
    notify_len = len + response_len + 1;
    std::memcpy(notify_data, notify_response, response_len);
    notify_data[response_len] = '.';
    std::memcpy(notify_data + response_len + 1, data, len);

    notifier = create_listen_socket(port, net_socket::SOCKET_FAST);
    if (notifier)
        notifier->read_selectable();
    return notifier;
}

void tcpip_protocol::end_notify()
{
    delete notifier;
    notifier = nullptr;
    notify_len = 0;
}

int tcpip_protocol::handle_notification() const
{
    if (!notifier || !notifier->ready_to_read())
        return 0;

    char buf[513];
    net_address *sender = nullptr;
    const int len = notifier->read(buf, 512, &sender);
    if (sender && len == static_cast<int>(std::strlen(notify_signature)) &&
        std::memcmp(buf, notify_signature, len) == 0)
        notifier->write(notify_data, notify_len, sender);
    delete sender;
    return 1;
}

int tcpip_protocol::handle_responder(net_socket *responder)
{
    if (!responder || !responder->ready_to_read())
        return 0;

    char buf[513];
    net_address *sender = nullptr;
    const int len = responder->read(buf, 512, &sender);
    auto *address = static_cast<ip_address *>(sender);
    const int response_len = static_cast<int>(std::strlen(notify_response));

    if (address && len > response_len && buf[response_len] == '.' &&
        std::memcmp(buf, notify_response, response_len) == 0)
    {
        bool found = false;
        for (p_request p = servers.begin(); !found && p != servers.end(); ++p)
            found = (*p)->addr->equal(address);
        for (p_request p = returned.begin(); !found && p != returned.end(); ++p)
            found = (*p)->addr->equal(address);

        if (!found)
        {
            auto *request = new RequestItem;
            request->addr = address;
            address = nullptr;

            const int name_len = std::min(len - response_len - 1, 255);
            std::memcpy(request->name, buf + response_len + 1, name_len);
            request->name[name_len] = '\0';

            const char *text = NET_GetAddressString(request->addr->address);
            const size_t used = std::strlen(request->name);
            if (text && used < sizeof(request->name) - 4)
                std::snprintf(request->name + used, sizeof(request->name) - used, " (%s)", text);
            servers.insert(request);
        }
    }

    delete address;
    return 1;
}

int tcpip_protocol::handle_responder()
{
    int handled = 0;
    for (const DiscoveryResponder &responder : responders)
        handled += handle_responder(responder.socket);
    return handled;
}

bool tcpip_protocol::create_responders()
{
    auto *socket = new sdl_datagram_socket(0, true);
    if (socket->valid())
    {
        NET_Address *loopback = NET_ResolveHostname("127.0.0.1");
        if (loopback && NET_WaitUntilResolved(loopback, -1) != NET_SUCCESS)
        {
            NET_UnrefAddress(loopback);
            loopback = nullptr;
        }
        responders.push_back({socket, loopback ? new ip_address(loopback, 0, false) : nullptr, true});
        socket->read_selectable();
        return true;
    }
    delete socket;

    // SDL3_net's all-interface broadcast socket also joins the IPv6 ff02::1
    // multicast group. If IPv6 is unavailable on the LAN, that join can fail
    // the whole socket even though IPv4 broadcasting is usable. Fall back to
    // one socket per non-loopback IPv4 interface.
    int address_count = 0;
    NET_Address **addresses = NET_GetLocalAddresses(&address_count);
    for (int i = 0; addresses && i < address_count; ++i)
    {
        const char *text = NET_GetAddressString(addresses[i]);
        if (!text || std::strchr(text, ':') || std::strncmp(text, "127.", 4) == 0)
            continue;

        socket = new sdl_datagram_socket(0, true, nullptr, addresses[i]);
        if (!socket->valid())
        {
            delete socket;
            continue;
        }

        responders.push_back({socket, new ip_address(addresses[i], 0), true});
        socket->read_selectable();
    }
    NET_FreeLocalAddresses(addresses);

    // An offline machine can still discover a server in another local
    // process. This socket only sends a direct loopback probe.
    if (responders.empty())
    {
        NET_Address *loopback = NET_ResolveHostname("127.0.0.1");
        if (loopback && NET_WaitUntilResolved(loopback, -1) == NET_SUCCESS)
        {
            socket = new sdl_datagram_socket(0, false);
            if (socket->valid())
            {
                responders.push_back({socket, new ip_address(loopback, 0, false), false});
                loopback = nullptr;
                socket->read_selectable();
            }
            else
                delete socket;
        }
        if (loopback)
            NET_UnrefAddress(loopback);
    }

    return !responders.empty();
}

int tcpip_protocol::select(const bool block)
{
    do
    {
        handle_notification();
        handle_responder();

        int ready = 0;
        for (sdl_net_socket *socket : sockets)
        {
            if ((socket->monitors_read() && socket->ready_to_read()) ||
                (socket->monitors_write() && socket->ready_to_write()) || socket->error())
                ++ready;
        }
        if (ready || !block)
            return ready;
        SDL_Delay(1);
    } while (block);
    return 0;
}

net_address *tcpip_protocol::find_address(const int port, char *name)
{
    end_notify();
    if (responders.empty() && !create_responders())
        return nullptr;

    // Process replies to the previous probe before sending the next one.
    handle_responder();

    ip_address broadcast(nullptr, static_cast<uint16_t>(port), false);
    for (DiscoveryResponder &responder : responders)
    {
        if (responder.broadcasts)
            responder.socket->write(notify_signature, static_cast<int>(std::strlen(notify_signature)), &broadcast);
        if (responder.local_target)
        {
            responder.local_target->set_port(port);
            responder.socket->write(notify_signature, static_cast<int>(std::strlen(notify_signature)),
                                    responder.local_target);
        }
    }

    if (servers.empty())
        return nullptr;

    servers.move_next(servers.begin_prev(), returned.begin_prev());
    auto *result = (*returned.begin())->addr->copy();
    std::strncpy(name, (*returned.begin())->name, 255);
    name[255] = '\0';
    return result;
}

void tcpip_protocol::reset_find_list()
{
    for (const auto *request : servers)
    {
        delete request->addr;
        delete request;
    }
    for (const auto *request : returned)
    {
        delete request->addr;
        delete request;
    }
    servers.erase_all();
    returned.erase_all();
}
