/*
 * Manages the available connections (note the file is misnamed)
 *
 * mICQ Copyright (C) © 2001,2002,2003 Rüdiger Kuhlmann
 *
 * mICQ is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 dated June, 1991.
 *
 * mICQ is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * In addition, as a special exception permission is granted to link the
 * code of this release of mICQ with the OpenSSL project's "OpenSSL"
 * library, and distribute the linked executables.  You must obey the GNU
 * General Public License in all respects for all of the code used other
 * than "OpenSSL".  If you modify this file, you may extend this exception
 * to your version of the file, but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your version
 * of this file.
 *
 * $Id$
 */

#include "micq.h"
#include "session.h"
#include "preferences.h"
#include "util_ui.h"
#include "cmd_pkt_cmd_v5_util.h"
#include "cmd_pkt_v8.h"
#include "tcp.h"
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#if HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#include <assert.h>

#define listlen 40

static Connection *slist[listlen] = { 0 };

/*
 * Creates a new session.
 */
Connection *ConnectionC (void)
{
    int i;
    
    for (i = 0; slist[i] && i < listlen; i++)  ;
     
    if (i == listlen)
        return NULL;

    slist[i] = calloc (1, sizeof (Connection));
    
    assert (slist[i]);
    
    slist[i]->our_local_ip   = 0x7f000001;
    slist[i]->our_outside_ip = 0x7f000001;
    slist[i]->status = STATUS_OFFLINE;
    slist[i]->sok = -1;

    Debug (DEB_CONNECT, "<=== %p[%d] create", slist[i], i);

    return slist[i];
}

/*
 * Clones an existing session, while blanking out some values.
 */
Connection *ConnectionClone (Connection *conn, UWORD type)
{
    Connection *child;
    
    child = ConnectionC ();
    if (!child)
        return NULL;
    memcpy (child, conn, sizeof (*child));
    child->parent   = conn;
    child->assoc    = NULL;
    child->sok      = -1;
    child->connect  = 0;
    child->incoming = NULL;
    child->type     = type;
    child->open     = NULL;
    child->server   = NULL;
    child->contacts = NULL;
    
    Debug (DEB_CONNECT, "<=*= %p (%s) clone from %p (%s)", child, ConnectionType (child), conn, ConnectionType (conn));

    return child;
}

/*
 * Returns the n-th session.
 */
Connection *ConnectionNr (int i)
{
    if (i >= listlen)
        return NULL;
    return slist[i];
}

/*
 * Finds a session of given type and/or given uin.
 * Actually, you may specify TYPEF_* here that must all be set.
 * The parent is the session this one has to have as parent.
 */
Connection *ConnectionFind (UWORD type, UDWORD uin, const Connection *parent)
{
    int i;
    
    if (parent)
    {
        if (uin)
            for (i = 0; i < listlen; i++)
                if (slist[i] && ((slist[i]->type & type) == type) && (slist[i]->uin == uin) && slist[i]->parent == parent)
                    return slist[i];
        if (!uin)
            for (i = 0; i < listlen; i++)
                if (slist[i] && ((slist[i]->type & type) == type) && (slist[i]->connect & CONNECT_OK) && slist[i]->parent == parent)
                    return slist[i];
    }
    else
    {
        if (uin)
            for (i = 0; i < listlen; i++)
                if (slist[i] && ((slist[i]->type & type) == type) && (slist[i]->uin == uin))
                    return slist[i];
        if (!uin)
            for (i = 0; i < listlen; i++)
                if (slist[i] && ((slist[i]->type & type) == type) && (slist[i]->connect & CONNECT_OK))
                    return slist[i];
        if (!uin)
            for (i = 0; i < listlen; i++)
                if (slist[i] && ((slist[i]->type & type) == type))
                    return slist[i];
    }
    return NULL;
}

/*
 * Finds the index of this session.
 */
UDWORD ConnectionFindNr (Connection *conn)
{
    int i;

    if (!conn)
        return -1;
    for (i = 0; i < listlen; i++)
        if (slist[i] == conn)
            return i;
    return -1;
}

/*
 * Closes and removes a session.
 */
void ConnectionClose (Connection *conn)
{
    int i, j;
    
    i = ConnectionFindNr (conn);
    
    assert (conn);
    
    Debug (DEB_CONNECT, "===> %p[%d] (%s) closing...", conn, i, ConnectionType (conn));

    if (i == -1)
        return;

    if (conn->close)
        conn->close (conn);

    i = ConnectionFindNr (conn);
    if (i == -1)
        return;

    if (conn->sok != -1)
        sockclose (conn->sok);
    conn->sok     = -1;
    conn->connect = 0;
    conn->type    = 0;
    conn->parent  = NULL;
    if (conn->server)
        free (conn->server);
    conn->server  = NULL;

    for (j = 0; j < listlen; j++)
        if (slist[j] && slist[j]->assoc == conn)
            slist[j]->assoc = NULL;

    for (j = 0; j < listlen; j++)
        if (slist[j] && slist[j]->parent == conn)
        {
            slist[j]->parent = NULL;
            ConnectionClose (slist[j]);
            j--;
        }

    QueueCancel (conn);

    for (j = i; j + 1 < listlen && slist[j]; j++)
        slist[j] = slist[j + 1];
    slist[j] = NULL;

    Debug (DEB_CONNECT, "===> %p[%d] closed.", conn, i);

    free (conn);
}

/*
 * Returns a string describing the session's type.
 */
const char *ConnectionType (Connection *conn)
{
    switch (conn->type) {
        case TYPE_SERVER:
            return i18n (1889, "server");
        case TYPE_SERVER_OLD:
            return i18n (1744, "server (v5)");
        case TYPE_MSGLISTEN:
            return i18n (1947, "listener");
        case TYPE_MSGDIRECT:
            return i18n (1890, "peer-to-peer");
        case TYPE_FILELISTEN:
            return i18n (2089, "file listener");
        case TYPE_FILEDIRECT:
            return i18n (2090, "file peer-to-peer");
        case TYPE_FILE:
            return i18n (2067, "file io");
        case TYPE_REMOTE:
            return i18n (2225, "remote control");
        default:
            return i18n (1745, "unknown");
    }
}
