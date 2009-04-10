/*
 * Assorted helper functions for doing I/O.
 *
 * Copyright: This file may be distributed under version 2 of the GPL licence.
 *
 * $Id$
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
#include <fcntl.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <assert.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <signal.h>
#include <stdarg.h>
#include "preferences.h"
#include "util_ui.h"
#include "connection.h"
#include "util_io.h"
#include "io/io_private.h"
#include "io/io_tcp.h"
#include "io/io_socks5.h"
#include "conv.h"
#include "util.h"
#include "contact.h"

void UtilIOConnectTCP (Connection *conn)
{
    if (ConnectionPrefVal (conn->serv, CO_S5USE))
        return IOConnectSocks5 (conn);
    return IOConnectTCP (conn);
}

void UtilIOListenTCP  (Connection *conn)
{
    if (ConnectionPrefVal (conn->serv, CO_S5USE))
        return IOListenSocks5 (conn);
    return IOListenTCP (conn);
}

int UtilIOAccept (Connection *conn, Connection *newc)
{
    Dispatcher *d;
    if (!conn || !conn->dispatcher || !conn->dispatcher->funcs)
        return IO_NO_CONN;
    d = conn->dispatcher;
    if (d->flags != FLAG_OPEN)
        return d->funcs->f_cting (conn, d);
    return d->funcs->f_accept (conn, d, newc);
}

int UtilIORead (Connection *conn, char *buf, size_t count)
{
    Dispatcher *d;
    if (!conn || !conn->dispatcher || !conn->dispatcher->funcs)
        return IO_NO_CONN;
    d = conn->dispatcher;
    if (d->flags != FLAG_OPEN)
        return d->funcs->f_cting (conn, d);
    return d->funcs->f_read (conn, d, buf, count);
}

io_err_t UtilIOWrite (Connection *conn, const char *buf, size_t count)
{
    Dispatcher *d;
    if (!conn || !conn->dispatcher || !conn->dispatcher->funcs)
        return IO_NO_CONN;
    d = conn->dispatcher;
    if (d->flags != FLAG_OPEN)
        return io_any_appendbuf (conn, d, buf, count);
    if (d->funcs->f_write)
        return d->funcs->f_write (conn, d, buf, count);
    return IO_NO_CONN;
}

io_err_t io_any_appendbuf (Connection *conn, Dispatcher *d, const char *buf, size_t count)
{
    char *newbuf;
    conn->connect |= CONNECT_SELECT_W;
    DebugH (DEB_TCP, "conn %p append %d to %d", conn, count, d->outlen);
    if (!count)
        return IO_OK;
    if (d->outlen)
        newbuf = realloc (d->outbuf, d->outlen + count);
    else
        newbuf = malloc (count);
    if (!newbuf)
    {
        s_repl (&d->lasterr, "");
        return IO_NO_MEM;
    }
    memcpy (newbuf + d->outlen, buf, count);
    d->outlen += count;
    d->outbuf = newbuf;
    return IO_OK;
}


void UtilIOClose (Connection *conn)
{
    if (conn && conn->dispatcher && conn->dispatcher->funcs && conn->dispatcher->funcs->f_close)
        conn->dispatcher->funcs->f_close (conn, conn->dispatcher);
    assert (!conn || conn->sok < 0);
    assert (!conn || !conn->dispatcher);
}

const char *UtilIOErr (Connection *conn)
{
    return conn->dispatcher->funcs->f_err (conn, conn->dispatcher);
}

io_ssl_err_t UtilIOSSLSupported (void)
{
    io_ssl_err_t rcgnutls = IOGnuTLSSupported ();
    io_ssl_err_t rcopenssl = rcgnutls == IO_SSL_OK ? IO_SSL_NOLIB : IOOpenSSLSupported ();

    if (rcgnutls == IO_SSL_OK || rcopenssl == IO_SSL_OK)
        return IO_SSL_OK;
    if (rcgnutls != IO_SSL_NOLIB)
    {
        rl_printf (i18n (2374, "SSL error: %s [%d]\n"), IOGnuTLSInitError (), 0);
        rl_printf (i18n (2371, "SSL init failed.\n"));
    }
    if (rcopenssl == IO_SSL_INIT)
    {
        rl_printf (i18n (2374, "SSL error: %s [%d]\n"), IOOpenSSLInitError (), 0);
        rl_printf (i18n (2371, "SSL init failed.\n"));
    }
    else
        rl_printf (i18n (2581, "Install the GnuTLS library and enjoy encrypted connections to peers!\n"));
    return IO_SSL_NOLIB;
}

io_ssl_err_t UtilIOSSLOpen (Connection *conn, char is_client)
{
    if (IOGnuTLSSupported () == IO_SSL_OK)
        return IOGnuTLSOpen (conn, is_client);
    else if (IOOpenSSLSupported () == IO_SSL_OK)
        return IOOpenSSLOpen (conn, is_client);
    else
        return IO_SSL_NOLIB;
}

io_err_t UtilIOShowError (Connection *conn, io_err_t rc)
{
    int e = errno;
    switch (rc) {
        const char *t = NULL;
    
        case IO_CONNECTED: 
            rl_print ("");
            if (prG->verbose || (conn->serv && conn == conn->serv->conn))
                if (rl_pos () > 0)
                     rl_print (i18n (1634, "ok.\n"));
            return IO_CONNECTED;

        case IO_OK:
            return IO_OK;

        case IO_NO_MEM:
        case IO_NO_PARAM:
            assert (0);

        case IO_NO_SOCKET:
            if (1) t = i18n (1638, "Couldn't create socket"); else
        case IO_NO_NONBLOCK:
            if (1) t = i18n (1950, "Couldn't set socket nonblocking"); else
        case IO_NO_HOSTNAME:
            if (1) t = i18n (9999, "Can't find hostname"); else
        case IO_CONN_TO:
            if (1) t = i18n (9999, "Connection timed out"); else
        case IO_NO_CONN:
                   t = i18n (1952, "Couldn't open connection");
            if (prG->verbose || (conn->serv && conn == conn->serv->conn))
            {
                Contact *cont = conn->cont;
                rl_log_for (cont->nick, COLCONTACT);
                rl_printf (i18n (9999, "Opening connection to %s:%s%ld%s "),
                          s_wordquote (conn->server), COLQUOTE, UD2UL (conn->port), COLNONE);
                rl_print (i18n (1949, "failed:\n"));
                rl_printf ("%s [%d]\n",
                    s_sprintf  ("%s: %s (%d).", t, conn->dispatcher->funcs->f_err (conn, conn->dispatcher), e),
                    __LINE__);
            }
            UtilIOClose (conn);
            return IO_RW;
        case IO_CLOSED:
#ifdef ECONNRESET
            if (!errno)
                errno = ECONNRESET;
#endif
        case IO_RW:
            if (prG->verbose || (conn->serv && conn == conn->serv->conn))
            {
                Contact *cont;
                if ((cont = conn->cont))
                {
                    rl_log_for (cont->nick, COLCONTACT);
                    rl_printf (i18n (1878, "Error while reading from socket: %s (%d, %d)\n"), conn->dispatcher->funcs->f_err (conn, conn->dispatcher), rc, errno);
                }
            }
            UtilIOClose (conn);
            return IO_RW;
        default:
            assert (0);
    }
}

/*
 * Read a complete line from a fd.
 *
 * Returned string may not be free()d.
 */
strc_t UtilIOReadline (FILE *fd)
{
    static str_s str;
    char *p;
    
    s_init (&str, "", 256);
    while (1)
    {
        str.txt[str.max - 2] = 0;
        if (!fgets (str.txt + str.len, str.max - str.len, fd))
        {
            str.txt[str.len] = '\0';
            if (!str.len)
                return NULL;
            break;
        }
        str.txt[str.max - 1] = '\0';
        str.len = strlen (str.txt);
        if (!str.txt[str.max - 2])
            break;
        s_blow (&str, 128);
    }
    if ((p = strpbrk (str.txt, "\r\n")))
    {
        *p = 0;
        str.len = strlen (str.txt);
    }
    return &str;
}

static struct timeval tv;
static fd_set fds[3];
static int maxfd;

void UtilIOSelectInit (int sec, int usec)
{
    FD_ZERO (&fds[0]);
    FD_ZERO (&fds[1]);
    FD_ZERO (&fds[2]);
    tv.tv_sec = sec;
    tv.tv_usec = usec;
    maxfd = 0;
}

void UtilIOSelectAdd (FD_T sok, int nr)
{
    FD_SET (sok, &fds[nr & 3]);
    if (sok > maxfd)
        maxfd = sok;
}

BOOL UtilIOSelectIs (FD_T sok, int nr)
{
    return ((nr & READFDS)   && FD_ISSET (sok, &fds[READFDS & 3]))
        || ((nr & WRITEFDS)  && FD_ISSET (sok, &fds[WRITEFDS & 3]))
        || ((nr & EXCEPTFDS) && FD_ISSET (sok, &fds[EXCEPTFDS & 3]));
}

void UtilIOSelect (void)
{
    int res, rc;

    errno = 0;
    res = select (maxfd + 1, &fds[READFDS & 3], &fds[WRITEFDS & 3], &fds[EXCEPTFDS & 3], &tv);
    rc = errno;
    if (res == -1)
    {
        FD_ZERO (&fds[READFDS & 3]);
        FD_ZERO (&fds[WRITEFDS & 3]);
        FD_ZERO (&fds[EXCEPTFDS & 3]);
        if (rc != EINTR && rc != EAGAIN)
        {
            printf (i18n (1849, "Error on select: %s (%d)\n"), strerror (rc), rc);
            assert (0);
        }
    }
}
