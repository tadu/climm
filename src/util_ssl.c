/*
 * TLS Diffie Hellmann extension.
 *
 * mICQ TLS extension Copyright (C) © 2003 Roman Hoog Antink
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
 * You should have received a copy of the GNU General Public License
 * along with this package; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * $Id$
 */

#include "micq.h"

#ifdef ENABLE_SSL

#include "preferences.h"
#include "conv.h"
#include "util_ssl.h"
#include "util_str.h"
#include "util_tcl.h"
#include "util_ui.h"
#include "tcp.h"
#include "session.h"
#include "contact.h"
#include "packet.h"

#include <gnutls/gnutls.h>
#include <string.h>
#include <errno.h>

#ifndef ENABLE_TCL
#define TCLMessage(from, text) {}
#define TCLEvent(type, data) {}
#endif

#define SSL_FAIL(s, e)    { const char *t = s; \
                            if (prG->verbose & DEB_SSL || \
                                e != GNUTLS_E_UNEXPECTED_PACKET_LENGTH) \
                                  M_printf (i18n (2374, "SSL error: %s [%d]\n"), \
                                    t ? t : "unknown", __LINE__);  \
                       }    
                        

#define SSL_CHECK_SUCCESS_1_OK(status, ret, msg1, msg2) SSL_CHECK_SUCCESS (status, ret, 1, msg1, msg2)
#define SSL_CHECK_SUCCESS_0_OK(status, ret, msg1, msg2) SSL_CHECK_SUCCESS (status, ret, 0, msg1, msg2)

#define SSL_CHECK_SUCCESS(status, ret, ok, msg1, msg2) { if (status != ok) { \
                                     SSL_FAIL (s_sprintf ("%s %s %s", \
                                        msg1 ? msg1 : "", msg2 ? msg2 : "", \
                                        gnutls_strerror (status)), status); \
                                     return ret; \
                                 } \
                               } 

static gnutls_anon_client_credentials client_cred;
static gnutls_anon_server_credentials server_cred;
static gnutls_dh_params dh_parm;
static int ssl_init_ok;

/* 
 * Initialize ssl library
 *
 * Return -1 means failure, 0 means ok.
 */
int SSLInit ()
{
    int ret;
#ifndef HAVE_DH_GENPARAM2
    gnutls_datum p1, p2;
#endif

    ssl_init_ok = 0;

    ret = gnutls_global_init ();
    SSL_CHECK_SUCCESS_0_OK (ret, -1, "gnutls_global_init", NULL);

    ret = gnutls_anon_allocate_client_credentials (&client_cred);
    SSL_CHECK_SUCCESS_0_OK (ret, -1, "allocate_credentials", "[client]");

    ret = gnutls_anon_allocate_server_credentials (&server_cred);
    SSL_CHECK_SUCCESS_0_OK (ret, -1, "allocate_credentials", "[server]");

    ret = gnutls_dh_params_init (&dh_parm);
    SSL_CHECK_SUCCESS_0_OK (ret, -1, "DH param init", "[server]");

#ifdef HAVE_DH_GENPARAM2
    ret = gnutls_dh_params_generate2 (dh_parm, DH_OFFER_BITS);
    SSL_CHECK_SUCCESS_0_OK (ret, -1, "DH param generate2", "[server]");
#else
    ret = gnutls_dh_params_generate (&p1, &p2, DH_OFFER_BITS);
    SSL_CHECK_SUCCESS_0_OK (ret, -1, "DH param generate", "[server]");
    ret = gnutls_dh_params_set (dh_parm, p1, p2, DH_OFFER_BITS);
    SSL_CHECK_SUCCESS_0_OK (ret, -1, "DH param set", "[server]");
    free (p1.data);
    free (p2.data);
#endif

    gnutls_anon_set_server_dh_params (server_cred, dh_parm);

    ssl_init_ok = 1;
    return 0;
}

/*
 * Check whether peer supports SSL
 *
 * Returns 0 if SSL/TLS not supported by peer.
 */
int ssl_supported (Connection *conn)
{
    Contact *contact;
    UBYTE status_save = conn->ssl_status;
    
    if (!ssl_init_ok)
        return 0;   /* our SSL core is not working :( */
    
    if (conn->ssl_status == SSL_STATUS_OK)
        return 1;   /* SSL session already established */

    if (conn->ssl_status == SSL_STATUS_FAILED)
        return 0;   /* ssl handshake with peer already failed. So don't try again */

    conn->ssl_status = SSL_STATUS_FAILED;
    
    /* I don't assume server supporting SSL. Would not
     * offer much privacy anyway due to lack of end-to-end encryption.
     */
    if (! (conn->type & TYPEF_ANY_PEER))
        return 0;
        
    contact = ContactUIN (conn, conn->uin);
    
    /* check for peer capabilities */

    /* Caveat: if we are server, contact is our own contact, not the peers!
     *         how can we get the peers contact/CAPs? 
     * Note: we never initialize SSL for incoming direct connections yet
     *        in order to avoid mutual SSL init trials among mICQ peers.
     */
    if (!contact || !(HAS_CAP(contact->caps, CAP_MICQ) || (HAS_CAP(contact->caps, CAP_LICQ) && contact->dc && (contact->dc->id1 & 0xFFFF0000) == LICQ_WITHSSL)))
    {
        Debug (DEB_SSL, "%s (%ld) is no SSL candidate", contact->nick, contact->uin);
        TCLEvent ("ssl", s_sprintf ("%lu no_candidate", contact->uin));
        return 0;
    }

    conn->ssl_status = status_save;
    Debug (DEB_SSL, "%s (%ld) is an SSL candidate", contact->nick, contact->uin);
    TCLEvent ("ssl", s_sprintf ("%lu candidate", contact->uin));
    return 1;
}

/*
 * ssl_connect
 *
 * execute SSL handshake
 *
 * return: 1 means ok. 0 failed.
 */
int ssl_connect (Connection *conn, BOOL is_client)
{
    int ret;
    int kx_prio[2] = { GNUTLS_KX_ANON_DH, 0 };
#ifdef ENABLE_TCL
    Contact *contact = ContactUIN (conn, conn->uin);    
#endif

    Debug (DEB_SSL, "ssl_connect");
    if (!ssl_init_ok || (conn->ssl_status != SSL_STATUS_NA && conn->ssl_status != SSL_STATUS_INIT && conn->ssl_status != SSL_STATUS_REQUEST))
    {
        TCLEvent ("ssl", s_sprintf ("%lu failed precondition", contact->uin));
        return 0;
    }    
    conn->ssl_status = SSL_STATUS_FAILED;
    conn->connect = 1;

    ret = gnutls_init (&conn->ssl, is_client ? GNUTLS_CLIENT : GNUTLS_SERVER);
    if (ret)
        TCLEvent ("ssl", s_sprintf ("%lu failed init", contact->uin));

    SSL_CHECK_SUCCESS_0_OK (ret, 0, "init", is_client ? "[client]" : "[server]");
    
    gnutls_set_default_priority (conn->ssl);
    gnutls_kx_set_priority (conn->ssl, kx_prio);

    ret = gnutls_credentials_set (conn->ssl, GNUTLS_CRD_ANON, is_client ? client_cred : server_cred);
    if (ret)
        TCLEvent ("ssl", s_sprintf ("%lu failed key", contact->uin));

    SSL_CHECK_SUCCESS_0_OK (ret, 0, "credentials_set", is_client ? "[client]" : "[server]");
    if (is_client)
        /* reduce minimal prime bits expected for licq interoperability */
        gnutls_dh_set_prime_bits (conn->ssl, DH_EXPECT_BITS);
    
    gnutls_transport_set_ptr (conn->ssl, (gnutls_transport_ptr)conn->sok); /* return type void */

    return ssl_handshake (conn) != 0;
}

int ssl_handshake (Connection *conn)
{
    Contact *contact = ContactUIN (conn, conn->uin);    
    int ret;

    ret = gnutls_handshake (conn->ssl);
    Debug (DEB_SSL, "handshake %d (%d,%d)", ret, GNUTLS_E_AGAIN, GNUTLS_E_INTERRUPTED);
    if (ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED)
    {
        conn->ssl_status = SSL_STATUS_HANDSHAKE;
        conn->connect |= CONNECT_SELECT_R | CONNECT_SELECT_W;
        return 1;
    }

    if (ret)
    {
        TCLEvent ("ssl", s_sprintf ("%lu failed handshake", contact->uin));

        SSL_FAIL (s_sprintf ("%s %s", "handshake", gnutls_strerror (ret)), ret);
        conn->ssl_status = SSL_STATUS_FAILED;
        TCPClose (conn);
        conn->connect = 0;
        return 0;
    }

    conn->ssl_status = SSL_STATUS_OK;
    conn->connect = CONNECT_OK | CONNECT_SELECT_R;
    if (prG->verbose)
    {
        M_printf ("%s %s%*s%s ", s_now, COLCONTACT, uiG.nick_len + s_delta (contact->nick), contact->nick, COLNONE);
        M_printf (i18n (2375, "SSL handshake ok\n"));
    }
    TCLEvent ("ssl", s_sprintf ("%lu ok", contact->uin));

    return 2;
}

/*
 * sockread wrapper for network connections
 *
 * Calls default sockread() for non-SSL connections.
 */
int ssl_sockread (Connection *conn, UBYTE *data, UWORD len_p)
{
    int len, rc;
    Contact *contact = ContactUIN (conn, conn->uin);    
    
    if (conn->ssl_status == SSL_STATUS_HANDSHAKE)
    {
        ssl_handshake (conn);
        errno = EAGAIN;
        return -1;
    }
    if (conn->ssl_status != SSL_STATUS_OK)
    {
        len = sockread (conn->sok, data, len_p);
        rc = errno;
        if (!len && !rc)
            rc = ECONNRESET;
        errno = rc;
        return len;
    }

    len = gnutls_record_recv (conn->ssl, data, len_p);
    if (len > 0)
        Debug (DEB_SSL, "read %d bytes from %s", len, contact->nick);
    if (len < 0)
    {
        SSL_FAIL (s_sprintf (i18n (2376, "SSL read from %s [ERR=%d]: %s"), 
                  contact->nick, len, gnutls_strerror (len)), len);
        ssl_disconnect (conn);
    }
    if (!len)
    {
        errno = EAGAIN;
        len = -1;
    }
    return len;
}

/*
 * sockwrite wrapper for network connections
 *
 * Calls default sockwrite() for non-SSL connections.
 */
int ssl_sockwrite (Connection *conn, UBYTE *data, UWORD len_p)
{
    int len;
    Contact *contact = ContactUIN (conn, conn->uin);    
    
    if (conn->ssl_status == SSL_STATUS_HANDSHAKE)
    {
        ssl_handshake (conn);
        return 0;
    }
    if (conn->ssl_status != SSL_STATUS_OK)
        return sockwrite (conn->sok, data, len_p);

    len = gnutls_record_send (conn->ssl, data, len_p);
    if (len > 0)
        Debug (DEB_SSL, "write %d bytes to %s", len, contact->nick);
    if (len < 0)
    {
        SSL_FAIL (s_sprintf (i18n (2377, "SSL write to %s [ERR=%d]: %s"), 
                 contact->nick, len, gnutls_strerror (len)), len);
        ssl_disconnect (conn);
    }
    return len;
}

/*
 * Stop SSL traffic on given Connection.
 *
 * Note: does not close socket. Frees SSL data only.
 */
void ssl_disconnect (Connection *conn)
{
    Debug (DEB_SSL, "ssl_disconnect");

    if (conn->ssl_status == SSL_STATUS_OK)
    {
        conn->ssl_status = SSL_STATUS_NA;
        if (conn->ssl) 
            free (conn->ssl);
        conn->ssl = NULL;
    }
}

/*
 * Shutdown SSL session and release SSL memory
 */
void ssl_sockclose (Connection *conn)
{
    Debug (DEB_SSL, "ssl_sockclose");

    if (conn->ssl_status == SSL_STATUS_OK)
    {
        Debug (DEB_SSL, "ssl_sockclose calling ssl_disconnect");
        ssl_disconnect (conn);
    }
    sockclose (conn->sok);
}

/*
 * Return error description
 */
const char *ssl_strerror (int error)
{
    return gnutls_strerror (error);
}

/* 
 * Request secure channel in licq's way.
 */
BOOL TCPSendSSLReq (Connection *list, Contact *cont)
{
    return TCPSendMsg (list, cont, "", MSG_SSL_OPEN);
}                      
#endif /* ENABLE_SSL */
