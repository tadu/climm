/*
 * TLS Diffie Hellmann extension. (OpenSSL part)
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
#include "io/io_openssl.h"

#include <errno.h>

io_ssl_err_t io_openssl_init_ok = IO_SSL_UNINIT;

#if ENABLE_OPENSSL

static void         io_openssl_open  (Connection *c, Dispatcher *d, char is_client);
static io_err_t     io_openssl_connecting (Connection *conn, Dispatcher *d);
static int          io_openssl_read  (Connection *c, Dispatcher *d, char *buf, size_t count);
static io_err_t     io_openssl_write (Connection *c, Dispatcher *d, const char *buf, size_t count);
static void         io_openssl_close (Connection *c, Dispatcher *d);
static const char  *io_openssl_err   (Connection *c, Dispatcher *d);
static io_ssl_err_t io_openssl_seterr (io_ssl_err_t err, int opensslerr, const char *msg);
static void         io_openssl_setconnerr (Dispatcher *d, io_ssl_err_t err, int opensslerr, const char *msg);
static char        *io_openssl_lasterr = NULL;

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/dh.h>
#include <openssl/opensslv.h>
#include <openssl/md5.h>

static SSL_CTX *gSSL_CTX;

static Conn_Func io_openssl_func = {
    NULL,
    &io_openssl_connecting,
    &io_openssl_read,
    &io_openssl_write,
    &io_openssl_close,
    &io_openssl_err
};

/* AUTOGENERATED by openssl dhparam -5 -C */
static DH *get_dh512()
{
    static unsigned char dh512_p[]={
            0xA5,0x82,0x78,0x01,0x4D,0x7A,0x33,0x49,0x2A,0xD2,0x15,0x74,
            0x8E,0x95,0x67,0x26,0xB1,0x5D,0x8A,0x01,0xDA,0x0A,0x08,0x51,
            0x31,0x64,0x71,0xD7,0x38,0x53,0xEA,0xD2,0xCF,0x3D,0xB9,0x26,
            0x78,0x5F,0x75,0xF3,0x90,0xC5,0x63,0x7B,0x9A,0x4F,0x4D,0x03,
            0xAB,0x38,0x8D,0x79,0x58,0x19,0xD8,0x83,0x55,0x90,0xC8,0xEC,
            0xA0,0x8F,0x01,0x2F,
    };
    static unsigned char dh512_g[]={
            0x05,
    };

    DH *dh;

    if ((dh=DH_new()) == NULL) return(NULL);
    dh->p=BN_bin2bn(dh512_p,sizeof(dh512_p),NULL);
    dh->g=BN_bin2bn(dh512_g,sizeof(dh512_g),NULL);
    if ((dh->p == NULL) || (dh->g == NULL))
            { DH_free(dh); return(NULL); }
    return(dh);
}
/* END AUTOGENERATED */

static void openssl_info_callback (SSL *s, int where, int ret)
{
    const char *str;
    int w;

    w = where & ~SSL_ST_MASK;

    if (w & SSL_ST_CONNECT) str = "SSL_connect";
    else if (w & SSL_ST_ACCEPT) str = "SSL_accept";
    else str = "undefined";

    if (where & SSL_CB_LOOP)
    {
        rl_printf ("%s:%s\n", str, SSL_state_string_long (s));
    }
    else if (where & SSL_CB_ALERT)
    {
        str = (where & SSL_CB_READ) ? "read" : "write";
        rl_printf ("SSL3 alert %s:%s:%s\n",
            str,
            SSL_alert_type_string_long (ret),
            SSL_alert_desc_string_long (ret));
    }
    else if (where & SSL_CB_EXIT)
    {
        if (ret == 0)
            rl_printf ("%s:failed in %s\n",
                str, SSL_state_string_long(s));
        else if (ret < 0)
        {
            rl_printf ("%s:%s\n",str,SSL_state_string_long(s));
        }
    }
    else if (where & SSL_CB_ALERT)
    {
        str = (where & SSL_CB_READ) ? "read" : "write";
        rl_printf ("SSL3 alert %s:%s:%s\n",
            str,
            SSL_alert_type_string_long (ret),
            SSL_alert_desc_string_long (ret));
    }
    else if (where & SSL_CB_EXIT)
    {
        if (ret == 0)
            rl_printf ("%s:failed in %s\n",
                str, SSL_state_string_long(s));
        else if (ret < 0)
        {
            rl_printf ("%s:error in %s\n",
                str, SSL_state_string_long(s));
        }
    }
}

io_ssl_err_t IOOpenSSLSupported (void)
{
    int ret;

    if (io_openssl_init_ok >= IO_SSL_OK)
        return io_openssl_init_ok;

    if (!libopenssl_is_present)
        return io_openssl_init_ok = IO_SSL_NOLIB;

    DH *dh;
    SSL_library_init();
    gSSL_CTX = SSL_CTX_new (TLSv1_method ());
#if OPENSSL_VERSION_NUMBER >= 0x00905000L
    SSL_CTX_set_cipher_list (gSSL_CTX, "ADH:@STRENGTH");
#else
    SSL_CTX_set_cipher_list (gSSL_CTX, "ADH");
#endif

    if (prG->verbose & DEB_SSL)
        SSL_CTX_set_info_callback (gSSL_CTX, (void (*)())openssl_info_callback);

    dh = get_dh512 ();
    if (!dh)
        return io_openssl_seterr (IO_SSL_INIT, ret, "openssl_global_init");
    SSL_CTX_set_tmp_dh (gSSL_CTX, dh);
    DH_free (dh);
    return IO_SSL_OK;
}

io_ssl_err_t IOOpenSSLOpen (Connection *conn, char is_client)
{
    Dispatcher *d = calloc (1, sizeof (Dispatcher));
    if (!d)
        return IO_SSL_NOMEM;

    if (!io_openssl_init_ok)
        return IO_SSL_NOLIB;

    /* hack:      disallow SSL if we use proxy since we do not
       %%FIXME%%  have pull/push functions for OpenSSL yet */
    if (conn->dispatcher->next)
        return IO_SSL_NOLIB;

    conn->connect |= CONNECT_SELECT_A;
    d->next = conn->dispatcher;
    conn->dispatcher = d;
    d->funcs = &io_openssl_func;
    d->conn = conn;
    
    io_openssl_open (conn, d, is_client);
    return IO_SSL_OK;
}

const char *IOOpenSSLInitError ()
{
    return io_openssl_lasterr;
}

static io_ssl_err_t io_openssl_seterr (io_ssl_err_t err, int opensslerr, const char *msg)
{
    io_openssl_init_ok = err;
    s_repl (&io_openssl_lasterr, s_sprintf ("%s [%d] %s [%d]", msg, err, "OpenSSL error", opensslerr));
    return io_openssl_init_ok;
}

static void io_openssl_setconnerr (Dispatcher *d, io_ssl_err_t err, int opensslerr, const char *msg)
{
    d->openssl_err = err;
    d->d_errno = opensslerr;
    if (err == IO_SSL_INIT)
        d->conn->connect |= CONNECT_SELECT_A;
    s_repl (&d->lasterr, s_sprintf ("%s [%d] %s [%d]", msg, err, "OpenSSL error", opensslerr));
}

static const char *io_openssl_err (Connection *conn, Dispatcher *d)
{
    return d->lasterr;
}

#if 0
static ssize_t io_openssl_pull (openssl_transport_ptr_t user_data, void *buf, size_t len)
{
    Dispatcher *d = (Dispatcher *) user_data;
    int rc = d->conn->dispatcher->funcs->f_read (d->conn, d->next, buf, len);
    if      (rc == IO_CLOSED)  return 0;
    else if (rc == IO_OK)        { errno = EAGAIN; return -1; }
    else if (rc == IO_CONNECTED) { errno = EAGAIN; d->conn->connect |= CONNECT_SELECT_A; return -1; }
    else if (rc < 0)           return -1;
    else                       return rc;
}

static ssize_t io_openssl_push (openssl_transport_ptr_t user_data, const void *buf, size_t len)
{
    Dispatcher *d = (Dispatcher *) user_data;
    int rc = d->conn->dispatcher->funcs->f_write (d->conn, d->next, buf, len);
    if      (rc == IO_CLOSED)  return 0;
    else if (rc == IO_OK)        { errno = EAGAIN; return -1; }
    else if (rc == IO_CONNECTED) { errno = EAGAIN; d->conn->connect |= CONNECT_SELECT_A; return -1; }
    else if (rc < 0)           return -1;
    else                       return rc;
}
#endif

/*
 */
static void io_openssl_open (Connection *conn, Dispatcher *d, char is_client)
{
    conn->ssl_status = SSL_STATUS_HANDSHAKE;

    d->openssl = SSL_new (gSSL_CTX);
    SSL_set_session (d->openssl, NULL);
    SSL_set_fd (d->openssl, conn->sok);

    if (is_client)
        SSL_set_connect_state (d->openssl);
    else
        SSL_set_accept_state (d->openssl);

//    openssl_transport_set_ptr (d->openssl, d);
//    openssl_transport_set_pull_function (d->openssl, &io_openssl_pull);
//    openssl_transport_set_push_function (d->openssl, &io_openssl_push);
}

static io_err_t io_openssl_connecting (Connection *conn, Dispatcher *d)
{
    if (conn->connect & CONNECT_SELECT_A && d->openssl_err == IO_SSL_INIT)
        return IO_RW;

    if (conn->ssl_status == SSL_STATUS_FAILED
        || conn->ssl_status == SSL_STATUS_NA
        || conn->ssl_status == SSL_STATUS_CLOSE
        || conn->ssl_status == SSL_STATUS_INIT
        || conn->ssl_status == SSL_STATUS_REQUEST)
        return IO_CLOSED;
    
    if (conn->ssl_status == SSL_STATUS_HANDSHAKE)
    {
        conn->connect &= ~CONNECT_SELECT_A;

        int err_i = SSL_do_handshake (d->openssl);
        int err_j = SSL_get_error (d->openssl, err_i);
        switch (err_j)
        {
            case SSL_ERROR_NONE:
                break;
#ifdef SSL_ERROR_WANT_ACCEPT
            case SSL_ERROR_WANT_ACCEPT:
#endif
            case SSL_ERROR_WANT_READ:
                conn->ssl_status = SSL_STATUS_HANDSHAKE;
                conn->connect |= CONNECT_SELECT_R;
                return IO_OK;

            case SSL_ERROR_WANT_CONNECT:
            case SSL_ERROR_WANT_WRITE:
            case SSL_ERROR_WANT_X509_LOOKUP:
                conn->ssl_status = SSL_STATUS_HANDSHAKE;
                conn->connect |= CONNECT_SELECT_R | CONNECT_SELECT_W;
                return IO_OK;

            case SSL_ERROR_SSL:
                io_openssl_setconnerr (d, IO_SSL_OK, err_i, "internal error (handshake)");
                conn->ssl_status = SSL_STATUS_FAILED;
                return IO_RW;
            default:
                io_openssl_setconnerr (d, IO_SSL_OK, err_i, "other error (handshake)");
                conn->ssl_status = SSL_STATUS_FAILED;
                return IO_RW;
        }
        d->flags = FLAG_OPEN;
        conn->ssl_status = SSL_STATUS_OK;
        return IO_CONNECTED;
    }
    assert(0);
}

static int io_openssl_read (Connection *conn, Dispatcher *d, char *buf, size_t count)
{
    int rc, tmp;
    Contact *cont = conn->cont;

    if (conn->connect & CONNECT_SELECT_W)
    {
        rc = io_openssl_write (conn, d, NULL, 0);
        if (rc < 0)
            return rc;
    }

    DebugH (DEB_SSL, "SSL_read pre");
    rc = SSL_read (d->openssl, buf, count);
    DebugH (DEB_SSL, "SSL_read post");
    if (rc > 0)
    {
        DebugH (DEB_SSL, "read %d bytes from %s", rc, cont->nick);
        /* setting _W to trigger more reading */
        conn->connect |= CONNECT_SELECT_R | CONNECT_SELECT_W;
        return rc;
    }
    tmp = SSL_get_error (d->openssl, rc);
    switch (tmp)
    {
        case SSL_ERROR_NONE:
            return IO_OK;
        case SSL_ERROR_WANT_WRITE:
            conn->connect |= CONNECT_SELECT_W;
            conn->connect &= ~CONNECT_SELECT_R;
            return IO_OK;
        case SSL_ERROR_WANT_READ:
            conn->connect |= CONNECT_SELECT_R;
            conn->connect &= ~CONNECT_SELECT_W;
            return IO_OK;
        case SSL_ERROR_SSL:
            io_openssl_setconnerr (d, IO_SSL_OK, rc, "error ssl");
            return IO_RW;
        case SSL_ERROR_SYSCALL:
            io_openssl_setconnerr (d, IO_SSL_OK, rc, "error syscall");
            return IO_RW;
        case SSL_ERROR_ZERO_RETURN:
            io_openssl_setconnerr (d, IO_SSL_OK, rc, "zero return");
            return IO_CLOSED;
        default:
            io_openssl_setconnerr (d, IO_SSL_OK, rc, "other error");
            return IO_RW;
    }
}

static io_err_t io_openssl_write (Connection *conn, Dispatcher *d, const char *buf, size_t len)
{
    Contact *cont = conn->cont;
    int rc = 0;
    
    if (d->outlen)
    {
        rc = SSL_write (d->openssl, d->outbuf, d->outlen);
        if (rc > 0)
            DebugH (DEB_SSL, "sent %d/%d buffered bytes to %s", rc, d->outlen, cont->nick);
        else if (rc <= 0)
        {
            int tmp = SSL_get_error (d->openssl, rc);
            switch (tmp)
            {
                case SSL_ERROR_NONE:
                    break;
                case SSL_ERROR_WANT_WRITE:
                    conn->connect |= CONNECT_SELECT_W;
                    conn->connect &= ~CONNECT_SELECT_R;
                    break;
                case SSL_ERROR_WANT_READ:
                    conn->connect |= CONNECT_SELECT_R;
                    conn->connect &= ~CONNECT_SELECT_W;
                    break;
                case SSL_ERROR_SYSCALL:
                    io_openssl_setconnerr (d, IO_SSL_OK, rc, "error syscall (write)");
                    return IO_RW;
                default:
                    io_openssl_setconnerr (d, IO_SSL_OK, rc, "other error (write)");
                    return IO_RW;
            }
            rc = 0;
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
            return io_any_appendbuf (conn, d, buf, len);
        }
    }

    if (len > 0)
    {
        rc = SSL_write (d->openssl, buf, len);
        if (rc > 0)
            DebugH (DEB_SSL, "sent %d/%d bytes to %s", rc, len, cont->nick);
        else if (rc <= 0)
        {
            int tmp = SSL_get_error (d->openssl, rc);
            switch (tmp)
            {
                case SSL_ERROR_NONE:
                    break;
                case SSL_ERROR_WANT_WRITE:
                    conn->connect |= CONNECT_SELECT_W;
                    conn->connect &= ~CONNECT_SELECT_R;
                    break;
                case SSL_ERROR_WANT_READ:
                    conn->connect |= CONNECT_SELECT_R;
                    conn->connect &= ~CONNECT_SELECT_W;
                    break;
                case SSL_ERROR_SYSCALL:
                    io_openssl_setconnerr (d, IO_SSL_OK, rc, "error syscall (write)");
                    return IO_RW;
                default:
                    io_openssl_setconnerr (d, IO_SSL_OK, rc, "other error (write)");
                    return IO_RW;
            }
            rc = 0;
        }
        len -= rc;
        buf += rc;
    }
    if (len <= 0)
        return IO_OK;
    return io_any_appendbuf (conn, d, buf, len);
}

static void io_openssl_close (Connection *conn, Dispatcher *d)
{
    if (d->next && d->next->funcs && d->next->funcs->f_close)
        d->next->funcs->f_close (conn, d->next);

    if (conn->dispatcher == d)
        conn->dispatcher = NULL;

    if (d)
    {
        if (d->openssl) 
        {
            SSL_shutdown (d->openssl);
            SSL_free (d->openssl);
        }
        if (d->outlen)
            free (d->outbuf);
        free (d);
    }
    conn->ssl_status = conn->ssl_status == SSL_STATUS_OK ? SSL_STATUS_NA : SSL_STATUS_FAILED;
}

#endif
