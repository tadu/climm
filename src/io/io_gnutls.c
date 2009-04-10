/*
 * TLS Diffie Hellmann extension. (GnuTLS part)
 *
 * climm TLS extension Copyright (C) © 2003-2007 Roman Hoog Antink
 * Reorganized for new i/o schema by Rüdiger Kuhlmann
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
#include <assert.h>

#include "preferences.h"
#include "conv.h"
#include "util_str.h"
#include "util_ui.h"
#include "oscar_dc.h"
#include "connection.h"
#include "contact.h"
#include "packet.h"
#include "io/io_private.h"
#include "io/io_gnutls.h"
#include <errno.h>

io_ssl_err_t io_gnutls_init_ok = IO_SSL_UNINIT;

/* Number of bits climm uses for Diffie-Hellman key exchange of
 * incoming direct connections. You may change this value.
 */
#define DH_OFFER_BITS       768
/* Number of bits climm enforces as a minimum for DH of outgoing
 * direct connections.
 * This value must be less than or equal to 512 in order to work with licq.
 */
#define DH_EXPECT_BITS      512

#if ENABLE_GNUTLS

static void         io_gnutls_open  (Connection *c, Dispatcher *d, char is_client);
static io_err_t     io_gnutls_connecting (Connection *conn, Dispatcher *d);
static int          io_gnutls_read  (Connection *c, Dispatcher *d, char *buf, size_t count);
static io_err_t     io_gnutls_write (Connection *c, Dispatcher *d, const char *buf, size_t count);
static void         io_gnutls_close (Connection *c, Dispatcher *d);
static const char  *io_gnutls_err   (Connection *c, Dispatcher *d);
static io_ssl_err_t io_gnutls_seterr (io_ssl_err_t err, int gnutlserr, const char *msg);
static void         io_gnutls_setconnerr (Dispatcher *d, io_ssl_err_t err, int gnutlserr, const char *msg);
static char        *io_gnutls_lasterr = NULL;

#include <gnutls/gnutls.h>
#include <gcrypt.h>

static gnutls_anon_client_credentials client_cred;
static gnutls_anon_server_credentials server_cred;
static gnutls_dh_params dh_parm;

static Conn_Func io_gnutls_func = {
    NULL,
    &io_gnutls_connecting,
    &io_gnutls_read,
    &io_gnutls_write,
    &io_gnutls_close,
    &io_gnutls_err
};

io_ssl_err_t IOGnuTLSSupported (void)
{
    int ret;

    if (io_gnutls_init_ok != IO_SSL_UNINIT)
        return io_gnutls_init_ok;

    io_gnutls_init_ok = IO_SSL_OK;
    
    if (!libgnutls_is_present)
        return io_gnutls_init_ok = IO_SSL_NOLIB;

    if ((ret = gnutls_global_init ()))
        return io_gnutls_seterr (IO_SSL_INIT, ret, "gnutls_global_init");

    if ((ret = gnutls_anon_allocate_client_credentials (&client_cred)))
        return io_gnutls_seterr (IO_SSL_INIT, ret, "allocate_credentials (client)");

    if ((ret = gnutls_anon_allocate_server_credentials (&server_cred)))
        return io_gnutls_seterr (IO_SSL_INIT, ret, "allocate_credentials (server)");

    if ((ret = gnutls_dh_params_init (&dh_parm)))
        return io_gnutls_seterr (IO_SSL_INIT, ret, "DH param init (server)");

#if HAVE_DH_GENPARAM2
    if (libgnutls_symbol_is_present ("gnutls_dh_params_generate2"))
    {
        if ((ret = gnutls_dh_params_generate2 (dh_parm, DH_OFFER_BITS)))
            return io_gnutls_seterr (IO_SSL_INIT, ret, "DH param generate2 (server)");
    }
#if defined(ENABLE_AUTOPACKAGE)
    else
#endif
#endif
#if !HAVE_DH_GENPARAM2 || ENABLE_AUTOPACKAGE
    {
        gnutls_datum p1, p2;
        if ((ret = gnutls_dh_params_generate (&p1, &p2, DH_OFFER_BITS)))
            return io_gnutls_seterr (IO_SSL_INIT, ret, "DH param generate (server)");
        ret = gnutls_dh_params_set (dh_parm, p1, p2, DH_OFFER_BITS);
        free (p1.data);
        free (p2.data);
        if (ret)
            return io_gnutls_seterr (IO_SSL_INIT, ret, "DH param set (server)");
    }
#endif

    gnutls_anon_set_server_dh_params (server_cred, dh_parm);

    io_gnutls_init_ok = IO_SSL_OK;
    return IO_SSL_OK;
}

io_ssl_err_t IOGnuTLSOpen (Connection *conn, char is_client)
{
    Dispatcher *d = calloc (1, sizeof (Dispatcher));
    if (!d)
        return IO_SSL_NOMEM;

    if (io_gnutls_init_ok != IO_SSL_OK)
        return IO_SSL_NOLIB;

    conn->connect |= CONNECT_SELECT_A;
    d->next = conn->dispatcher;
    conn->dispatcher = d;
    d->funcs = &io_gnutls_func;
    d->conn = conn;
    
    io_gnutls_open (conn, d, is_client);
    return IO_SSL_OK;
}

const char *IOGnuTLSInitError ()
{
    return io_gnutls_lasterr;
}

static io_ssl_err_t io_gnutls_seterr (io_ssl_err_t err, int gnutlserr, const char *msg)
{
    io_gnutls_init_ok = err;
    s_repl (&io_gnutls_lasterr, s_sprintf ("%s [%d] %s [%d]", msg, err, gnutls_strerror (gnutlserr), gnutlserr));
    return io_gnutls_init_ok;
}

static void io_gnutls_setconnerr (Dispatcher *d, io_ssl_err_t err, int gnutlserr, const char *msg)
{
    d->gnutls_err = err;
    d->d_errno = gnutlserr;
    DebugH (DEB_SSL, "setting error [connect=%x ssl=%d err=%d gnutlserr=%d] %s [%s]", d->conn->connect, d->conn->ssl_status, d->gnutls_err, gnutlserr, msg, gnutls_strerror (gnutlserr));
    s_repl (&d->lasterr, s_sprintf ("%s [%d] %s [%d]", msg, err, gnutls_strerror (gnutlserr), gnutlserr));
}

static const char *io_gnutls_err (Connection *conn, Dispatcher *d)
{
    return d->lasterr;
}

static ssize_t io_gnutls_pull (gnutls_transport_ptr_t user_data, void *buf, size_t len)
{
    Dispatcher *d = (Dispatcher *) user_data;
    int rc = d->next->funcs->f_read (d->conn, d->next, buf, len);
    if      (rc == IO_CLOSED)  return 0;
    else if (rc == IO_OK)        { errno = EAGAIN; return -1; }
    else if (rc == IO_CONNECTED) { errno = EAGAIN; d->conn->connect |= CONNECT_SELECT_A; return -1; }
    else if (rc < 0)           return -1;
    else                       return rc;
}

static ssize_t io_gnutls_push (gnutls_transport_ptr_t user_data, const void *buf, size_t len)
{
    Dispatcher *d = (Dispatcher *) user_data;
    int rc = d->next->funcs->f_write (d->conn, d->next, buf, len);
    if      (rc == IO_CLOSED)  return 0;
    else if (rc == IO_OK || rc == IO_CONNECTED)
                               return len;
    else                       return -1;
}

#if 0
void io_gnutls_log (int level, const char *data)
{
    DebugH (DEB_SSL, "level %d text %s\n", level, data);
}
#endif

/*
 */
static void io_gnutls_open (Connection *conn, Dispatcher *d, char is_client)
{
    int ret;
    DebugH (DEB_SSL, "ssl_connect %d", is_client);

    conn->ssl_status = SSL_STATUS_HANDSHAKE;
    ret = gnutls_init (&d->ssl, is_client ? GNUTLS_CLIENT : GNUTLS_SERVER);
    if (ret)
        return io_gnutls_setconnerr (d, IO_SSL_INIT, ret, is_client ? "failed init (client)" : "failed init (server)");

#if LIBGNUTLS_VERSION_NUMBER >= 0x020400
    if (libgnutls_symbol_is_present ("gnutls_priority_set_direct"))
    {
        ret = gnutls_priority_set_direct (d->ssl, "NORMAL:+ANON-DH", NULL);
        if (ret)
            return io_gnutls_setconnerr (d, IO_SSL_INIT, ret, is_client ? "failed prio (client)" : "failed prio (server)");
    }
#if defined(ENABLE_AUTOPACKAGE)
    else
#endif
#endif
#if LIBGNUTLS_VERSION_NUMBER <= 0x020400 || defined (ENABLE_AUTOPACKAGE)
    {
        const int kx_prio[] = { GNUTLS_KX_RSA, GNUTLS_KX_ANON_DH, 0 };

        ret = gnutls_set_default_priority (d->ssl);
        if (ret)
            return io_gnutls_setconnerr (d, IO_SSL_INIT, ret, is_client ? "failed def prio (client)" : "failed def prio (server)");

        ret = gnutls_kx_set_priority (d->ssl, is_client != 2 ? kx_prio + 1: kx_prio);
        if (ret)
            return io_gnutls_setconnerr (d, IO_SSL_INIT, ret, is_client ? "failed key prio (client)" : "failed key prio (server)");
    }
#endif

    if (is_client == 2)
    {
        gnutls_certificate_allocate_credentials (&d->cred);
        ret = gnutls_credentials_set (d->ssl, GNUTLS_CRD_CERTIFICATE, &d->cred);
    }
    else if (is_client == 1)
        ret = gnutls_credentials_set (d->ssl, GNUTLS_CRD_ANON, client_cred);
    else if (is_client == 0)
        ret = gnutls_credentials_set (d->ssl, GNUTLS_CRD_ANON, server_cred);
    else
        assert (0);
    if (ret)
        return io_gnutls_setconnerr (d, IO_SSL_INIT, ret, is_client ? "credentials_set (client)" : "credentials_set (server)");

    /* reduce minimal prime bits expected for licq interoperability */
    if (!is_client)
        gnutls_dh_set_prime_bits (d->ssl, is_client ? DH_EXPECT_BITS : DH_OFFER_BITS);

    gnutls_transport_set_ptr (d->ssl, d);
    gnutls_transport_set_pull_function (d->ssl, &io_gnutls_pull);
    gnutls_transport_set_push_function (d->ssl, &io_gnutls_push);
    
#if 0
    gnutls_global_set_log_function (io_gnutls_log);
    gnutls_global_set_log_level (10);
#endif
}

static io_err_t io_gnutls_connecting (Connection *conn, Dispatcher *d)
{
    DebugH (DEB_SSL, "connecting [connect=%x ssl=%d gnutlserr=%d]", conn->connect, conn->ssl_status, d->gnutls_err);

    if (conn->connect & CONNECT_SELECT_A && d->gnutls_err == IO_SSL_INIT)
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
            io_gnutls_setconnerr (d, IO_SSL_OK, ret, "handshake");
            conn->ssl_status = SSL_STATUS_FAILED;
            return IO_RW;
        }

        DebugH (DEB_SSL, "connected");

        conn->ssl_status = SSL_STATUS_OK;
        conn->connect |= CONNECT_SELECT_R;
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
    conn->connect &= ~CONNECT_SELECT_A;

    rc = gnutls_record_recv (d->ssl, buf, count);
    DebugH (DEB_SSL, "read [connect=%x ssl=%d rc=%d]", conn->connect, conn->ssl_status, rc);
    if (rc < 0 && rc != GNUTLS_E_AGAIN && rc != GNUTLS_E_INTERRUPTED)
    {
        io_gnutls_setconnerr (d, IO_SSL_OK, rc, "read");
        return IO_RW;
    }
    if (rc < 0)
        return IO_OK;
    if (rc > 0)
        conn->connect |= CONNECT_SELECT_A;
    return rc;
}

static io_err_t io_gnutls_write (Connection *conn, Dispatcher *d, const char *buf, size_t len)
{
    int rc = 0;
    
    if (conn->ssl_status != SSL_STATUS_OK)
        return io_any_appendbuf (conn, d, buf, len);

    if (d->outlen)
    {
        rc = gnutls_record_send (d->ssl, d->outbuf, d->outlen);
        DebugH (DEB_SSL, "write old %d [connect=%x ssl=%d rc=%d]", d->outlen, conn->connect, conn->ssl_status, rc);
        if (rc < 0 && rc != GNUTLS_E_AGAIN && rc != GNUTLS_E_INTERRUPTED)
        {
            io_gnutls_setconnerr (d, IO_SSL_OK, rc, "write");
            return IO_RW;
        }
        if (rc < 0)
            return io_any_appendbuf (conn, d, buf, len);
        if (rc >= d->outlen)
        {
            d->outlen = 0;
            free (d->outbuf);
            d->outbuf = NULL;
            conn->connect &= ~CONNECT_SELECT_W;
        } else {
            memmove (d->outbuf, d->outbuf + rc, len - rc);
            d->outlen -= rc;
            return io_any_appendbuf (conn, d, buf, len);
        }
    }

    if (len > 0)
    {
        rc = gnutls_record_send (d->ssl, buf, len);
        DebugH (DEB_SSL, "write %d [connect=%x ssl=%d rc=%d]", len, conn->connect, conn->ssl_status, rc);
        if (rc < 0 && rc != GNUTLS_E_AGAIN && rc != GNUTLS_E_INTERRUPTED)
        {
            io_gnutls_setconnerr (d, IO_SSL_OK, rc, "write");
            return IO_RW;
        }
        if (rc > 0)
        {
            len -= rc;
            buf += rc;
        }
    }
    if (len <= 0)
        return IO_OK;
    return io_any_appendbuf (conn, d, buf, len);
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
        free (d);
    }
    conn->ssl_status = conn->ssl_status == SSL_STATUS_OK ? SSL_STATUS_NA : SSL_STATUS_FAILED;
    conn->connect = 0;
}

#endif
