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
 * $Id$
 */

#include "micq.h"
#include "session.h"
#include "preferences.h"
#include "util_ui.h"
#include "cmd_pkt_cmd_v5_util.h"
#include "cmd_pkt_v8.h"
#include "tcp.h"
#include <string.h>
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#if HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#include <assert.h>

#define ConnectionListLen 10

typedef struct ConnectionList_s ConnectionList;

struct ConnectionList_s
{
    ConnectionList *more;
    Connection     *conn[ConnectionListLen];
};


static ConnectionList slist = { NULL };

/*
 * Creates a new session.
 * Returns NULL if not enough memory available.
 */
Connection *ConnectionC (void)
{
    ConnectionList *cl;
    Connection *conn;
    int i, j;
    
    for (i = j = 0, cl = &slist; cl && cl->conn[i]; cl = cl->more)
        for (i = 0; i < ConnectionListLen && cl->conn[i]; i++, j++)
            ;
    
    if (!cl)
    {
        for (cl = &slist; cl->more; cl = cl->more)
            ;
        cl->more = calloc (1, sizeof (ConnectionList));
        
        if (!cl->more)
            return NULL;

        cl = cl->more;
        i = 0;
    }
    
    conn = calloc (1, sizeof (Connection));
    
    if (!conn)
        return NULL;
    
    conn->our_local_ip   = 0x7f000001;
    conn->our_outside_ip = 0x7f000001;
    conn->status = STATUS_OFFLINE;
    conn->sok = -1;

    Debug (DEB_CONNECT, "<=== %p[%d] create", conn, j);

    return cl->conn[i] = conn;
}

/*
 * Clones an existing session, while blanking out some values.
 * Returns NULL if not enough memory available.
 */
Connection *ConnectionClone (Connection *conn, UWORD type)
{
    Connection *child;
    
    child = ConnectionC ();
    if (!child)
        return NULL;

    memcpy (child, conn, sizeof (Connection));
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
    ConnectionList *cl;
    
    for (cl = &slist; cl && i > ConnectionListLen; cl = cl->more)
        i -= ConnectionListLen;
    
    if (!cl)
        return NULL;
    
    return cl->conn[i];
}

/*
 * Finds a session of given type and/or given uin.
 * Actually, you may specify TYPEF_* here that must all be set.
 * The parent is the session this one has to have as parent.
 */
Connection *ConnectionFind (UWORD type, UDWORD uin, const Connection *parent)
{
    ConnectionList *cl;
    Connection *conn;
    int i;
    
    if (parent)
    {
        if (uin)
        {
            for (cl = &slist; cl; cl = cl->more)
                for (i = 0; i < ConnectionListLen; i++)
                    if ((conn = cl->conn[i]) && (conn->type & type) == type && conn->uin == uin && conn->parent == parent)
                        return conn;
        }
        else
            for (cl = &slist; cl; cl = cl->more)
                for (i = 0; i < ConnectionListLen; i++)
                    if ((conn = cl->conn[i]) && (conn->type & type) == type && (conn->connect & CONNECT_OK) && conn->parent == parent)
                        return conn;
    }
    else
    {
        if (uin)
        {
            for (cl = &slist; cl; cl = cl->more)
                for (i = 0; i < ConnectionListLen; i++)
                    if ((conn = cl->conn[i]) && (conn->type & type) == type && conn->uin == uin)
                        return conn;
        }
        else
        {
            for (cl = &slist; cl; cl = cl->more)
                for (i = 0; i < ConnectionListLen; i++)
                    if ((conn = cl->conn[i]) && (conn->type & type) == type && (conn->connect & CONNECT_OK))
                        return conn;
            for (cl = &slist; cl; cl = cl->more)
                for (i = 0; i < ConnectionListLen; i++)
                    if ((conn = cl->conn[i]) && (conn->type & type) == type)
                        return conn;
        }
    }
    return NULL;
}

/*
 * Finds the index of this session.
 */
UDWORD ConnectionFindNr (Connection *conn)
{
    ConnectionList *cl;
    int i, j;

    if (!conn)
        return -1;

    for (i = 0, cl = &slist; cl; cl = cl->more)
        for (j = i; i < j + ConnectionListLen; i++)
            if (cl->conn[i] == conn)
                return i;
    return -1;
}

/*
 * Closes and removes a session.
 */
void ConnectionClose (Connection *conn)
{
    ConnectionList *cl;
    Connection *clc;
    int i, j, k;
    
    i = ConnectionFindNr (conn);
    
    if (i == -1)
        return;

    Debug (DEB_CONNECT, "===> %p[%d] (%s) closing...", conn, i, ConnectionType (conn));

    if (conn->close)
        conn->close (conn);

    i = k = ConnectionFindNr (conn);
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

    for (cl = &slist; cl; cl = cl->more)
        for (j = 0; j < ConnectionListLen; j++)
            if ((clc = cl->conn[j]) && clc->assoc == conn)
                clc->assoc = NULL;

    for (cl = &slist; cl; cl = cl->more)
        for (j = 0; j < ConnectionListLen; j++)
            if ((clc = cl->conn[j]) && clc->parent == conn)
            {
                clc->parent = NULL;
                ConnectionClose (clc);
                cl = &slist;
                j = -1;
            }

    QueueCancel (conn);

    for (cl = &slist; cl && i > ConnectionListLen; cl = cl->more)
        i -= ConnectionListLen;
    assert (cl);
    
    while (cl)
    {
        for (j = i; j + 1 < ConnectionListLen; j++)
            cl->conn[j] = cl->conn[j + 1];
        cl->conn[ConnectionListLen - 1] = cl->more ? cl->more->conn[0] : NULL;
        j = 0;
    }

    Debug (DEB_CONNECT, "===> %p[%d] closed.", conn, k);

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
