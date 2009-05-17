/*
 * SOCKS 5 proxy I/O layer.
 *
 * climm (C) © 2001-2009 Rüdiger Kuhlmann
 *
 * climm is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 dated June, 1991.
 *
 * climm is distributed in the hope that it will be useful, but WITHOUT
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
#include <assert.h>

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include "connection.h"
#include "util_io.h"
#include "io/io_private.h"
#include "io/io_tcp.h"
#include "io/io_socks5.h"

static void        io_socks5_open  (Connection *c, Dispatcher *d);
static io_err_t    io_socks5_connecting (Connection *conn, Dispatcher *d);
static void        io_socks5_close (Connection *c, Dispatcher *d);
static const char *io_socks5_err   (Connection *c, Dispatcher *d);
static int         io_socks5_accept(Connection *c, Dispatcher *d, Connection *cn);

static Conn_Func io_socks5_func = {
    NULL,
    &io_socks5_connecting,
    NULL,
    NULL,
    &io_socks5_close,
    &io_socks5_err
};

static Conn_Func io_listen_socks5_func = {
    &io_socks5_accept,
    &io_socks5_connecting,
    NULL,
    NULL,
    &io_socks5_close,
    &io_socks5_err
};

void IOConnectSocks5 (Connection *conn)
{
    Dispatcher *d;
    assert (conn);
    UtilIOClose (conn);
    d = calloc (1, sizeof (Dispatcher));
    if (!d)
        return;
    d->funcs = &io_socks5_func;
    io_socks5_open (conn, d);
}

void IOListenSocks5 (Connection *conn)
{
    Dispatcher *d;
    assert (conn);
    UtilIOClose (conn);
    d = calloc (1, sizeof (Dispatcher));
    if (!d)
        return;
    d->funcs = &io_listen_socks5_func;
    io_socks5_open (conn, d);
}

static const char *io_socks5_err (Connection *conn, Dispatcher *d)
{
    return d->lasterr;
}

static io_err_t io_socks5_seterr (Dispatcher *d, io_err_t err, const char *msg)
{
    s_repl (&d->lasterr, s_sprintf ("[socks5] %s [%d]", msg, err));
    return err;
}

/*
 * Connect to conn->server or conn->ip
 */
static void io_socks5_open (Connection *conn, Dispatcher *d)
{
    const char *socks5server = ConnectionPrefStr (conn->serv, CO_S5HOST);
    UDWORD      socks5port   = ConnectionPrefVal (conn->serv, CO_S5PORT);
    char *origserver = conn->server;
    UDWORD origport = conn->port, origip = conn->ip;
    
    d->flags = FLAG_CONNECTING;
    d->err = IO_OK;
    
    if (!socks5server || !socks5port)
    {
        d->d_errno = errno;
        d->err = IO_NO_PARAM;
        s_repl (&d->lasterr, "");
        return;
    }

    conn->server = strdup (socks5server);
    conn->port   = socks5port;
    conn->ip     = -1;

    IOConnectTCP (conn);

    free (conn->server);
    conn->server = origserver;
    conn->ip = origip;
    conn->port = origport;

    d->next = conn->dispatcher;
    conn->dispatcher = d;
}

static io_err_t io_socks5_connecting (Connection *conn, Dispatcher *d)
{
    const char *send;
    io_err_t e;
    int len;

    if (d->flags == FLAG_CONNECTING)
    {
        int rc = io_util_read (conn, d->next, NULL, 0);
        s_repl (&d->lasterr, NULL);
        if (rc != IO_CONNECTED)
            return io_socks5_seterr (d, rc, d->next->funcs->f_err (conn,  d->next));
        if (ConnectionPrefVal (conn->serv, CO_S5NAME) && ConnectionPrefVal (conn->serv, CO_S5PASS))
            e = io_util_write (conn, d->next, "\x05\x02\x02\x00", 4);            
        else
            e = io_util_write (conn, d->next, "\x05\x01\x00", 3);
        d->read = 0;
        d->flags = FLAG_METHODS_SENT;
        if (e != IO_OK)
            return io_socks5_seterr (d, e, d->next->funcs->f_err (conn,  d->next));
    }
    if (d->flags == FLAG_METHODS_SENT)
    {
        int rc = io_util_read (conn, d->next, d->buf + d->read, 2 - d->read);
        if (rc < 0)
            return io_socks5_seterr (d, rc, d->next->funcs->f_err (conn,  d->next));
        d->read += rc;
        if (d->read < 2)
            return IO_OK;
        if (d->buf[0] != 5 || !(d->buf[1] == 0 || d->buf[1] == 2))
            return io_socks5_seterr (d, IO_RW, i18n (1601, "[SOCKS] General SOCKS server failure"));
        d->flags = d->buf[1] == 2 ? FLAG_SEND_CRED : FLAG_SEND_REQ;
    }
    if (d->flags == FLAG_SEND_CRED)
    {
        const char *socks5name = ConnectionPrefStr (conn->serv, CO_S5NAME);
        const char *socks5pass = ConnectionPrefStr (conn->serv, CO_S5PASS);
        if (!socks5name || !socks5pass)
            return io_socks5_seterr (d, IO_RW, i18n (1599, "[SOCKS] Authentication method incorrect"));

        send = s_sprintf ("%c%c%s%c%s%n", 1, (char) strlen (socks5name), socks5name, strlen (socks5pass), socks5pass, &len);
        e = io_util_write (conn, d->next, send, len);
        d->flags = FLAG_CRED_SENT;
        d->read = 0;
        if (e != IO_OK)
            return io_socks5_seterr (d, e, d->next->funcs->f_err (conn,  d->next));
    }
    if (d->flags == FLAG_CRED_SENT)
    {
        int rc = io_util_read (conn, d->next, d->buf + d->read, 2 - d->read);
        if (rc < 0)
            return io_socks5_seterr (d, rc, d->next->funcs->f_err (conn,  d->next));
        d->read += rc;
        if (d->read < 2)
            return IO_OK;
        if (d->buf[1])
            return io_socks5_seterr (d, IO_RW, i18n (1600, "[SOCKS] Authorization failure"));
        d->flags = FLAG_SEND_REQ;
    }
    if (d->flags == FLAG_REQ_SENT || d->flags == FLAG_REQ_NOPORT_SENT)
    {
        int rc = io_util_read (conn, d->next, d->buf + d->read, 10 - d->read);
        if (rc < 0)
            return io_socks5_seterr (d, rc, d->next->funcs->f_err (conn,  d->next));
        d->read += rc;
        if (d->read < 10)
            return IO_OK;
        if (d->buf[3] != 1)
            return io_socks5_seterr (d, IO_RW, i18n (1601, "[SOCKS] General SOCKS server failure"));
        if (d->funcs->f_accept && d->buf[1] == 4 && d->flags == FLAG_REQ_SENT)
            d->flags = FLAG_SEND_REQ_NOPORT;
        else
        {
            if (d->buf[1])
                return io_socks5_seterr (d, IO_RW, i18n (1958, "[SOCKS] Connection request refused (%d)"));
            if (d->funcs->f_accept)
            {
                conn->our_outside_ip = ntohl (*(UDWORD *)(&d->buf[4]));
                conn->port = ntohs (*(UWORD *)(&d->buf[8]));
                if (conn->serv && conn->serv->oscar_dc)
                    conn->serv->oscar_dc->our_local_ip = conn->our_outside_ip;
            }
            d->flags = FLAG_OPEN;
            d->read = 0;
            return IO_CONNECTED;
        }
    }
    if (d->flags == FLAG_SEND_REQ || d->flags == FLAG_SEND_REQ_NOPORT)
    {
        d->flags = FLAG_REQ_SENT;
        if (d->funcs->f_accept)
        {
            if (d->flags == FLAG_SEND_REQ)
                send = s_sprintf ("%c%c%c%c%c%c%c%c%c%c%n", 5, 2, 0, 1, 0, 0, 0, 0,
                                   (char)(conn->port >> 8), (char)(conn->port & 255), &len);
            else
            {
                send = s_sprintf ("%c%c%c%c%c%c%c%c%c%c%n", 5, 2, 0, 1, 0, 0, 0, 0, 0, 0, &len);
                d->flags = FLAG_REQ_NOPORT_SENT;
            }
        }
        else
        {
            if (conn->server)
                send = s_sprintf ("%c%c%c%c%c%s%c%c%n", 5, 1, 0, 3,
                                   (char)strlen (conn->server), conn->server,
                                   (char)(conn->port >> 8), (char)(conn->port & 255), &len);
            else
                send = s_sprintf ("%c%c%c%c%c%c%c%c%c%c%n", 5, 1, 0, 1,
                                   (char)(conn->ip >> 24), (char)(conn->ip >> 16), (char)(conn->ip >> 8), (char)conn->ip,
                                   (char)(conn->port >> 8), (char)(conn->port & 255), &len);
        }
        e = io_util_write (conn, d->next, send, len);
        d->read = 0;
        if (e != IO_OK)
            return io_socks5_seterr (d, e, d->next->funcs->f_err (conn,  d->next));
    }
    return IO_OK;
    assert(0);
}

static int io_socks5_accept (Connection *conn, Dispatcher *d, Connection *newconn)
{
    Dispatcher *nd, *dd;
    int rc;

    rc = io_util_read (conn, d->next, d->buf + d->read, 10 - d->read);
    if (rc < 0)
        return io_socks5_seterr (d, rc, d->next->funcs->f_err (conn,  d->next));
    d->read += rc;
    if (d->read < 10)
        return IO_OK;
    
    if (d->buf[3] != 1)
        return io_socks5_seterr (d, IO_RW, i18n (1601, "[SOCKS] General SOCKS server failure"));
    
    nd = calloc (1, sizeof (Dispatcher));
    if (!nd)
        return IO_NO_MEM;
    
    assert (newconn);
    newconn->sok = conn->sok;
    conn->sok = -1;
    conn->connect = 0;
    dd = conn->dispatcher;
    conn->dispatcher = dd->next;
    io_socks5_open (conn, dd);
    
    newconn->connect |= CONNECT_SELECT_R;
    newconn->connect &= ~CONNECT_SELECT_W & ~CONNECT_SELECT_X & ~CONNECT_SELECT_A;
    newconn->dispatcher = nd;
    nd->flags = FLAG_OPEN;
    nd->funcs = &io_socks5_func;
    newconn->port = conn->port;
    s_repl (&newconn->server, "local??host");
    
    return newconn->sok;
}

static void io_socks5_close (Connection *conn, Dispatcher *d)
{
    if (conn->dispatcher == d)
        conn->dispatcher = NULL;
    if (d)
    {
        d->next->funcs->f_close (conn, d->next);
        free (d);
    }
}
