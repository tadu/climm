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
 * $Id$
 */

#include "climm.h"
#include <assert.h>

#if ENABLE_SSL

#include "preferences.h"
#include "conv.h"
#include "util_ssl.h"
#include "util_str.h"
#include "util_tcl.h"
#include "util_ui.h"
#include "oscar_dc.h"
#include "connection.h"
#include "contact.h"
#include "packet.h"

#include <errno.h>

#ifndef ENABLE_TCL
#define TCLMessage(from, text) {}
#define TCLEvent(from, type, data) {}
#endif

#if ENABLE_GNUTLS
#include <gnutls/gnutls.h>                                                                                                        
#include <gcrypt.h>                                                                                                               
#else /* ENABLE_GNUTLS */
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/dh.h>
#include <openssl/opensslv.h>
#include <openssl/md5.h>
#endif /* ENABLE_GNUTLS */


#if ENABLE_GNUTLS
struct ssl_md5ctx_s {
    gcry_md_hd_t h;
};

ssl_md5ctx_t *ssl_md5_init ()
{
    ssl_md5ctx_t *ctx = malloc (sizeof *ctx);
    if (!ctx)
        return NULL;
    gcry_error_t err = gcry_md_open (&ctx->h, GCRY_MD_MD5, 0);
    if (gcry_err_code (err))
    {
        free (ctx);
        return NULL;
    }
    return ctx;
}

void ssl_md5_write (ssl_md5ctx_t *ctx, char *buf, size_t len)
{
    if (!ctx)
        return;
    gcry_md_write (ctx->h, buf, len);
}

int ssl_md5_final (ssl_md5ctx_t *ctx, char *buf)
{
    if (!ctx)
        return -1;
    gcry_md_final (ctx->h);
    unsigned char *hash = gcry_md_read (ctx->h, 0);
    assert (hash);
    memcpy (buf, hash, 16);
    gcry_md_close (ctx->h);
    free (ctx);
    return 0;
}
#else
struct ssl_md5ctx_s {
    MD5_CTX ctx;
};

ssl_md5ctx_t *ssl_md5_init ()
{
    ssl_md5ctx_t *ctx = malloc (sizeof *ctx);
    if (!ctx)
        return NULL;
    MD5_Init (&ctx->ctx);
    return ctx;
}

void ssl_md5_write (ssl_md5ctx_t *ctx, char *buf, size_t len)
{
    if (!ctx)
        return;
    MD5_Update (&ctx->ctx, buf, len);
}

int ssl_md5_final (ssl_md5ctx_t *ctx, char *buf)
{
    if (!ctx)
        return -1;
    MD5_Final ((unsigned char *)buf, &ctx->ctx);
    free (ctx);
    return 0;
}
#endif


/*
 * Check whether peer supports SSL
 *
 * Returns 0 if SSL/TLS not supported by peer.
 */
#undef ssl_supported
int ssl_supported (Connection *conn DEBUGPARAM)
{
    Contact *cont;
    UBYTE status_save = conn->ssl_status;
    
//    if (!ssl_init_ok)
//        return 0;   /* our SSL core is not working :( */
    
    if (conn->ssl_status == SSL_STATUS_OK)
        return 1;   /* SSL session already established */

    if (conn->ssl_status == SSL_STATUS_FAILED)
        return 0;   /* ssl handshake with peer already failed. So don't try again */

    conn->ssl_status = SSL_STATUS_FAILED;
    
    if (!(conn->type & TYPEF_ANY_PEER))
        return 0;
        
    cont = conn->cont;
    
    /* check for peer capabilities
     * Note: we never initialize SSL for incoming direct connections yet
     *        in order to avoid mutual SSL init trials among climm peers.
     */
    if (!cont)
        return 0;

    if (!(HAS_CAP (cont->caps, CAP_SIMNEW) || HAS_CAP (cont->caps, CAP_MICQ)
          || HAS_CAP (cont->caps, CAP_CLIMM) || HAS_CAP (cont->caps, CAP_LICQNEW)
          || (cont->dc && (cont->dc->id1 & 0xFFFF0000) == LICQ_WITHSSL)))
    {
        Debug (DEB_SSL, "%s (%s) is no SSL candidate", cont->nick, cont->screen);
        TCLEvent (cont, "ssl", "no_candidate");
        return 0;
    }

    conn->ssl_status = status_save;
    Debug (DEB_SSL, "%s (%s) is an SSL candidate", cont->nick, cont->screen);
    TCLEvent (cont, "ssl", "candidate");
    return 1;
}

/* 
 * Request secure channel in licq's way.
 */
BOOL TCPSendSSLReq (Connection *list, Contact *cont)
{
    Connection *peer;
    UBYTE ret;

    ret = PeerSendMsg (list, cont, MSG_SSL_OPEN, "");
    if ((peer = ServerFindChild (list->serv, cont, TYPE_MSGDIRECT)))
        peer->ssl_status = SSL_STATUS_REQUEST;
    return ret;
}                      
#endif
