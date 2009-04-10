/*
 * Basic TCP I/O layer.
 *
 * climm (C) © 2001-2009 Rüdiger Kuhlmann
 *
 * This extension is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 dated June, 1991.
 *
 * This extension is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * In addition, as a special exception permission is granted to link the
 * code of this release of climm with the OpenSSL project's "OpenSSL"
 * library, and distribute the linked executables.  You must obey the GNU
 * General Public License in all respects for all of the code used other
 * than "OpenSSL".  If you modify this file, you may extend this exception
 * to your version of the file, but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your version
 * of this file.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * $Id: util_ssl.c 2723 2009-03-10 21:44:18Z kuhlmann $
 */

#include "climm.h"
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
#if !HAVE_DECL_H_ERRNO
extern int h_errno;
#endif
#include <signal.h>
#include <stdarg.h>
#if HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#include <fcntl.h>
#include <assert.h>

#include "connection.h"
#include "io/io_private.h"
#include "io/io_tcp.h"

#if 0
#include "util_ui.h"
#else
#undef DebugH
#define DebugH(...)
#endif

#ifndef HAVE_HSTRERROR
static inline const char *hstrerror (int rc)
{
    return "";
}
#endif
    
#ifndef HAVE_GETHOSTBYNAME
static inline struct hostent *gethostbyname(const char *name)
{
    return NULL;
}
#endif

#define BACKLOG 10

static void        io_tcp_open  (Connection *c, Dispatcher *d);
static io_err_t    io_tcp_connecting (Connection *conn, Dispatcher *d);
static int         io_tcp_read  (Connection *c, Dispatcher *d, char *buf, size_t count);
static io_err_t    io_tcp_write (Connection *c, Dispatcher *d, const char *buf, size_t count);
static void        io_tcp_close (Connection *c, Dispatcher *d);
static const char *io_tcp_err   (Connection *c, Dispatcher *d);

static int   io_listen_tcp_accept(Connection *c, Dispatcher *d, Connection *cn);
static void  io_listen_tcp_open  (Connection *c, Dispatcher *d);

static Conn_Func io_tcp_func = {
    NULL,
    &io_tcp_connecting,
    &io_tcp_read,
    &io_tcp_write,
    &io_tcp_close,
    &io_tcp_err
};

static Conn_Func io_listen_tcp_func = {
    &io_listen_tcp_accept,
    &io_tcp_connecting,
    NULL,
    NULL,
    &io_tcp_close,
    &io_tcp_err
};

void IOConnectTCP (Connection *conn)
{
    assert (conn);
    UtilIOClose (conn);
    conn->connect &= ~CONNECT_SELECT_R & ~CONNECT_SELECT_W & ~CONNECT_SELECT_X;
    conn->connect |= CONNECT_SELECT_A;
    conn->dispatcher = calloc (1, sizeof (Dispatcher));
    DebugH (DEB_TCP, "conn %p connect %p", conn, conn->dispatcher);
    if (!conn->dispatcher)
        return;
    conn->dispatcher->funcs = &io_tcp_func;
    io_tcp_open (conn, conn->dispatcher);
}

void IOListenTCP (Connection *conn)
{
    assert (conn);
    UtilIOClose (conn);
    conn->connect &= ~CONNECT_SELECT_R & ~CONNECT_SELECT_W & ~CONNECT_SELECT_X;
    conn->connect |= CONNECT_SELECT_A;
    conn->dispatcher = calloc (1, sizeof (Dispatcher));
    DebugH (DEB_TCP, "conn %p listen %p", conn, conn->dispatcher);
    if (!conn->dispatcher)
        return;
    conn->dispatcher->funcs = &io_listen_tcp_func;
    io_listen_tcp_open (conn, conn->dispatcher);
}

/*
 * Handles timeout on TCP connect
 */
static void io_tcp_to (Event *event)
{
    Connection *conn = event->conn;
    EventD (event);
    DebugH (DEB_TCP, "conn %p timeout", conn);
    conn->connect &= ~CONNECT_SELECT_R & ~CONNECT_SELECT_W & ~CONNECT_SELECT_X;
    conn->connect |= CONNECT_SELECT_A;
    conn->dispatcher->flags = FLAG_TIMEOUT;
}

static const char *io_tcp_err (Connection *conn, Dispatcher *d)
{
     return d->lasterr;
}

static char io_tcp_makesocket (Connection *conn, Dispatcher *d)
{
    int rc;

    errno = 0;
    conn->sok = socket (AF_INET, SOCK_STREAM, 0);
    if (conn->sok < 0)
    {
        d->d_errno = errno;
        d->err = IO_NO_SOCKET;
        DebugH (DEB_TCP, "conn %p socket %d err %d %s", conn, conn->sok, errno, strerror (errno));
        s_repl (&d->lasterr, strerror (errno));
        return 0;
    }
    DebugH (DEB_TCP, "conn %p socket %d", conn, conn->sok);
#if HAVE_FCNTL
    rc = fcntl (conn->sok, F_GETFL, 0);
    if (rc != -1)
        rc = fcntl (conn->sok, F_SETFL, rc | O_NONBLOCK);
#elif defined(HAVE_IOCTLSOCKET)
    origip = 1;
    rc = ioctlsocket (conn->sok, FIONBIO, &origip);
#endif
    if (rc == -1)
    {
        close (conn->sok);
        conn->sok = -1;
        d->d_errno = errno;
        DebugH (DEB_TCP, "conn %p socket %d err %d %s", conn, conn->sok, errno, strerror (errno));
        d->err = IO_NO_NONBLOCK;
        s_repl (&d->lasterr, strerror (rc));
        return 0;
    }
    return 1;
}

/*
 * Connect to conn->server or conn->ip
 */
static void io_tcp_open (Connection *conn, Dispatcher *d)
{
    int rc, rce;
    socklen_t length;
    struct sockaddr_in sin;
    struct hostent *host;
    
    d->flags = FLAG_CONNECTING;
    d->err = IO_OK;
    
    if (!(conn->server || conn->ip))
    {
        d->d_errno = errno;
        d->err = IO_NO_PARAM;
        DebugH (DEB_TCP, "conn %p open param err %d %s", conn, errno, strerror (errno));
        s_repl (&d->lasterr, "");
        return;
    }
    
    DebugH (DEB_TCP, "conn %p open", conn);

    if (!io_tcp_makesocket (conn, d))
        return;

    sin.sin_family = AF_INET;
    sin.sin_port = htons (conn->port);
    
    if (conn->server)
        conn->ip = htonl (inet_addr (conn->server));
    if (conn->ip + 1 == 0 && conn->server)
    {
        host = gethostbyname (conn->server);
        if (!host)
        {
            close (conn->sok);
            conn->sok = -1;
            d->d_errno = rc = h_errno;
            d->err = IO_NO_HOSTNAME;
            s_repl (&d->lasterr, hstrerror (rc));
            return;
        }
        sin.sin_addr = *((struct in_addr *) host->h_addr);
        conn->ip = ntohl (sin.sin_addr.s_addr);
    }
    sin.sin_addr.s_addr = htonl (conn->ip);
    
    rc = connect (conn->sok, (struct sockaddr *) &sin, sizeof (struct sockaddr));
    rce = rc < 0 ? errno : 0;

    length = sizeof (struct sockaddr);
    getsockname (conn->sok, (struct sockaddr *) &sin, &length);
    conn->our_local_ip = ntohl (sin.sin_addr.s_addr);
    if (conn->serv && conn->serv->oscar_dc && (conn->serv->type == TYPE_SERVER))
        conn->serv->oscar_dc->our_local_ip = conn->our_local_ip;

    if (rc >= 0)
    {
        d->flags = FLAG_CONNECTED;
        DebugH (DEB_TCP, "conn %p open ok", conn);
        conn->connect |= CONNECT_SELECT_R | CONNECT_SELECT_A;
        conn->connect &= ~CONNECT_SELECT_W & ~CONNECT_SELECT_X;
        return;
    }

#ifdef __AMIGA__
    if (rce == EINPROGRESS || rce == 115) /* UAE sucks */
#elif defined(EINPROGRESS)
    if (rce == EINPROGRESS)
#elif defined(WSAEINPROGRESS)
    if (!rce || rce == WSAEINPROGRESS)
#else
    rce = 1;
    if (0)
#endif
    {
        QueueEnqueueData (conn, QUEUE_CON_TIMEOUT, 0,
                          time (NULL) + 10, NULL,
                          conn->cont, NULL, &io_tcp_to);
        conn->connect |= CONNECT_SELECT_W | CONNECT_SELECT_X;
        conn->connect &= ~CONNECT_SELECT_R & ~CONNECT_SELECT_A;
        DebugH (DEB_TCP, "conn %p connecting", conn);
        d->flags = FLAG_CONNECTING;
        return;
    }
    else
    {
        close (conn->sok);
        conn->sok = -1;
        errno = rce;
        d->err = IO_NO_CONN;
        d->d_errno = errno;
        s_repl (&d->lasterr, strerror (rc));
        return;
    }
}

/*
 * Connect to listen
 */
static void io_listen_tcp_open (Connection *conn, Dispatcher *d)
{
    struct sockaddr_in sin;
    socklen_t length;
    int rc, i;

    d->flags = FLAG_CONNECTING;
    d->err = IO_OK;
    
    if (!io_tcp_makesocket (conn, d))
        return;

    sin.sin_family = AF_INET;
    sin.sin_port = htons (conn->port);
    sin.sin_addr.s_addr = INADDR_ANY;

    if (bind (conn->sok, (struct sockaddr*)&sin, sizeof (struct sockaddr)) < 0)
    {
#if defined(EADDRINUSE)
        i = 0;
        while ((rc = errno) == EADDRINUSE && conn->port  &&++i < 100)
        {
            rc = 0;
            sin.sin_port = htons (++conn->port);
            if (bind (conn->sok, (struct sockaddr*)&sin, sizeof (struct sockaddr)) == 0)
                break;
            rc = errno;
        }
#endif
        if (rc)
        {
            close (conn->sok);
            conn->sok = -1;
            d->d_errno = errno;
            d->err = IO_NO_BIND;
            s_repl (&d->lasterr, strerror (rc));
            return;
        }
    }

    DebugH (DEB_TCP, "conn %p bind ok", conn);

    if (listen (conn->sok, BACKLOG) < 0)
    {
        close (conn->sok);
        conn->sok = -1;
        d->d_errno = errno;
        d->err = IO_NO_LISTEN;
        s_repl (&d->lasterr, strerror (rc));
        return;
    }
    DebugH (DEB_TCP, "conn %p listen ok", conn);

    length = sizeof (struct sockaddr);
    getsockname (conn->sok, (struct sockaddr *) &sin, &length);
    conn->port = ntohs (sin.sin_port);
    s_repl (&conn->server, "localhost");
    d->flags = FLAG_CONNECTED;
}

static int io_is_select (Connection *conn, int i)
{
    int rc;
    fd_set fds;
    struct timeval tv;
    
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO (&fds);
    FD_SET (conn->sok, &fds);
    rc = select (conn->sok + 1, i ? NULL : &fds, i ? &fds : NULL, NULL, &tv);
    if (rc < 0 && (errno == EAGAIN || errno == EINTR))
        return 0;
    if (rc < 0)
        return rc;
    if (!FD_ISSET  (conn->sok, &fds))
        return 0;
    return 1;
}

static io_err_t io_tcp_connecting (Connection *conn, Dispatcher *d)
{
    if (d->flags == FLAG_CONNECTED)
    {
        DebugH (DEB_TCP, "conn %p connecting connected", conn);
        d->flags = FLAG_OPEN;
        conn->connect |= CONNECT_SELECT_R;
        conn->connect &= ~CONNECT_SELECT_W & ~CONNECT_SELECT_X & ~CONNECT_SELECT_A;
        EventD (QueueDequeue (conn, QUEUE_CON_TIMEOUT, 0));
        return IO_CONNECTED;
    }

    if (d->flags == FLAG_CLOSED)
    {
        DebugH (DEB_TCP, "conn %p connecting closed", conn);
        return IO_CLOSED;
    }
    
    if (d->flags == FLAG_TIMEOUT)
    {
        DebugH (DEB_TCP, "conn %p connecting timeout", conn);
        close (conn->sok);
        conn->sok = -1;
        d->flags = FLAG_CLOSED;
#if defined (ETIMEDOUT)
        s_repl (&d->lasterr, strerror (ETIMEDOUT));
#else
        s_repl (&d->lasterr, "connection timed out");
#endif
        return IO_CONN_TO;
    }
    
    if (d->flags == FLAG_CONNECTING)
    {
        int rc;
        socklen_t length;

        if (d->err)
        {
            DebugH (DEB_TCP, "conn %p connecting err %d %s", conn, d->d_errno, strerror (d->d_errno));
            d->flags = FLAG_CLOSED;
            errno = d->d_errno;
            return d->err;
        }

        rc = io_is_select (conn, 1);
        if (rc == 0)
        {
            DebugH (DEB_TCP, "conn %p connecting no select", conn);
            return IO_OK;
        }
        if (rc < 0)
        {
            d->flags = FLAG_CLOSED;
            d->d_errno  = errno;
            s_repl (&d->lasterr, strerror (errno));
            DebugH (DEB_TCP, "conn %p connecting err %d %s", conn, d->d_errno, strerror (d->d_errno));
            errno = d->d_errno;
            return IO_RW;
        }
        
        /* not reached  for listening connection  */
        length = sizeof (int);
#ifdef SO_ERROR
        if (getsockopt (conn->sok, SOL_SOCKET, SO_ERROR, (void *)&rc, &length) < 0)
#endif
            rc = errno;
        if (rc)
        {
            close (conn->sok);
            conn->sok = -1;
            d->flags = FLAG_CLOSED;
            s_repl (&d->lasterr, strerror (rc));
            DebugH (DEB_TCP, "conn %p connecting err %d %s", conn, rc, d->lasterr);
            return IO_NO_CONN;
        }
        d->flags = FLAG_OPEN;
        DebugH (DEB_TCP, "conn %p connecting now connected", conn);
        EventD (QueueDequeue (conn, QUEUE_CON_TIMEOUT, 0));
        conn->connect |= CONNECT_SELECT_R;
        conn->connect &= ~CONNECT_SELECT_W & ~CONNECT_SELECT_X & ~CONNECT_SELECT_A;
        if (d->outlen)
        {
            io_err_t rce = io_tcp_write (conn, d, NULL, 0);
            if (rce != IO_OK)
                return rce;
        }
        return IO_CONNECTED;
    }
    assert(0);
}

static int io_listen_tcp_accept (Connection *conn, Dispatcher *d, Connection *newconn)
{
    struct sockaddr_in sin;
    socklen_t length;
    int rc, sok;

    length = sizeof (sin);
    sok = accept (conn->sok, (struct sockaddr *)&sin, &length);

    if (sok <= 0)
        return IO_OK;

    DebugH (DEB_TCP, "conn %p listen got %d", conn, sok);

#if HAVE_FCNTL
    rc = fcntl (sok, F_GETFL, 0);
    if (rc != -1)
        rc = fcntl (sok, F_SETFL, rc | O_NONBLOCK);
#elif HAVE_IOCTLSOCKET
    {
        int origip = 1;
        rc = ioctlsocket (sok, FIONBIO, &origip);
    }
#endif
    if (rc == -1)
    {
        close (sok);
        d->d_errno = errno;
        d->err = IO_NO_NONBLOCK;
        s_repl (&d->lasterr, strerror (errno));
        DebugH (DEB_TCP, "conn %p listen got %d err %d %s", conn, sok, d->d_errno, d->lasterr);
        return IO_NO_NONBLOCK;
    }

    assert (newconn);
    newconn->connect |= CONNECT_SELECT_R;
    newconn->connect &= ~CONNECT_SELECT_W & ~CONNECT_SELECT_X & ~CONNECT_SELECT_A;
    newconn->dispatcher = calloc (1, sizeof (Dispatcher));
    if (!newconn->dispatcher)
    {
        close (sok);
        d->d_errno = errno;
        d->err = IO_NO_NONBLOCK;
        s_repl (&d->lasterr, strerror (errno));
        DebugH (DEB_TCP, "conn %p listen got %d err %d %s", conn, sok, d->d_errno, d->lasterr);
        return IO_NO_MEM;
    }
    newconn->dispatcher->funcs = &io_tcp_func;
    newconn->port = ntohs (sin.sin_port);
    s_repl (&newconn->server, "local??host");
    d->flags = FLAG_OPEN;
    DebugH (DEB_TCP, "conn %p listen got %d for %p", conn, sok, newconn);
    return sok;
}

static int io_tcp_read (Connection *conn, Dispatcher *d, char *buf, size_t count)
{
    io_err_t rce;
    int rc;

    DebugH (DEB_TCP, "conn %p read", conn);
    
    if (d->outlen)
    {
        rce = io_tcp_write (conn, d, NULL, 0);
        if (rce != IO_OK)
            return rce;
        DebugH (DEB_TCP, "conn %p write from read done", conn);
    }
    rc = io_is_select (conn, 0);
    if (rc < 0)
    {
        s_repl (&d->lasterr, strerror (errno));
        DebugH (DEB_TCP, "conn %p read err %d %d %s", conn, rc, errno, strerror (errno));
        return IO_RW;
    }
    if (rc == 0)
    {
        DebugH (DEB_TCP, "conn %p read no select", conn);
        conn->connect |= CONNECT_SELECT_R;
        return IO_OK;
    }
#if defined(SIGPIPE)
    signal (SIGPIPE, SIG_IGN);
#endif
    rc = sockread (conn->sok, buf, count);
    if (rc > 0)
    {
        DebugH (DEB_TCP, "conn %p read got %d of %d bytes\n%s", conn, rc, count, conn->dispatcher == d ? s_dump (buf, rc) : "");
        return rc;
    }
    if (rc == 0)
    {
        DebugH (DEB_TCP, "conn %p read closed", conn);
        return IO_CLOSED;
    }
    if (errno == EAGAIN || errno == EINTR)
    {
        conn->connect |= CONNECT_SELECT_R;
        DebugH (DEB_TCP, "conn %p read again", conn);
        return IO_OK;
    }
    s_repl (&d->lasterr, strerror (errno));
    DebugH (DEB_TCP, "conn %p read err %d %s", conn, errno, d->lasterr);
    return IO_RW;
}

static io_err_t io_tcp_write (Connection *conn, Dispatcher *d, const char *buf, size_t len)
{
    int rc;
    
    DebugH (DEB_TCP, "conn %p write", conn);
    
    if (d->outlen)
    {
        rc = sockwrite (conn->sok, d->outbuf, d->outlen);
        if (rc < 0)
        {
            DebugH (DEB_TCP, "conn %p write buf err %d %s", conn, errno, strerror (errno));
            s_repl (&d->lasterr, strerror (errno));
            return IO_RW;
        }
        if (rc >= d->outlen)
        {
            d->outlen = 0;
            free (d->outbuf);
            d->outbuf = NULL;
            conn->connect &= ~CONNECT_SELECT_W;
            DebugH (DEB_TCP, "conn %p write buf done %d %d", conn, rc, d->outlen);
        }
        else if (rc == 0)
        {
            DebugH (DEB_TCP, "conn %p write buf closed", conn);
            return IO_CLOSED;
        }
        else
        {
            memmove (d->outbuf, d->outbuf + rc, d->outlen - rc);
            DebugH (DEB_TCP, "conn %p write buf done %d of %d", conn, rc, d->outlen);
            d->outlen -= rc;
            return io_any_appendbuf (conn, d, buf, len);
        }
    }
    if (len > 0)
    {
#if defined(SIGPIPE)
        signal (SIGPIPE, SIG_IGN);
#endif
        rc = sockwrite (conn->sok, buf, len);
        if (rc < 0)
        {
            DebugH (DEB_TCP, "conn %p write err %d %s", conn, errno, strerror (errno));
            s_repl (&d->lasterr, strerror (errno));
            return IO_RW;
        }
        if (rc == 0)
        {
            DebugH (DEB_TCP, "conn %p write closed", conn);
            return IO_CLOSED;
        }
        DebugH (DEB_TCP, "conn %p write %d of %d.\n%s", conn, rc, len, conn->dispatcher == d ? s_dump (buf, len) : "");
        len -= rc;
        buf += rc;
    }
    else
    {
        DebugH (DEB_TCP, "conn %p write none", conn);
        conn->connect &= ~CONNECT_SELECT_W;
    }
    if (len <= 0)
        return IO_OK;
    return io_any_appendbuf (conn, d, buf, len);
}

static void io_tcp_close (Connection *conn, Dispatcher *d)
{
    if (conn->sok >= 0)
    {
        close (conn->sok);
        conn->sok = -1;
    }
    if (conn->dispatcher == d)
        conn->dispatcher = NULL;
    if (d)
    {
        if (d->outlen)
            free (d->outbuf);
        EventD (QueueDequeue (conn, QUEUE_CON_TIMEOUT, 0));
        free (d);
    }
}
