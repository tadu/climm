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
#include "conv.h"
#include "util.h"
#include "contact.h"
#include "packet.h"

int UtilIOAccept (Connection *conn, Connection *newc)
{
    return conn->dispatcher->funcs->f_accept (conn, conn->dispatcher, newc);
}

int UtilIORead (Connection *conn, char *buf, size_t count)
{
    return conn->dispatcher->funcs->f_read (conn, conn->dispatcher, buf, count);
}

io_err_t UtilIOWrite (Connection *conn, const char *buf, size_t count)
{
    return conn->dispatcher->funcs->f_write (conn, conn->dispatcher, buf, count);
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

#define CONNS_FAIL_RC(s) { int rc = errno;            \
                           const char *t = s_sprintf ("%s: %s (%d).", s, strerror (rc), rc); \
                           rl_print (i18n (1949, "failed:\n"));  \
                           rl_printf ("%s [%d]\n", t, __LINE__);  \
                           if (conn->sok > 0)             \
                               sockclose (conn->sok);      \
                           conn->sok = -1;                  \
                           conn->connect = 0;                \
                           return; }

#if ENABLE_REMOTECONTROL
void UtilIOConnectF (Connection *conn)
{
    int rc;
    
    if (!conn->server)
        return;

    unlink (conn->server);
    if (mkfifo (conn->server, 0600) < 0)
        CONNS_FAIL_RC (i18n (2226, "Couldn't create FIFO"));

#if defined(O_NONBLOCK)
    if ((conn->sok = open (conn->server, O_RDWR /*ONLY*/ | O_NONBLOCK)) < 0)
#else
    if ((conn->sok = open (conn->server, O_RDWR)) < 0)
#endif
        CONNS_FAIL_RC (i18n (2227, "Couldn't open FIFO"));

#if HAVE_FCNTL
    rc = fcntl (conn->sok, F_GETFL, 0);
    if (rc != -1)
        rc = fcntl (conn->sok, F_SETFL, rc | O_NONBLOCK);
#elif defined(HAVE_IOCTLSOCKET)
    origip = 1;
    rc = ioctlsocket(conn->sok, FIONBIO, &origip);
#endif
    if (rc == -1)
        CONNS_FAIL_RC (i18n (2228, "Couldn't set FIFO nonblocking"));

    if (rl_pos () > 0)
        rl_print (i18n (1634, "ok.\n"));

    conn->connect = CONNECT_OK;
}
#endif

int UtilIOError (Connection *conn)
{
    int rc;
    socklen_t length;

    length = sizeof (int);
#ifdef SO_ERROR
    if (getsockopt (conn->sok, SOL_SOCKET, SO_ERROR, (void *)&rc, &length) < 0)
#endif
        rc = errno;

    return rc;
}

#ifndef ECONNRESET
#define ECONNRESET 0x424242
#endif

void UtilIOShowDisconnect (Connection *conn, io_err_t rc)
{
    switch (rc) {
        case IO_CLOSED:
            if (!errno)
                errno = ECONNRESET;
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
            break;
        default:
            assert (0);
    }
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

        case IO_RW:
        case IO_CLOSED:
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
        default:
            assert (0);
    }
}

#if ENABLE_REMOTECONTROL
/*
 * Receives a line from a FIFO socket.
 */
Packet *UtilIOReceiveF (Connection *conn)
{
    int rc;
    Packet *pak;
    UBYTE *end, *beg;
    
    if (!(conn->connect & CONNECT_MASK))
        return NULL;
    
    if (!(pak = conn->incoming))
        conn->incoming = pak = PacketC ();
    
    while (1)
    {
        errno = 0;
#if defined(SIGPIPE)
        signal (SIGPIPE, SIG_IGN);
#endif
        rc = sockread (conn->sok, pak->data + pak->len, sizeof (pak->data) - pak->len);
        if (rc <= 0)
        {
            rc = errno;
            if (rc == EAGAIN)
                rc = 0;
            if (rc)
                break;
        }
        pak->len += rc;
        pak->data[pak->len] = '\0';
        if (!(beg = end = (UBYTE *)strpbrk ((const char *)pak->data, "\r\n")))
            return NULL;
        while (*end && strchr ("\r\n\t ", *end))
            end++;
        *beg = '\0';
        if (*end)
        {
            conn->incoming = PacketC ();
            conn->incoming->len = pak->len - (end - pak->data);
            memcpy (conn->incoming->data, end, conn->incoming->len);
        }
        else
            conn->incoming = NULL;
        pak->len = beg - pak->data;
        return pak;
    }

    PacketD (pak);
    sockclose (conn->sok);
    conn->sok = -1;
    conn->incoming = NULL;

    if (prG->verbose)
    {
        Contact *cont;
        if ((cont = conn->cont))
        {
            rl_log_for (cont->nick, COLCONTACT);
            rl_printf (i18n (1878, "Error while reading from socket: %s (%d, %d)\n"), strerror (rc), rc, rc);
        }
    }
    conn->connect = 0;
    return NULL;
}
#endif

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
