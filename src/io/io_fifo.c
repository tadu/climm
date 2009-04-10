/*
 * Basic FIFO I/O layer.
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
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <signal.h>
#include <stdarg.h>
#include <fcntl.h>
#include <assert.h>

#include "connection.h"
#include "io/io_private.h"
#include "io/io_fifo.h"

#if ENABLE_REMOTECONTROL

static void        io_fifo_open  (Connection *c, Dispatcher *d);
static io_err_t    io_fifo_connecting (Connection *conn, Dispatcher *d);
static int         io_fifo_read  (Connection *c, Dispatcher *d, char *buf, size_t count);
static void        io_fifo_close (Connection *c, Dispatcher *d);
static const char *io_fifo_err   (Connection *c, Dispatcher *d);

static Conn_Func io_fifo_func = {
    NULL,
    &io_fifo_connecting,
    &io_fifo_read,
    NULL,
    &io_fifo_close,
    &io_fifo_err
};

void IOConnectFifo (Connection *conn)
{
    assert (conn);
    UtilIOClose (conn);
    conn->connect &= ~CONNECT_SELECT_R & ~CONNECT_SELECT_W & ~CONNECT_SELECT_X;
    conn->connect |= CONNECT_SELECT_A;
    conn->dispatcher = calloc (1, sizeof (Dispatcher));
    if (!conn->dispatcher)
        return;
    conn->dispatcher->funcs = &io_fifo_func;
    io_fifo_open (conn, conn->dispatcher);
}

static const char *io_fifo_err (Connection *conn, Dispatcher *d)
{
     return d->lasterr;
}

static void io_fifo_setconnerr (Dispatcher *d, io_err_t err, const char *msg)
{
    d->d_errno = errno;
    d->err = err;
    s_repl (&d->lasterr, s_sprintf ("%s [%d] %s [%d]", msg, err, strerror (errno), errno));
}

/*
 * Connect to conn->server or conn->ip
 */
static void io_fifo_open (Connection *conn, Dispatcher *d)
{
    int rc;
    
    d->flags = FLAG_CONNECTING;
    d->err = IO_OK;
    
    if (!conn->server)
        return io_fifo_setconnerr (d, IO_NO_PARAM, "");

    unlink (conn->server);
    if (mkfifo (conn->server, 0600) < 0)
        return io_fifo_setconnerr (d, IO_NO_SOCKET, i18n (2226, "Couldn't create FIFO"));

#if defined(O_NONBLOCK)
    if ((conn->sok = open (conn->server, O_RDWR /*ONLY*/ | O_NONBLOCK)) < 0)
#else
    if ((conn->sok = open (conn->server, O_RDWR)) < 0)
#endif
        return io_fifo_setconnerr (d, IO_NO_CONN, i18n (2226, "Couldn't create FIFO"));

#if HAVE_FCNTL
    rc = fcntl (conn->sok, F_GETFL, 0);
    if (rc != -1)
        rc = fcntl (conn->sok, F_SETFL, rc | O_NONBLOCK);
#elif defined(HAVE_IOCTLSOCKET)
    origip = 1;
    rc = ioctlsocket(conn->sok, FIONBIO, &origip);
#endif
    if (rc == -1)
        return io_fifo_setconnerr (d, IO_NO_NONBLOCK, i18n (2228, "Couldn't set FIFO nonblocking"));

    d->flags = FLAG_CONNECTED;
    conn->connect |= CONNECT_SELECT_R | CONNECT_SELECT_A;
    conn->connect &= ~CONNECT_SELECT_W & ~CONNECT_SELECT_X;
}

static io_err_t io_fifo_connecting (Connection *conn, Dispatcher *d)
{
    if (d->flags == FLAG_CONNECTED)
    {
        d->flags = FLAG_OPEN;
        conn->connect |= CONNECT_SELECT_R;
        conn->connect &= ~CONNECT_SELECT_W & ~CONNECT_SELECT_X & ~CONNECT_SELECT_A;
        return IO_CONNECTED;
    }

    if (d->flags == FLAG_CLOSED)
        return IO_CLOSED;

    if (d->flags == FLAG_CONNECTING)
    {
        if (d->err)
        {
            d->flags = FLAG_CLOSED;
            errno = d->d_errno;
            return d->err;
        }
    }
    assert(0);
}

static int io_fifo_read (Connection *conn, Dispatcher *d, char *buf, size_t count)
{
    fd_set fds;
    struct timeval tv;
    int rc;
    
    if (d->flags != FLAG_OPEN)
        return io_fifo_connecting (conn, d);

    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO (&fds);
    FD_SET (conn->sok, &fds);
    rc = select (conn->sok + 1, &fds, NULL, NULL, &tv);
    if (rc < 0 && (errno == EAGAIN || errno == EINTR))
    {
        conn->connect |= CONNECT_SELECT_R;
        return IO_OK;
    }
    if (rc < 0)
    {
        s_repl (&d->lasterr, strerror (errno));
        return IO_RW;
    }
    if (!FD_ISSET  (conn->sok, &fds))
        return IO_OK;
#if defined(SIGPIPE)
    signal (SIGPIPE, SIG_IGN);
#endif
    rc = sockread (conn->sok, buf, count);
    if (rc > 0)
        return rc;
    if (rc == 0)
        return IO_CLOSED;
    if (errno == EAGAIN || errno == EINTR)
    {
        conn->connect |= CONNECT_SELECT_R;
        return IO_OK;
    }
    s_repl (&d->lasterr, strerror (errno));
    return IO_RW;
}

static void io_fifo_close (Connection *conn, Dispatcher *d)
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
        free (d);
    }
}
#endif
