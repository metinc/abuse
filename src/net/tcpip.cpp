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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_NETWORK

#include "file_utils.h"
#include "tcpip.h"
#include <cctype>

#ifndef WIN32
#include <ifaddrs.h>
#include <arpa/inet.h>
#else
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#endif

// Global instances
static FILE *log_file = nullptr;
extern int net_start();

// Logging utilities
static void net_log(const char *st, void *buf, const long size)
{
    if (!log_file)
    {
        log_file = prefix_fopen(net_start() ? "abuseclient.log" : "abuseserver.log", "wb");
    }

    fprintf(log_file, "%s%ld - ", st, size);

    // Print readable characters
    for (int i = 0; i < size; i++)
    {
        const unsigned char c = *((unsigned char *)buf + i);
        fprintf(log_file, "%c", isprint(c) ? c : '~');
    }

    fprintf(log_file, " : ");

    // Print hex values
    for (int i = 0; i < size; i++)
    {
        fprintf(log_file, "%02x, ", *((unsigned char *)buf + i));
    }

    fprintf(log_file, "\n");
    fflush(log_file);
}

// ip_address implementation
ip_address::ip_address(const sockaddr_in *addr)
{
    memcpy(&this->addr, addr, sizeof(this->addr));
}

net_address::protocol ip_address::protocol_type() const
{
    return IP;
}

int ip_address::equal(const net_address *who) const
{
    if (who->protocol_type() != IP)
        return 0;
    return !memcmp(&addr.sin_addr, &((ip_address const *)who)->addr.sin_addr, sizeof(addr.sin_addr));
}

int ip_address::set_port(int port)
{
    addr.sin_port = htons(port);
    return 1;
}

void ip_address::print()
{
    const unsigned char *c = (unsigned char *)&addr.sin_addr.s_addr;
    DEBUG_LOG("%d.%d.%d.%d", c[0], c[1], c[2], c[3]);
}

int ip_address::get_port()
{
    return htons(addr.sin_port);
}

net_address *ip_address::copy()
{
    return new ip_address(&addr);
}

void ip_address::store_string(char *st, const int st_length)
{
    char buf[100];
    const unsigned char *c = (unsigned char *)&addr.sin_addr.s_addr;
    snprintf(buf, sizeof(buf), "%d.%d.%d.%d:%d", c[0], c[1], c[2], c[3], htons(addr.sin_port));
    strncpy(st, buf, st_length);
    st[st_length - 1] = 0;
}

// unix_fd implementation
unix_fd::~unix_fd()
{
    unix_fd::read_unselectable();
    unix_fd::write_unselectable();
#ifdef WIN32
    closesocket(fd);
#else
    close(fd);
#endif
}

int unix_fd::ready_to_write()
{
    timeval tv = {0, 0};
    fd_set write_check;
    FD_ZERO(&write_check);
    FD_SET(fd, &write_check);
    select(FD_SETSIZE, nullptr, &write_check, nullptr, &tv);
    return FD_ISSET(fd, &write_check);
}

int unix_fd::write(void const *buf, int size, net_address *addr)
{
    net_log("unix_fd::write:", (char *)buf, size);
    if (addr)
        DEBUG_LOG("Cannot change address for this socket type\n");
#ifdef WIN32
    return send(fd, (char *)buf, size, 0);
#else
    return ::write(fd, buf, size);
#endif
}

int unix_fd::read(void *buf, int size, net_address **addr)
{
    int tr = 0;
#ifdef WIN32
    tr = recv(fd, (char *)buf, size, 0);
#else
    tr = ::read(fd, buf, size);
#endif
    net_log("unix_fd::read:", buf, tr);
    if (addr)
        *addr = nullptr;
    return tr;
}

void unix_fd::broadcastable() const
{
    constexpr int option = 1;
#ifdef WIN32
    if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (const char *)&option, sizeof(option)) < 0)
#else
    if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &option, sizeof(option)) < 0)
#endif
    {
        DEBUG_LOG("Could not set socket option broadcast");
    }
}

// tcp_socket implementation
int tcp_socket::listen(int port)
{
    sockaddr_in host{};
    host.sin_family = AF_INET;
    host.sin_port = htons(port);
    host.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (sockaddr *)&host, sizeof(host)) == -1)
    {
        DEBUG_LOG("Could not bind socket to port %d\n", port);
        return 0;
    }

    if (::listen(fd, 5) == -1)
    {
        DEBUG_LOG("Could not listen to socket on port %d\n", port);
        return 0;
    }

    listening = true;
    return 1;
}

net_socket *tcp_socket::accept(net_address *&addr)
{
    if (!listening)
    {
        addr = nullptr;
        return nullptr;
    }

    sockaddr_in from{};
    socklen_t addr_len = sizeof(from);
    const int new_fd = ::accept(fd, (sockaddr *)&from, &addr_len);

    if (new_fd >= 0)
    {
        addr = new ip_address(&from);
        return new tcp_socket(new_fd);
    }

    addr = nullptr;
    return nullptr;
}

// udp_socket implementation
int udp_socket::read(void *buf, const int size, net_address **addr)
{
    if (addr)
    {
        *addr = new ip_address;
        socklen_t addr_size = sizeof(sockaddr_in);
#ifdef WIN32
        return recvfrom(fd, (char *)buf, size, 0, (sockaddr *)&((ip_address *)*addr)->addr, &addr_size);
#else
        return recvfrom(fd, buf, size, 0, (sockaddr *)&((ip_address *)*addr)->addr, &addr_size);
#endif
    }
#ifdef WIN32
    return recv(fd, (char *)buf, size, 0);
#else
    return recv(fd, buf, size, 0);
#endif
}

int udp_socket::write(void const *buf, int size, net_address *addr)
{
    if (addr)
    {
#ifdef WIN32
        return sendto(fd, (const char *)buf, size, 0, (sockaddr *)&((ip_address *)addr)->addr,
                      sizeof(((ip_address *)addr)->addr));
#else
        return sendto(fd, buf, size, 0, (sockaddr *)&((ip_address *)addr)->addr, sizeof(((ip_address *)addr)->addr));
#endif
    }
#ifdef WIN32
    return send(fd, (char *)buf, size, 0);
#else
    return ::write(fd, buf, size);
#endif
}

int udp_socket::listen(int port)
{
    sockaddr_in host{};
    host.sin_family = AF_INET;
    host.sin_port = htons(port);
    host.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (sockaddr *)&host, sizeof(host)) == -1)
    {
        DEBUG_LOG("Could not bind socket to port %d: %s\n", port, strerror(errno));
        return 0;
    }
    return 1;
}

// tcpip_protocol implementation
tcpip_protocol::tcpip_protocol()
{
    FD_ZERO(&master_set);
    FD_ZERO(&master_write_set);
    FD_ZERO(&read_set);
    FD_ZERO(&exception_set);
    FD_ZERO(&write_set);
}

net_address *tcpip_protocol::get_local_address()
{
    char my_name[100];
    gethostname(my_name, sizeof(my_name));

    if (my_name[0] < '0' || my_name[0] > '9')
    {
        const hostent *l_hn = gethostbyname(my_name);
        if (l_hn)
        {
            auto *addr = new ip_address;
            memset(&addr->addr, 0, sizeof(addr->addr));
            memcpy(&addr->addr.sin_addr, *l_hn->h_addr_list, 4);
            return addr;
        }
    }

#ifdef WIN32
    ULONG bufferSize = 15000;
    PIP_ADAPTER_ADDRESSES adapterAddresses = (PIP_ADAPTER_ADDRESSES)malloc(bufferSize);

    if (GetAdaptersAddresses(AF_INET, 0, nullptr, adapterAddresses, &bufferSize) == NO_ERROR)
    {
        for (PIP_ADAPTER_ADDRESSES adapter = adapterAddresses; adapter; adapter = adapter->Next)
        {
            for (PIP_ADAPTER_UNICAST_ADDRESS unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next)
            {
                sockaddr_in *sa_in = (sockaddr_in *)unicast->Address.lpSockaddr;
                if (sa_in->sin_addr.S_un.S_addr != INADDR_LOOPBACK)
                {
                    auto *addr = new ip_address;
                    addr->addr = *sa_in;
                    free(adapterAddresses);
                    return addr;
                }
            }
        }
    }
    free(adapterAddresses);
#else
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1)
    {
        return nullptr;
    }

    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == nullptr)
        {
            continue;
        }

        if (ifa->ifa_addr->sa_family == AF_INET)
        {
            sockaddr_in *sa_in = (sockaddr_in *)ifa->ifa_addr;
            if (sa_in->sin_addr.s_addr != htonl(INADDR_LOOPBACK))
            {
                auto *addr = new ip_address;
                addr->addr = *sa_in;
                printf("Local IP Address: %s\n", inet_ntoa(addr->addr.sin_addr));
                freeifaddrs(ifaddr);
                return addr;
            }
        }
    }
    freeifaddrs(ifaddr);
#endif

    return nullptr;
}

net_address *tcpip_protocol::get_node_address(char const *&server_host, int def_port, const int force_port)
{
    sockaddr_in host{};
    host.sin_family = AF_INET;

    if (isdigit(server_host[0]))
    {
        unsigned char tmp[4];
        const char *np = server_host;

        for (unsigned char &i : tmp)
        {
            int num = 0;
            while (*np && *np != '.' && *np != ':')
            {
                num = num * 10 + (*np - '0');
                np++;
            }
            i = num;
            if (*np == '.')
                np++;
        }

        if (*np == ':' && !force_port)
        {
            int port;
            if (sscanf(np + 1, "%d", &port) == 1)
            {
                def_port = port;
            }
        }

        host.sin_port = htons(def_port);
        host.sin_addr.s_addr = htonl(INADDR_ANY);
        memcpy(&host.sin_addr, tmp, sizeof(in_addr));

        return new ip_address(&host);
    }

    char name[256];
    const char *src = server_host;
    char *dst = name;
    while (*src && *src != ':' && *src != '/' && dst < name + sizeof(name) - 1)
    {
        *dst++ = *src++;
    }
    *dst = 0;

    if (*src == ':' && !force_port)
    {
        src++;
        int port_num;
        if (sscanf(src, "%d", &port_num) == 1)
        {
            def_port = port_num;
            while (*src && *src != '/')
                src++;
        }
        else
        {
            return nullptr;
        }
    }

    if (*src == '/')
        src++;

    const hostent *hp = gethostbyname(name);
    if (!hp)
    {
        DEBUG_LOG("Unable to locate server named '%s'\n", name);
        return nullptr;
    }

    host.sin_port = htons(def_port);
    host.sin_addr.s_addr = htonl(INADDR_ANY);
    memcpy(&host.sin_addr, hp->h_addr, hp->h_length);

    server_host = src;
    return new ip_address(&host);
}

net_socket *tcpip_protocol::connect_to_server(net_address *addr, const net_socket::socket_type sock_type)
{
    if (addr->protocol_type() != net_address::IP)
    {
        DEBUG_LOG("Protocol type not supported in the executable\n");
        return nullptr;
    }

    int socket_fd = socket(AF_INET, sock_type == net_socket::SOCKET_SECURE ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (socket_fd < 0)
    {
        DEBUG_LOG("Unable to create socket (too many open files?)\n");
        return nullptr;
    }

    if (connect(socket_fd, (sockaddr *)&((ip_address *)addr)->addr, sizeof(((ip_address *)addr)->addr)) == -1)
    {
        DEBUG_LOG("Unable to connect\n");
#ifdef WIN32
        closesocket(socket_fd);
#else
        close(socket_fd);
#endif
        return nullptr;
    }

    return sock_type == net_socket::SOCKET_SECURE ? static_cast<net_socket *>(new tcp_socket(socket_fd))
                                                  : static_cast<net_socket *>(new udp_socket(socket_fd));
}

net_socket *tcpip_protocol::create_listen_socket(const int port, const net_socket::socket_type sock_type)
{
    const int socket_fd = socket(AF_INET, sock_type == net_socket::SOCKET_SECURE ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (socket_fd < 0)
    {
        DEBUG_LOG("Unable to create socket (too many open files?)\n");
        return nullptr;
    }

    net_socket *s = sock_type == net_socket::SOCKET_SECURE ? static_cast<net_socket *>(new tcp_socket(socket_fd))
                                                           : static_cast<net_socket *>(new udp_socket(socket_fd));

    if (!s->listen(port))
    {
        delete s;
        return nullptr;
    }

    return s;
}

void tcpip_protocol::cleanup()
{
    if (notifier)
    {
        end_notify();
    }

    reset_find_list();

    if (responder)
    {
        delete responder;
        delete bcast;
        responder = nullptr;
        bcast = nullptr;
    }
}

net_socket *tcpip_protocol::start_notify(const int port, void *data, const int len)
{
    if (responder)
    {
        delete responder;
        delete bcast;
        responder = nullptr;
        bcast = nullptr;
    }

    const int resp_len = strlen(notify_response);
    notify_len = len + resp_len + 1;
    strcpy(notify_data, notify_response);
    notify_data[resp_len] = '.';
    memcpy(notify_data + resp_len + 1, data, len);

    notifier = create_listen_socket(port, net_socket::SOCKET_FAST);
    if (notifier)
    {
        notifier->read_selectable();
        notifier->write_unselectable();
    }
    else
    {
        DEBUG_LOG("Couldn't start notifier\n");
    }

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
    if (!notifier)
    {
        return 0;
    }

    if (notifier->ready_to_read())
    {
        char buf[513];
        net_address *tmp;
        const int len = notifier->read(buf, 512, &tmp);
        auto *addr = static_cast<ip_address *>(tmp);

        if (addr && len > 0)
        {
            buf[len] = 0;
            if (strcmp(buf, notify_signature) == 0)
            {
                notifier->write(notify_data, notify_len, addr);
            }
            delete addr;
        }
        return 1;
    }

    if (notifier->error())
    {
        DEBUG_LOG("Error on notification socket!\n");
        return 1;
    }

    return 0;
}

int tcpip_protocol::handle_responder()
{
    if (!responder)
    {
        return 0;
    }

    if (responder->ready_to_read())
    {
        char buf[513];
        net_address *tmp;
        const int len = responder->read(buf, 512, &tmp);
        auto *addr = static_cast<ip_address *>(tmp);

        if (addr && len > 0)
        {
            buf[len] = 0;
            buf[4] = 0; // Limit response signature check
            if (strcmp(buf, notify_response) == 0)
            {
                bool found = false;

                // Check servers list
                for (p_request p = servers.begin(); !found && p != servers.end(); ++p)
                {
                    if ((*p)->addr->equal(addr))
                    {
                        found = true;
                    }
                }

                // Check returned list
                for (p_request q = returned.begin(); !found && q != returned.end(); ++q)
                {
                    if ((*q)->addr->equal(addr))
                    {
                        found = true;
                    }
                }

                if (!found)
                {
                    auto *request = new RequestItem;
                    request->addr = addr;
                    strncpy(request->name, buf + 5, sizeof(request->name) - 1);
                    request->name[sizeof(request->name) - 1] = 0;
                    // Append IP address to the name for display
                    char ipbuf[32];
#if defined(WIN32)
                    strncpy(ipbuf, inet_ntoa(addr->addr.sin_addr), sizeof(ipbuf) - 1);
                    ipbuf[sizeof(ipbuf) - 1] = 0;
#else
                    inet_ntop(AF_INET, &addr->addr.sin_addr, ipbuf, sizeof(ipbuf));
#endif
                    size_t cur_len = strlen(request->name);
                    if (cur_len < sizeof(request->name) - 5) // ensure space for at least ' ()' and some IP
                    {
                        snprintf(request->name + cur_len, sizeof(request->name) - cur_len, " (%s)", ipbuf);
                    }
                    servers.insert(request);
                    addr = nullptr; // Prevent deletion
                }
            }

            delete addr; // Delete if not stored in servers list
        }

        return 1;
    }

    if (responder->error())
    {
        DEBUG_LOG("Error on responder socket!\n");
        return 1;
    }

    return 0;
}

/**
 * Monitors sockets for activity using select() system call.
 * This method checks all registered sockets for readability, writability, and exceptions.
 * 
 * @param block If true, the function blocks until at least one socket is ready;
 *              if false, it returns immediately regardless of socket status.
 * 
 * @return The number of ready sockets, or a negative value if an error occurred.
 *         Note: The return value is adjusted by subtracting counts for internally
 *         handled notification and responder events.
 */
int tcpip_protocol::select(const bool block)
{
    memcpy(&read_set, &master_set, sizeof(master_set));
    memcpy(&exception_set, &master_set, sizeof(master_set));
    memcpy(&write_set, &master_write_set, sizeof(master_set));

    int ret;
    if (block)
    {
        ret = 0;
        while (ret == 0)
        {
            ret = ::select(FD_SETSIZE, &read_set, &write_set, &exception_set, nullptr);
            if (handle_notification())
                ret--;
            if (handle_responder())
                ret--;
        }
    }
    else
    {
        timeval tv = {0, 0};
        ret = ::select(FD_SETSIZE, &read_set, &write_set, &exception_set, &tv);
        if (handle_notification())
            ret--;
        if (handle_responder())
            ret--;
    }

    return ret;
}

net_address *tcpip_protocol::find_address(const int port, char *name)
{
    end_notify();

    if (!responder)
    {
        // Use ephemeral port (0) so we don't collide with the actual server port
        responder = create_listen_socket(0, net_socket::SOCKET_FAST);
        if (responder)
        {
            responder->read_selectable();
            responder->write_unselectable();
            if (auto *ufd = dynamic_cast<unix_fd *>(responder))
            {
                ufd->broadcastable();
            }

            // Determine broadcast address
#ifndef WIN32
            struct ifaddrs *ifaddr = nullptr;
            if (getifaddrs(&ifaddr) == 0)
            {
                for (struct ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next)
                {
                    if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
                        continue;
                    sockaddr_in *sa = (sockaddr_in *)ifa->ifa_addr;
                    if (sa->sin_addr.s_addr == htonl(INADDR_LOOPBACK))
                        continue; // skip loopback

                    sockaddr_in baddr{};
                    baddr.sin_family = AF_INET;
                    baddr.sin_port = htons(port);

                    if (ifa->ifa_broadaddr && ifa->ifa_broadaddr->sa_family == AF_INET)
                    {
                        baddr.sin_addr = ((sockaddr_in *)ifa->ifa_broadaddr)->sin_addr;
                    }
                    else if (ifa->ifa_netmask && ifa->ifa_netmask->sa_family == AF_INET)
                    {
                        // Compute broadcast = ip | ~netmask
                        uint32_t ip = ntohl(sa->sin_addr.s_addr);
                        uint32_t mask = ntohl(((sockaddr_in *)ifa->ifa_netmask)->sin_addr.s_addr);
                        uint32_t bcast_ip = (ip & mask) | (~mask);
                        baddr.sin_addr.s_addr = htonl(bcast_ip);
                    }
                    else
                    {
                        // Fallback to limited broadcast
                        baddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
                    }

                    bcast = new ip_address(&baddr);
                    break; // use first suitable interface
                }
                freeifaddrs(ifaddr);
            }
#endif
            if (!bcast)
            {
                // Fallback to 255.255.255.255
                sockaddr_in baddr{};
                baddr.sin_family = AF_INET;
                baddr.sin_port = htons(port);
                baddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
                bcast = new ip_address(&baddr);
            }
        }
    }

    if (responder && bcast)
    {
        // Send a single broadcast probe
        responder->write(notify_signature, strlen(notify_signature), bcast);
        select(false);
    }

    if (servers.empty())
    {
        return nullptr;
    }

    // Move first server to returned list and return its address
    servers.move_next(servers.begin_prev(), returned.begin_prev());
    auto *ret = static_cast<ip_address *>((*returned.begin())->addr->copy());
    strncpy(name, (*returned.begin())->name, 256);
    name[255] = 0;

    return ret;
}

void tcpip_protocol::reset_find_list()
{
    for (const auto *p : servers)
    {
        delete p->addr;
        delete p;
    }

    for (const auto *p : returned)
    {
        delete p->addr;
        delete p;
    }

    servers.erase_all();
    returned.erase_all();
}

#endif // HAVE_NETWORK