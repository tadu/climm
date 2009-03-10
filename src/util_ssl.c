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
