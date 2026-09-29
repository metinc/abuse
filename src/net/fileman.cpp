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
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <algorithm>
#include <climits>
#include <vector>
#ifdef WIN32
#include <io.h>
#endif

#include "common.h"

#include "fileman.h"
#include "netface.h"
#include "ghandler.h"
#include "specache.h"

extern net_protocol *prot;

file_manager *fman = NULL;

file_manager::file_manager(int argc, char **argv, net_protocol *proto)
    : default_fs(nullptr), no_security(0), nfs_list(nullptr), remote_list(nullptr), proto(proto)
{
    int i;
    for (i = 1; i < argc; i++)
        if (!strcmp(argv[i], "-bastard")) // this bypasses filename security features
        {
            fprintf(stderr, "Warning : Security measures bypassed (-bastard)\n");
            no_security = 1;
        }
}

void file_manager::process_net()
{
    nfs_client *nc, *last = NULL;
    for (nc = nfs_list; nc;) // check for nfs request
    {
        int ok = 1;

        if (nc->sock->error())
        {
            ok = 0;
            // fprintf(stderr,"Killing nfs client, socket went bad\n");
        }
        else if (nc->sock->ready_to_read())
            ok = process_nfs_command(nc); // if we couldn't process the packet, delete the connection

        if (ok)
        {
            last = nc;
            nc = nc->next;
        }
        else
        {
            if (last)
                last->next = nc->next;
            else
                nfs_list = nc->next;
            nfs_client *c = nc;
            nc = nc->next;
            delete c;
        }
    }
}

int file_manager::process_nfs_command(nfs_client *c)
{
    char cmd;
    if (c->sock->read(/* client_nfs_command */ &cmd, 1) != 1)
        return 0;

    switch (cmd)
    {
    case NFCMD_READ: {
        int32_t requested;
        if (c->sock->read(&requested, sizeof(requested)) != sizeof(requested))
            return 0;
        requested = lltl(requested);
        if (requested < 0 || requested > INT_MAX - static_cast<int>(sizeof(int32_t)))
            return 0;

        const auto offset = lseek(c->file_fd, 0, SEEK_CUR);
        if (offset < 0)
            return 0;
        const int32_t available = offset < c->size ? c->size - offset : 0;
        const int32_t count = std::min(requested, available);
        std::vector<char> response(sizeof(int32_t) + count);
        int32_t received = 0;
        while (received < count)
        {
            const int got = read(c->file_fd, response.data() + sizeof(int32_t) + received, count - received);
            if (got < 0)
            {
                if (errno == EINTR)
                    continue;
                return 0;
            }
            if (got == 0)
                break;
            received += got;
        }

        // One length followed by the complete payload. The transport handles packet boundaries.
        const int32_t length = lltl(received);
        memcpy(response.data(), &length, sizeof(length));
        const int response_size = sizeof(length) + received;
        return c->sock->write(response.data(), response_size) == response_size;
    }
    break;
    case NFCMD_CLOSE: {
        return 0;
    }
    break;
    case NFCMD_SEEK: {
        int32_t offset;
        if (c->sock->read(/* client_seek_offset */ &offset, sizeof(offset)) != sizeof(offset))
            return 0;
        offset = lltl(offset);
        offset = lseek(c->file_fd, offset, 0);
        offset = lltl(offset);
        if (c->sock->write(/* server_seek_response */ &offset, sizeof(offset)) != sizeof(offset))
            return 0;
        return 1;
    }
    break;
    case NFCMD_TELL: {
        int32_t offset = lseek(c->file_fd, 0, SEEK_CUR);
        offset = lltl(offset);
        if (c->sock->write(/* server_tell_response */ &offset, sizeof(offset)) != sizeof(offset))
            return 0;
        return 1;
    }
    break;

    default: {
        fprintf(stderr, "net driver : bad command from nfs client\n");
        return 0;
    }
    }
}

void file_manager::secure_filename(char *filename, char *mode)
{
    if (!no_security)
    {
        if (filename[0] == '/')
        {
            filename[0] = 0;
            return;
        }
        int level = 0;
        char *f = filename;
        while (*f)
        {
            if (*f == '/')
            {
                f++;
                level++;
            }
            else if (*f == '.' && f[1] == '.')
            {
                if (f[3] == '.')
                    while (*f != '.')
                        f++;
                else
                {
                    f += 2;
                    level--;
                }
            }
            else
                f++;
        }
        if (level < 0)
            filename[0] = 0;
    }
}

file_manager::nfs_client::nfs_client(net_socket *sock, int file_fd, nfs_client *next)
    : sock(sock), file_fd(file_fd), next(next), size(0)
{
    sock->read_selectable();
}

file_manager::nfs_client::~nfs_client()
{
    delete sock;
    if (file_fd >= 0)
        close(file_fd);
}

void file_manager::add_nfs_client(net_socket *sock)
{
    uint8_t size[2];
    char filename[300], mode[20], *mp;
    if (sock->read(/* client_nfs_connect_info */ size, 2) !=
        2) // read size 2 bytes, because command was parsed by service_net_request() in innet.cpp
    {
        delete sock;
        return;
    }
    if (sock->read(/* client_nfs_filename */ filename, size[0]) != size[0])
    {
        delete sock;
        return;
    }
    if (sock->read(/* client_nfs_mode */ mode, size[1]) != size[1])
    {
        delete sock;
        return;
    }

    secure_filename(filename, mode); // make sure this filename isn't a security risk
    if (filename[0] == 0)
    {
        fprintf(stderr, "(denied)\n");
        delete sock;
        return;
    }

    mp = mode;
    int flags = 0;

    while (*mp)
    {
        if (*mp == 'w')
            flags |= O_CREAT | O_RDWR;
        else if (*mp == 'r')
            flags |= O_RDONLY;
#ifdef WIN32
        else if (*mp == 'b')
            flags |= O_BINARY;
#endif
        mp++;
    }

#ifdef WIN32
    int f = prefix_open(filename, flags, _S_IREAD | _S_IWRITE);
#else
    int f = prefix_open(filename, flags, S_IRWXU | S_IRWXG | S_IRWXO);
#endif

    FILE *fp = prefix_fopen("open.log", "ab");
    fprintf(fp, "open file %s, fd=%d\n", filename, f);
    fclose(fp);

    if (f < 0)
        f = -1; // make sure this is -1

    int32_t ret = lltl(f);
    if (sock->write(/* server_nfs_fd */ &ret, sizeof(ret)) != sizeof(ret))
    {
        delete sock;
        return;
    }

    if (f < 0) // no file, sorry
        delete sock;
    else
    {
        int32_t cur_pos = lseek(f, 0, SEEK_CUR);
        int32_t size = lseek(f, 0, SEEK_END);
        lseek(f, cur_pos, SEEK_SET);
        size = lltl(size);
        if (sock->write(/* server_nfs_filesize */ &size, sizeof(size)) != sizeof(size))
        {
            close(f);
            delete sock;
            sock = NULL;
            return;
        }

        nfs_list = new nfs_client(sock, f, nfs_list);
        nfs_list->size = lltl(size);
    }
}

void file_manager::remote_file::r_close(char const *reason)
{
    //  if (reason) fprintf(stderr,"remote_file : %s\n",reason);

    if (sock)
    {
        delete sock;
        sock = NULL;
    }
}

file_manager::remote_file::remote_file(net_socket *sock, char const *filename, char const *mode, remote_file *Next)
    : sock(sock), socket_fd(sock->get_fd())
{
    next = Next;
    open_local = 0;

    uint8_t sizes[3] = {CLIENT_NFS, static_cast<uint8_t>(strlen(filename) + 1), static_cast<uint8_t>(strlen(mode) + 1)};

    if (sock->write(/* client_nfs_connect_info */ sizes, 3) != 3)
    {
        r_close("could not send open info");
        return;
    }
    if (sock->write(/* client_nfs_filename */ filename, sizes[1]) != sizes[1])
    {
        r_close("could not send filename");
        return;
    }
    if (sock->write(/* client_nfs_mode */ mode, sizes[2]) != sizes[2])
    {
        r_close("could not send mode");
        return;
    }

    int32_t remote_file_fd;
    if (sock->read(/* server_nfs_fd */ &remote_file_fd, sizeof(remote_file_fd)) != sizeof(remote_file_fd))
    {
        r_close("could not read remote fd");
        return;
    }
    remote_file_fd = lltl(remote_file_fd);
    if (remote_file_fd < 0)
    {
        r_close("remote fd is bad");
        return;
    }

    if (sock->read(/* server_nfs_filesize */ &size, sizeof(size)) != sizeof(size))
    {
        r_close("could not read remote filesize");
        return;
    }

    size = lltl(size);
}

int file_manager::remote_file::unbuffered_read(void *buffer, size_t count)
{
    if (!sock || !count)
        return 0;

    const int32_t requested = std::min(count, static_cast<size_t>(INT_MAX - sizeof(int32_t)));
    const int32_t length = lltl(requested);
    uint8_t request[1 + sizeof(length)] = {NFCMD_READ};
    memcpy(request + 1, &length, sizeof(length));
    if (sock->write(request, sizeof(request)) != sizeof(request))
    {
        r_close("read : could not send request");
        return 0;
    }

    int32_t received;
    if (sock->read(&received, sizeof(received)) != sizeof(received))
    {
        r_close("read : could not read size");
        return 0;
    }
    received = lltl(received);
    if (received < 0 || received > requested)
    {
        r_close("read : invalid size");
        return 0;
    }
    if (received && sock->read(buffer, received) != received)
    {
        r_close("read : incomplete payload");
        return 0;
    }
    return received;
}

int32_t file_manager::remote_file::unbuffered_tell() // ask server where the offset of the file pointer is
{
    if (sock)
    {
        uint8_t cmd = NFCMD_TELL;
        if (sock->write(/* client_nfs_command */ &cmd, sizeof(cmd)) != sizeof(cmd))
        {
            r_close("tell : could not send command");
            return 0;
        }

        int32_t offset;
        if (sock->read(/* server_tell_response */ &offset, sizeof(offset)) != sizeof(offset))
        {
            r_close("tell : could not read offset");
            return 0;
        }
        return lltl(offset);
    }
    return 0;
}

int32_t file_manager::remote_file::unbuffered_seek(int32_t offset) // tell server to seek to a spot in a file
{
    if (sock)
    {
        uint8_t cmd = NFCMD_SEEK;
        if (sock->write(/* client_nfs_command */ &cmd, sizeof(cmd)) != sizeof(cmd))
        {
            r_close("seek : could not send command");
            return 0;
        }

        int32_t off = lltl(offset);
        if (sock->write(/* client_seek_offset */ &off, sizeof(off)) != sizeof(off))
        {
            r_close("seek : could not send offset");
            return 0;
        }

        if (sock->read(/* server_seek_response */ &offset, sizeof(offset)) != sizeof(offset))
        {
            r_close("seek : could not read offset");
            return 0;
        }
        return lltl(offset);
    }
    return 0;
}

file_manager::remote_file::~remote_file()
{
    r_close(NULL);
}

int file_manager::rf_open_file(char const *&filename, char const *mode)
{
    net_address *fs_server_addr = NULL;

    if (filename[0] == '/' && filename[1] == '/') // passive server file reference?
    {
        filename += 2;

        fs_server_addr = prot->get_node_address(filename, DEFAULT_COMM_PORT, 0);
        if (!fs_server_addr)
        {
            printf("could not get address for %s\n", filename);
            return -1;
        }
    }
    else if (default_fs)
        fs_server_addr = default_fs->copy();

    if (fs_server_addr)
    {
        net_socket *sock = proto->connect_to_server(fs_server_addr, net_socket::SOCKET_SECURE);
        delete fs_server_addr;

        if (!sock)
        {
            fprintf(stderr, "unable to connect\n");
            return -1;
        }

        remote_file *rf = new remote_file(sock, filename, mode, remote_list);
        if (rf->open_failure())
        {
            delete rf;
            return -1;
        }
        else
        {
            remote_list = rf;
            return rf->fd();
        }
    }

    // FIXME: AK - Should we keep this?
    //  secure_filename(filename,mode);
    if (filename[0] == 0)
        return -1;

    int flags = 0;
    while (*mode)
    {
        if (*mode == 'w')
        {
            flags |= O_CREAT | O_RDWR;
        }
        else if (*mode == 'r')
        {
            flags |= O_RDONLY;
        }
#ifdef WIN32
        else if (*mode == 'b')
        {
            flags |= O_BINARY;
        }
#endif
        mode++;
    }

#ifdef WIN32
    int f = prefix_open(filename, flags, S_IREAD | S_IWRITE);
#else
    int f = prefix_open(filename, flags, S_IRWXU | S_IRWXG | S_IRWXO);
#endif
    if (f >= 0)
    {
        close(f);
        return -2;
    }

    fprintf(stderr, "Unable to open file '%s': %s\n", filename, strerror(errno));

    return -1;
}

file_manager::remote_file *file_manager::find_rf(int fd)
{
    remote_file *r = remote_list;
    for (; r && r->fd() != fd; r = r->next)
    {
        if (r->fd() == -1)
        {
            fprintf(stderr, "bad sock\n");
        }
    }
    if (!r)
    {
        fprintf(stderr, "Bad fd for remote file %d\n", fd);
    }
    return r;
}

int32_t file_manager::rf_tell(int fd)
{
    remote_file *rf = find_rf(fd);
    if (rf)
        return rf->unbuffered_tell();
    else
        return 0;
}

int32_t file_manager::rf_seek(int fd, int32_t offset)
{
    remote_file *rf = find_rf(fd);
    if (rf)
        return rf->unbuffered_seek(offset);
    else
        return 0;
}

int file_manager::rf_read(int fd, void *buffer, size_t count)
{
    remote_file *rf = find_rf(fd);
    if (rf)
        return rf->unbuffered_read(buffer, count);
    else
        return 0;
}

int file_manager::rf_close(int fd)
{
    remote_file *rf = remote_list, *last = NULL;
    while (rf && rf->fd() != fd)
    {
        last = rf;
        rf = rf->next;
    }
    if (rf)
    {
        if (last)
            last->next = rf->next;
        else
            remote_list = rf->next;
        delete rf;
        return 1;
    }
    else
    {
        fprintf(stderr, "Bad fd for remote file %d\n", fd);
        return 0;
    }
}

int32_t file_manager::rf_file_size(int fd)
{
    remote_file *rf = find_rf(fd);
    if (rf)
        return rf->file_size();
    else
        return 0;
}
