/*
 * Delay client packets
 *
 * icqprx Copyright (C) © 2007 Rüdiger Kuhlmann
 *
 * icqprx is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 dated June, 1991.
 *
 * icqprx is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * $Id$
 */

#include "micq.h"
#include <errno.h>
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#include <fcntl.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <assert.h>
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#if HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

int wait_fd (int fd, int mask, long int to)
{
    int res, rc, ret;
    
    while (1)
    {
        fd_set fds[3];
        struct timeval tv;

        FD_ZERO (&fds[0]);
        FD_ZERO (&fds[1]);
        FD_ZERO (&fds[2]);
        if (mask & 1)
            FD_SET (fd, &fds[0]);
        if (mask & 2)
            FD_SET (fd, &fds[1]);
        if (mask & 4)
            FD_SET (fd, &fds[2]);
        if (to)
        {
            tv.tv_sec = to / 1000000;
            tv.tv_usec = to % 1000000;
        }
        else
        {
            tv.tv_sec = 10;
            tv.tv_usec = 0;
        }
        
        errno = 0;
        res = select (fd + 1, &fds[0], &fds[1], &fds[2], &tv);
        rc = errno;
        if (res == -1 && (rc == EINTR || rc == EAGAIN))
            continue;
        assert (res != -1);
        ret = 0;
        if (FD_ISSET (fd, &fds[0])) ret |= 1;
        if (FD_ISSET (fd, &fds[1])) ret |= 2;
        if (FD_ISSET (fd, &fds[2])) ret |= 4;
        if (ret)
            return ret;
        if (to)
            return 0;
    }
}

int do_listen (int *port)
{
    int rc, sok;
    socklen_t length;
    struct sockaddr_in sin;

    errno = 0;
    sok = socket (AF_INET, SOCK_STREAM, 0);
    assert (sok >= 0);
#if HAVE_FCNTL
    rc = fcntl (sok, F_GETFL, 0);
    if (rc != -1)
        rc = fcntl (sok, F_SETFL, rc | O_NONBLOCK);
#endif

    sin.sin_family = AF_INET;
    sin.sin_port = htons (*port);
    sin.sin_addr.s_addr = INADDR_ANY;

    rc = 1;
    setsockopt (sok, SOL_SOCKET, SO_REUSEADDR, &rc, sizeof (int));

    if (bind (sok, (struct sockaddr*)&sin, sizeof (struct sockaddr)) < 0)
    {
        rc = errno;
        assert (!rc);
    }
    rc = listen (sok, 10);
    assert (rc >= 0);

    length = sizeof (struct sockaddr);
    getsockname (sok, (struct sockaddr *) &sin, &length);
    *port = ntohs (sin.sin_port);
    
    return sok;
}

int do_accept (int fd)
{
    int sok;
    struct sockaddr_in sin;
    socklen_t length;
    int rc;
    
    wait_fd (fd, 1, 0);
    
    length = sizeof (sin);
    sok  = accept (fd, (struct sockaddr *)&sin, &length);

    assert (sok > 0);
#if HAVE_FCNTL
    rc = fcntl (sok, F_GETFL, 0);
    if (rc != -1)
        rc = fcntl (sok, F_SETFL, rc | O_NONBLOCK);
#endif
    assert (rc != -1);
    return sok;
}

int do_server (const char *hostname, int port)
{
    int rc, rce, sok, ip;
    socklen_t length;
    struct sockaddr_in sin;
    struct hostent *host;

    errno = 0;
    sok = socket (AF_INET, SOCK_STREAM, 0);
    assert (sok >= 0);
#if HAVE_FCNTL
    rc = fcntl (sok, F_GETFL, 0);
    if (rc != -1)
        rc = fcntl (sok, F_SETFL, rc | O_NONBLOCK);
#endif
    assert (rc != -1);

    sin.sin_family = AF_INET;
    sin.sin_port = htons (port);

    if (hostname)
        ip = htonl (inet_addr (hostname));
    if (ip + 1 == 0 && hostname)
    {
        host = gethostbyname (hostname);
        assert (host);
        sin.sin_addr = *((struct in_addr *) host->h_addr);
        ip = ntohl (sin.sin_addr.s_addr);
    }
    sin.sin_addr.s_addr = htonl (ip);

    rc = connect (sok, (struct sockaddr *) &sin, sizeof (struct sockaddr));
    rce = rc < 0 ? errno : 0;

    length = sizeof (struct sockaddr);
    getsockname (sok, (struct sockaddr *) &sin, &length);

    if (rc >= 0)
        return sok;

#if !defined(EINPROGRESS)
#define EINPROGRESS 1
        rce = 1;
#endif
    assert (rce == EINPROGRESS);

    wait_fd (sok, 6, 0);

    length = sizeof (int);
#ifdef SO_ERROR
    if (getsockopt (sok, SOL_SOCKET, SO_ERROR, (void *)&rc, &length) < 0)
#endif
        rc = errno;
    assert (!rc);
    return sok;
}

unsigned get_len (char *buf)
{
    unsigned char h = buf[4];
    unsigned char l = buf[5];
    unsigned r = h * 256 + l;
    return r;
}

int read_all_data (int fd, char *buf, int *index, int *reported, int size, int out)
{
    int ret, len;

    if (fd < 0)
        return 1;

    printf ("Reading all from %s.\n", out ? "server" : "client");    
    while (1)
    {
        ret = wait_fd (fd, 1, 250000);
        if (!ret)
        {
            printf ("No more data from %s.\n", out ? "server" : "client");
            return 0;
        }
        assert (size > *index + 10);
        len = read (fd, buf + *index, size - *index);
        if (!len)
        {
            printf ("EOF from %s.\n", out ? "server" : "client");
            return 1;
        }
        ret = errno;
        if (len == -1 && ret == ECONNRESET)
        {
            printf ("EOF2 from %s.\n", out ? "server" : "client");
            return 1;
        }
        assert (len > 0);
        *index += len;
        assert (*index >= *reported);
        while (*index - *reported >= 6)
        {
            len = get_len (buf + *reported);
            if (*index - *reported < 6 + len)
                break;
            printf ("Received %s (%d,%d) (0x%x,0x%x) from %s (6+%u bytes).\n",
                    buf[*reported + 1] == 2 ? "SNAC" : "FLAP",
                    buf[*reported + 7], buf[*reported + 9], buf[*reported + 7], buf[*reported + 9],
                    out ? "server" : "client", len);
            *reported += len + 6;
        }
        if (*index > *reported)
        {
            printf ("Received %d unreported bytes.\n", *index - *reported);
            for (len = *reported; len < *index; len++)
            {
                printf ("<%x|%c> ", buf[len], buf[len]);
                if (!(len % 8))
                    printf ("\n");
            }
            if ((len - 1) % 8)
                printf ("\n");
        }
    }
}        

char *use_host = "login.icq.com";
int use_port = 5190;

int send_packet (int tofd, char *buf, int *index, int *reported, int port, int out)
{
    size_t len, done, rc;

    if (*index < 6)
        return 0;

    len = get_len (buf);
    if (*index < 6 + len)
        return 0;
    
    printf ("Sending %s (%d,%d) (0x%x,0x%x) to %s.\n",
            buf[1] == 2 ? "SNAC" : "FLAP",
            buf[7], buf[9], (unsigned)buf[7], (unsigned)buf[9],
            out ? "server" : "client");

    assert (*index >= len + 6);

    if (buf[1] == 2 && buf[7] == 23 && buf[9] == 3)
    {
        done = 16;
        while (done + 4 <= len && buf[done + 1] != 5)
        {
            done += ((unsigned char)buf[done + 2]) * 256;
            done += ((unsigned char)buf[done + 3]) + 4;
        }
        if (done + 4 <= len)
        {
            if (*index > len + 6)
                memmove (buf + len + 14 + 6, buf + len + 6, *index - len - 6);
            use_host = strdup (buf + done + 4);
            use_port = atoi (strchr (use_host, ':') + 1);
            *strchr (use_host, ':') = 0;
            buf[done] = -1;
            buf[4] = (len + 18) / 256;
            buf[5] = (len + 18) % 256;
            len += 6;
            buf[len++] = 0;
            buf[len++] = 5;
            buf[len++] = 0;
            buf[len++] = 14;
            memcpy (buf + len, "127.0.0.1:1234", 14);
            len += 14 - 6;
            *index += 18;
            *reported += 18;
        }
    }
    else if (buf[1] == 4)
    {
        done = 6;
        while (done + 4 <= len && buf[done + 1] != 5)
        {
            done += ((unsigned char)buf[done + 2]) * 256;
            done += ((unsigned char)buf[done + 3]) + 4;
        }
        if (done + 4 <= len)
        {
            printf ("index: %u len %u\n", *index, len);
            if (*index > len + 6)
                memmove (buf + len + 14 + 6, buf + len + 6, *index - len - 6);
            use_host = strdup (buf + done + 4);
            use_port = atoi (strchr (use_host, ':') + 1);
            *strchr (use_host, ':') = 0;
            buf[done] = -1;
            buf[4] = (len + 18) / 256;
            buf[5] = (len + 18) % 256;
            len += 6;
            buf[len++] = 0;
            buf[len++] = 5;
            buf[len++] = 0;
            buf[len++] = 14;
            memcpy (buf + len, "127.0.0.1:1234", 14);
            len += 14 - 6;
            *index += 18;
            *reported += 18;
        }
    }

    assert (*index >= len + 6);
    done = 0;
    len += 6;
    while (done < len)
    {
        rc = write (tofd, buf + done, len - done);
        assert (rc > 0);
        done += rc;
    }
    assert (*index >= len);
    memmove (buf, buf + len, *index - len);
    *index -= len;
    *reported -= len;
    return 1;
}

void do_exchange (int fd_cli, int fd_srv, int port)
{
    char from_cli_buf[1024*1024];
    char to_cli_buf[1024*1024];
    int from_cli_index = 0, from_cli_reported = 0;
    int to_cli_index = 0, to_cli_reported = 0;
    int ret;

    while (1)
    {
        ret = read_all_data (fd_cli, from_cli_buf, &from_cli_index, &from_cli_reported, 1024*1024, 0);
        if (ret)
        {
            while (send_packet (fd_srv, from_cli_buf, &from_cli_index, &from_cli_reported, port, 1))
                ;
            close (fd_cli);
            close (fd_srv);
            return;
        }
        send_packet (fd_srv, from_cli_buf, &from_cli_index, &from_cli_reported, port, 1);
        ret = read_all_data (fd_srv, to_cli_buf, &to_cli_index, &to_cli_reported, 1024*1024, 1);
        if (ret)
        {
            while (send_packet (fd_cli, to_cli_buf, &to_cli_index, &to_cli_reported, port, 0))
                ;
            close (fd_cli);
            close (fd_srv);
            return;
        }
        send_packet (fd_cli, to_cli_buf, &to_cli_index, &to_cli_reported, port, 0);
    }
}

int main (int argc, char **argv)
{
    int fd_cli = -1, fd_lis = -1, fd_srv = -1;
    int port = 1234;

    fd_lis = do_listen (&port);
    while (1)
    {
        printf ("Waiting for connections... (login)\n");
        fd_cli = do_accept (fd_lis);
        fd_srv = do_server ("login.icq.com", 5190);
        do_exchange (fd_cli, fd_srv, 1234);
        printf ("Waiting for connections... (session)\n");
        fd_cli = do_accept (fd_lis);
        fd_srv = do_server (use_host, use_port);
        do_exchange (fd_cli, fd_srv, 1234);
        fflush (stdout);
    }
    return 0;
}
