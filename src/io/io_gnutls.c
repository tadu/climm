/*
 * TLS Diffie Hellmann extension.
 *
 * climm TLS extension Copyright (C) © 2003-2007 Roman Hoog Antink
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
 * $Id: util_ssl.c 2524 2008-03-08 21:08:36Z kuhlmann $
 */

#include "climm.h"
#include <assert.h>

#include "preferences.h"
#include "conv.h"
#include "util_ssl.h"
#include "util_str.h"
#include "util_ui.h"
#include "oscar_dc.h"
#include "connection.h"
#include "contact.h"
#include "packet.h"
#include "io/io_gnutls.h"
#include <errno.h>

io_gnutls_err_t io_gnutls_init_ok = IO_GNUTLS_UNINIT;

#if ENABLE_GNUTLS

static void            io_gnutls_open  (Connection *c, Dispatcher *d, char is_client);
static int             io_gnutls_read  (Connection *c, Dispatcher *d, char *buf, size_t count);
static io_err_t        io_gnutls_write (Connection *c, Dispatcher *d, const char *buf, size_t count);
static void            io_gnutls_close (Connection *c, Dispatcher *d);
static char           *io_gnutls_err   (Connection *c, Dispatcher *d);
static io_gnutls_err_t io_gnutls_seterr (io_gnutls_err_t err, int gnutlserr, const char *msg);
static void            io_gnutls_setconnerr (Dispatcher *d, io_gnutls_err_t err, int gnutlserr, const char *msg);
static char           *io_gnutls_lasterr = NULL;

#include <gnutls/gnutls.h>
#include <gcrypt.h>

static gnutls_anon_client_credentials client_cred;
static gnutls_anon_server_credentials server_cred;
static gnutls_dh_params dh_parm;

enum io_gnutls_dispatcher_flags {
    FLAG_OK,
    FLAG_TIMEOUT
};

static Conn_Func io_gnutls_func = {
    NULL,
    &io_gnutls_read,
    &io_gnutls_write,
    &io_gnutls_close,
    &io_gnutls_err
};

io_gnutls_err_t IOGnuTLSSupported (void)
{
    int ret;

    if (io_gnutls_init_ok != IO_GNUTLS_UNINIT)
        return io_gnutls_init_ok;

    io_gnutls_init_ok = IO_GNUTLS_OK;
    
    if (!libgnutls_is_present)
        return io_gnutls_init_ok = IO_GNUTLS_NOLIB;

    if ((ret = gnutls_global_init ()))
        return io_gnutls_seterr (IO_GNUTLS_INIT, ret, "gnutls_global_init");

    if ((ret = gnutls_anon_allocate_client_credentials (&client_cred)))
        return io_gnutls_seterr (IO_GNUTLS_INIT, ret, "allocate_credentials (client)");

    if ((ret = gnutls_anon_allocate_server_credentials (&server_cred)))
        return io_gnutls_seterr (IO_GNUTLS_INIT, ret, "allocate_credentials (server)");

    if ((ret = gnutls_dh_params_init (&dh_parm)))
        return io_gnutls_seterr (IO_GNUTLS_INIT, ret, "DH param init (server)");

#if HAVE_DH_GENPARAM2
    if (libgnutls_symbol_is_present ("gnutls_dh_params_generate2"))
    {
        if ((ret = gnutls_dh_params_generate2 (dh_parm, DH_OFFER_BITS)))
            return io_gnutls_seterr (IO_GNUTLS_INIT, ret, "DH param generate2 (server)");
    } else
#endif
#if !HAVE_DH_GENPARAM2 || ENABLE_AUTOPACKAGE
    {
        gnutls_datum p1, p2;
        if ((ret = gnutls_dh_params_generate (&p1, &p2, DH_OFFER_BITS)))
            return io_gnutls_seterr (IO_GNUTLS_INIT, ret, "DH param generate (server)");
        ret = gnutls_dh_params_set (dh_parm, p1, p2, DH_OFFER_BITS);
        free (p1.data);
        free (p2.data);
        if (ret)
            return io_gnutls_seterr (IO_GNUTLS_INIT, ret, "DH param set (server)");
    }
#endif

    gnutls_anon_set_server_dh_params (server_cred, dh_parm);

    io_gnutls_init_ok = IO_GNUTLS_OK;
    return IO_GNUTLS_OK;
}

io_gnutls_err_t IOGnuTLSOpen (Connection *conn, char is_client)
{
    Dispatcher *d = calloc (1, sizeof (Dispatcher));
    if (!d)
        return IO_GNUTLS_NOMEM;

    if (io_gnutls_init_ok != IO_GNUTLS_OK)
        return IO_GNUTLS_NOLIB;

    conn->connect |= CONNECT_SELECT_A;
    d->next = conn->dispatcher;
    conn->dispatcher = d;
    d->funcs = &io_gnutls_func;
    d->conn = conn;
    
    io_gnutls_open (conn, d, is_client);
    return IO_GNUTLS_OK;
}

const char *IOGnuTLSInitError ()
{
    return io_gnutls_lasterr;
}

static io_gnutls_err_t io_gnutls_seterr (io_gnutls_err_t err, int gnutlserr, const char *msg)
{
    io_gnutls_init_ok = err;
    s_repl (&io_gnutls_lasterr, s_sprintf ("%s [%d] %s [%d]", msg, err, gnutls_strerror (gnutlserr), gnutlserr));
    return io_gnutls_init_ok;
}

static void io_gnutls_setconnerr (Dispatcher *d, io_gnutls_err_t err, int gnutlserr, const char *msg)
{
    d->gnutls_err = err;
    d->d_errno = gnutlserr;
    DebugH (DEB_SSL, "setting error [connect=%x ssl=%d gnutlserr=%d]", d->conn->connect, d->conn->ssl_status, d->gnutls_err);
    s_repl (&d->lasterr, s_sprintf ("%s [%d] %s [%d]", msg, err, gnutls_strerror (gnutlserr), gnutlserr));
}

static char *io_gnutls_err (Connection *conn, Dispatcher *d)
{
    return d->lasterr;
}

static ssize_t io_gnutls_pull (gnutls_transport_ptr_t user_data, void *buf, size_t len)
{
    Dispatcher *d = (Dispatcher *) user_data;
    int rc = d->next->funcs->f_read (d->conn, d, buf, len);
    if      (rc == IO_CLOSED)  return 0;
    else if (rc == IO_OK)      { errno = EAGAIN; return -1; }
    else if (rc < 0)           return -1;
    else                       return rc;
}

static ssize_t io_gnutls_push (gnutls_transport_ptr_t user_data, const void *buf, size_t len)
{
    Dispatcher *d = (Dispatcher *) user_data;
    int rc = d->next->funcs->f_write (d->conn, d, buf, len);
    if      (rc == IO_CLOSED)  return 0;
    else if (rc == IO_OK)      { errno = EAGAIN; return -1; }
    else if (rc < 0)           return -1;
    else                       return rc;
}

/*
 */
static void io_gnutls_open (Connection *conn, Dispatcher *d, char is_client)
{
    int ret;
    int kx_prio[2] = { GNUTLS_KX_ANON_DH, 0 };
    
    DebugH (DEB_SSL, "ssl_connect");

    conn->ssl_status = SSL_STATUS_HANDSHAKE;
    ret = gnutls_init (&d->ssl, is_client ? GNUTLS_CLIENT : GNUTLS_SERVER);
    if (ret)
        return io_gnutls_setconnerr (d, IO_GNUTLS_INIT, ret, is_client ? "failed init (client)" : "failend init (server)");
        
    gnutls_set_default_priority (d->ssl);
    gnutls_kx_set_priority (d->ssl, kx_prio);

    if (is_client)
        ret = gnutls_credentials_set (d->ssl, GNUTLS_CRD_ANON, client_cred);
    else
        ret = gnutls_credentials_set (d->ssl, GNUTLS_CRD_ANON, server_cred);
    if (ret)
        return io_gnutls_setconnerr (d, IO_GNUTLS_INIT, ret, is_client ? "credentials_set (client)" : "credentials_set (server)");

    if (is_client)
        /* reduce minimal prime bits expected for licq interoperability */
        gnutls_dh_set_prime_bits (d->ssl, DH_EXPECT_BITS);

    gnutls_transport_set_ptr (d->ssl, d);
    gnutls_transport_set_pull_function (d->ssl, &io_gnutls_pull);
    gnutls_transport_set_push_function (d->ssl, &io_gnutls_push);
}

static io_err_t io_gnutls_connecting (Connection *conn, Dispatcher *d)
{
    DebugH (DEB_SSL, "connecting [connect=%x ssl=%d gnutlserr=%d]", conn->connect, conn->ssl_status, d->gnutls_err);

    if (conn->connect & CONNECT_SELECT_A && d->gnutls_err == IO_GNUTLS_INIT)
        return IO_RW;

    if (conn->ssl_status == SSL_STATUS_FAILED
        || conn->ssl_status == SSL_STATUS_NA
        || conn->ssl_status == SSL_STATUS_CLOSE
        || conn->ssl_status == SSL_STATUS_INIT
        || conn->ssl_status == SSL_STATUS_REQUEST)
        return IO_CLOSED;
    
    if (conn->ssl_status == SSL_STATUS_HANDSHAKE)
    {
        int ret = 0;
        
        conn->connect &= ~CONNECT_SELECT_A & ~CONNECT_SELECT_R & ~CONNECT_SELECT_W;
        ret = gnutls_handshake (d->ssl);

        if (ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED)
            return IO_OK;

        if (ret)
        {
            io_gnutls_setconnerr (d, IO_GNUTLS_OK, ret, "handshake");
            conn->ssl_status = SSL_STATUS_FAILED;
            return IO_RW;
        }

        DebugH (DEB_SSL, "connected");

        conn->ssl_status = SSL_STATUS_OK;
        return IO_CONNECTED;
    }
    assert(0);
}

static int io_gnutls_read (Connection *conn, Dispatcher *d, char *buf, size_t count)
{
    int rc;

    if (conn->ssl_status != SSL_STATUS_OK)
        return io_gnutls_connecting (conn, d);

    if (conn->connect & CONNECT_SELECT_W)
    {
        rc = io_gnutls_write (conn, d, NULL, 0);
        if (rc < 0)
            return rc;
    }

    rc = gnutls_record_recv (d->ssl, buf, count);
    DebugH (DEB_SSL, "read [connect=%x ssl=%d rc=%d]", conn->connect, conn->ssl_status, rc);
    if (rc < 0)
    {
        io_gnutls_setconnerr (d, IO_GNUTLS_OK, rc, "read");
        return IO_RW;
    }
    return rc;
}

static io_err_t io_gnutls_appendbuf (Connection *conn, Dispatcher *d, const char *buf, size_t count)
{
    char *newbuf;
    conn->connect |= CONNECT_SELECT_W;
    if (d->outlen)
        newbuf = realloc (d->outbuf, d->outlen + count);
    else
        newbuf = malloc (count);
    if (!newbuf)
    {
        s_repl (&d->lasterr, "");
        return IO_NO_MEM;
    }
    DebugH (DEB_SSL, "write saving %d to %d", count, d->outlen);
    memcpy (newbuf + d->outlen, buf, count);
    d->outlen += count;
    d->outbuf = newbuf;
    return IO_OK;
}

static io_err_t io_gnutls_write (Connection *conn, Dispatcher *d, const char *buf, size_t len)
{
    int rc = 0;
    
    if (conn->ssl_status != SSL_STATUS_OK)
        return io_gnutls_connecting (conn, d);

    if (d->outlen)
    {
        rc = gnutls_record_send (d->ssl, d->outbuf, d->outlen);
        DebugH (DEB_SSL, "write old %d [connect=%x ssl=%d rc=%d]", d->outlen, conn->connect, conn->ssl_status, rc);
        if (rc < 0)
        {
            io_gnutls_setconnerr (d, IO_GNUTLS_OK, rc, "write");
            return IO_RW;
        }
        if (rc >= d->outlen)
        {
            d->outlen = 0;
            free (d->outbuf);
            d->outbuf = NULL;
            conn->connect &= ~CONNECT_SELECT_W;
        } else {
            memmove (d->outbuf, d->outbuf + rc, len - rc);
            d->outlen -= rc;
            return io_gnutls_appendbuf (conn, d, buf, len);
        }
    }

    if (len > 0)
    {
        rc = gnutls_record_send (d->ssl, buf, len);
        DebugH (DEB_SSL, "write %d [connect=%x ssl=%d rc=%d]", len, conn->connect, conn->ssl_status, rc);
        if (rc < 0)
        {
            io_gnutls_setconnerr (d, IO_GNUTLS_OK, rc, "write");
            return IO_RW;
        }
        len -= rc;
        buf += rc;
    }
    if (len <= 0)
        return IO_OK;
    return io_gnutls_appendbuf (conn, d, buf, len);
}

static void io_gnutls_close (Connection *conn, Dispatcher *d)
{
    DebugH (DEB_SSL, "close [connect=%x ssl=%d]", conn->connect, conn->ssl_status);

    if (d->next && d->next->funcs && d->next->funcs->f_close)
        d->next->funcs->f_close (conn, d->next);

    if (conn->dispatcher == d)
        conn->dispatcher = NULL;

    if (d)
    {
        if (d->ssl) 
            free (d->ssl);
        if (d->outlen)
            free (d->outbuf);
        EventD (QueueDequeue (conn, QUEUE_CON_TIMEOUT, conn->ip));
        free (d);
    }
    conn->ssl_status = conn->ssl_status == SSL_STATUS_OK ? SSL_STATUS_NA : SSL_STATUS_FAILED;
}

#endif
