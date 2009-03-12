/*
 * Manages the available connections (note the file is misnamed)
 *
 * climm Copyright (C) © 2001-2007 Rüdiger Kuhlmann
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
 * $Id$
 */

#include "climm.h"
#include "connection.h"
#include "preferences.h"
#include "util_ui.h"
#include "oscar_dc.h"
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#if HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#include <assert.h>

#define ServerListLen      5
#define ConnectionListLen 10

typedef struct ConnectionList_s ConnectionList;
typedef struct ServerList_s     ServerList;

struct ConnectionList_s
{
    ConnectionList *more;
    Connection     *conn[ConnectionListLen];
};


struct ServerList_s
{
    ServerList *more;
    Server     *serv[ServerListLen];
};


static ConnectionList clist = { NULL, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0} };
static ServerList     slist = { NULL, {0, 0, 0, 0, 0} };

/*
 * Creates a new session.
 * Returns NULL if not enough memory available.
 */
#undef ServerC
Server *ServerC (UWORD type DEBUGPARAM)
{
    Connection *conn;
    Server *serv;
    ServerList *cl;
    int i, j;

    conn = ConnectionC (type & ~TYPEF_ANY_SERVER);
    if (!conn)
        return NULL;

    cl = &slist;
    i = j = 0;
    while (cl->serv[ServerListLen - 1] && cl->more)
        cl = cl->more, j += ServerListLen;
    if (cl->serv[ServerListLen - 1])
    {
        cl->more = calloc (1, sizeof (ServerList));
        
        if (!cl->more)
            return NULL;

        cl = cl->more;
        j += ServerListLen;
    }
    while (cl->serv[i])
        i++, j++;

    if (!(serv = calloc (1, sizeof (Server))))
    {
        ConnectionD (conn);
        return NULL;
    }
    cl->serv[i] = serv;
    
    conn->serv = serv;
    serv->conn = conn;

    serv->logfd = -1;
    serv->flags = 0;
    serv->status = ims_offline;
    assert (type & TYPEF_ANY_SERVER);
    serv->type = type;

    Debug (DEB_CONNECT, "<=S= %p create %d", serv, serv->type);
    return serv;
}

/*
 * Creates a new session.
 * Returns NULL if not enough memory available.
 */
#undef ConnectionC
Connection *ConnectionC (UWORD type DEBUGPARAM)
{
    ConnectionList *cl;
    Connection *conn;
    int i, j;

    cl = &clist;
    i = j = 0;
    assert (~type & TYPEF_ANY_SERVER);
    while (cl->conn[ConnectionListLen - 1] && cl->more)
        cl = cl->more, j += ConnectionListLen;
    if (cl->conn[ConnectionListLen - 1])
    {
        cl->more = calloc (1, sizeof (ConnectionList));
        
        if (!cl->more)
            return NULL;

        cl = cl->more;
        j += ConnectionListLen;
    }
    while (cl->conn[i])
        i++, j++;

    if (!(conn = calloc (1, sizeof (Connection))))
        return NULL;
    cl->conn[i] = conn;
    
    conn->our_local_ip   = 0x7f000001;
    conn->our_outside_ip = 0x7f000001;
    conn->sok = -1;
    conn->type = type;

    Debug (DEB_CONNECT, "<=== %p[%d] create %d", conn, j, type);

    return conn;
}

/*
 * Clones an existing session, while blanking out some values.
 * Returns NULL if not enough memory available.
 */
#undef ServerChild
Connection *ServerChild (Server *serv, Contact *cont, UWORD type DEBUGPARAM)
{
    Connection *child;
    
    child = ConnectionC (type DEBUGFOR);
    if (!child)
        return NULL;

    child->serv   = serv;
    child->cont   = cont;
    
    Debug (DEB_CONNECT, "<=*= %p (%s) clone from %p (%s)",
        child, ConnectionStrType (child), serv, ServerStrType (serv));

    return child;
}

/*
 * Returns the n-th connection.
 */
Connection *ConnectionNr (int i)
{
    ConnectionList *cl;
    
    for (cl = &clist; cl && i >= ConnectionListLen; cl = cl->more)
        i -= ConnectionListLen;
    
    if (!cl || i < 0)
        return NULL;
    
    return cl->conn[i];
}

/*
 * Returns the n-th session.
 */
Server *ServerNr (int i)
{
    ServerList *cl;
    
    for (cl = &slist; cl && i >= ServerListLen; cl = cl->more)
        i -= ServerListLen;
    
    if (!cl || i < 0)
        return NULL;
    
    return cl->serv[i];
}

/*
 * Finds a session of given type and/or given uin.
 * Actually, you may specify TYPEF_* here that must all be set.
 * The parent is the session this one has to have as parent.
 */
Connection *ServerFindChild (const Server *parent, const Contact *cont, UWORD type)
{
    ConnectionList *cl;
    Connection *conn;
    int i;
    
    if (parent)
    {
        if (cont)
        {
            for (cl = &clist; cl; cl = cl->more)
                for (i = 0; i < ConnectionListLen; i++)
                    if ((conn = cl->conn[i]) && (conn->type & type) == type && conn->cont == cont && conn->serv == parent)
                        return conn;
        }
        else
            for (cl = &clist; cl; cl = cl->more)
                for (i = 0; i < ConnectionListLen; i++)
                    if ((conn = cl->conn[i]) && (conn->type & type) == type && (conn->connect & CONNECT_OK) && conn->serv == parent)
                        return conn;
    }
    else
    {
        if (cont)
        {
            for (cl = &clist; cl; cl = cl->more)
                for (i = 0; i < ConnectionListLen; i++)
                    if ((conn = cl->conn[i]) && (conn->type & type) == type && conn->cont == cont)
                        return conn;
        }
        else
        {
            for (cl = &clist; cl; cl = cl->more)
                for (i = 0; i < ConnectionListLen; i++)
                    if ((conn = cl->conn[i]) && (conn->type & type) == type && (conn->connect & CONNECT_OK))
                        return conn;
            for (cl = &clist; cl; cl = cl->more)
                for (i = 0; i < ConnectionListLen; i++)
                    if ((conn = cl->conn[i]) && (conn->type & type) == type)
                        return conn;
        }
    }
    return NULL;
}

/*
 * Finds a session of given type and given screen.
 * Actually, you may specify TYPEF_* here that must all be set.
 */
Server *ServerFindScreen (UWORD type, const char *screen)
{
    ServerList *cl;
    Server *serv;
    int i;

    assert (type);    
    assert (screen);
    assert (type & TYPEF_ANY_SERVER);
    
    for (cl = &slist; cl; cl = cl->more)
        for (i = 0; i < ServerListLen; i++)
            if ((serv = cl->serv[i]) && (serv->type & type) == type
                && serv->screen && !strcmp (serv->screen, screen))
                return serv;
    return NULL;
}

/*
 * Finds the index of this session.
 */
UDWORD ConnectionFindNr (const Connection *conn)
{
    ConnectionList *cl;
    int i, j;

    if (!conn)
        return -1;

    for (i = 0, cl = &clist; cl; cl = cl->more)
        for (j = i; i < j + ConnectionListLen; i++)
            if (cl->conn[i % ConnectionListLen] == conn)
                return i;
    return -1;
}

/*
 * Finds the index of this session.
 */
UDWORD ServerFindNr (const Server *serv)
{
    ServerList *cl;
    int i, j;

    if (!serv)
        return -1;

    for (i = 0, cl = &slist; cl; cl = cl->more)
        for (j = i; i < j + ServerListLen; i++)
            if (cl->serv[i % ServerListLen] == serv)
                return i;
    return -1;
}

/*
 * Closes and removes a session.
 */
#undef ServerD
void ServerD (Server *serv DEBUGPARAM)
{
    ConnectionList *cl;
    Connection *clc;
    int j;

    s_repl (&serv->screen, NULL);
    for (cl = &clist; cl; cl = cl->more)
        for (j = 0; j < ConnectionListLen; j++)
            if ((clc = cl->conn[j]) && clc->serv == serv)
            {
                clc->serv = NULL;
                ConnectionD (clc);
                cl = &clist;
                j = -1;
            }

    ConnectionD (serv->conn);
    serv->conn = NULL;
    free (serv);

    Debug (DEB_CONNECT, "=S=> %p closed.", serv);
}

/*
 * Closes and removes a session.
 */
#undef ConnectionD
void ConnectionD (Connection *conn DEBUGPARAM)
{
    ConnectionList *cl;
    Connection *clc;
    int i, j, k;
    
    if ((i = ConnectionFindNr (conn)) == -1)
        return;

    Debug (DEB_CONNECT, "===> %p[%d] (%s) closing...", conn, i, ConnectionStrType (conn));
    
    UtilIOClose (conn);

    if ((i = ConnectionFindNr (conn)) == -1)
        return;

    conn->sok     = -1;
    conn->connect = 0;
    conn->serv  = NULL;
    s_free (conn->server);
    conn->server  = NULL;

    for (cl = &clist; cl; cl = cl->more)
        for (j = 0; j < ConnectionListLen; j++)
            if ((clc = cl->conn[j]) && clc->oscar_file == conn)
                clc->oscar_file = NULL;

    QueueCancel (conn);

    for (k = 0, cl = &clist; cl; cl = cl->more)
    {
        for (i = 0; i < ConnectionListLen; i++, k++)
            if (cl->conn[i] == conn)
                break;
        if (i < ConnectionListLen)
            break;
    }
    
    while (cl)
    {
        for (j = i; j + 1 < ConnectionListLen; j++)
            cl->conn[j] = cl->conn[j + 1];
        cl->conn[j] = cl->more ? cl->more->conn[0] : NULL;
        i = 0;
        cl = cl->more;
    }

    Debug (DEB_CONNECT, "===> %p[%d] closed.", conn, k);
    free (conn);
}

/*
 * Returns a string describing the session's type.
 */
const char *ConnectionStrType (Connection *conn)
{
    switch (conn->type) {
        case TYPE_XMPP_SERVER & ~TYPEF_ANY_SERVER:
            return i18n (2730, "xmpp main i/o");
        case TYPE_MSN_SERVER & ~TYPEF_ANY_SERVER:
            return i18n (2731, "msn main i/o");
        case TYPE_SERVER & ~TYPEF_ANY_SERVER:
            return i18n (2732, "icq main i/o");
        case TYPE_MSN_TEMP:
            return i18n (2584, "msn temp");
        case TYPE_MSN_CHAT:
            return i18n (2586, "msn chat");
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
        case TYPE_XMPPDIRECT:
            return i18n (2733, "xmpp peer-to-peer");
        case TYPE_FILEXMPP:
            return i18n (2734, "xmpp file io");
        case TYPE_REMOTE:
            return i18n (2225, "scripting");
        default:
            return i18n (1745, "unknown");
    }
}

const char *ServerStrType (Server *serv)
{
    switch (serv->type) {
        case TYPE_XMPP_SERVER:
            return i18n (2604, "xmpp");
        case TYPE_MSN_SERVER:
            return i18n (2735, "msn");
        case TYPE_SERVER:
            return i18n (2736, "icq");
        default:
            return i18n (1745, "unknown");
    }
}

const char *ConnectionServerType (UWORD type)
{
    switch (type) {
        case TYPE_XMPP_SERVER:
            return "xmpp";
        case TYPE_MSN_SERVER:
            return "msn";
        case TYPE_SERVER:
            return "icq8";
        case TYPE_MSGLISTEN:
            return "peer";
        case TYPE_REMOTE:
            return "remote";
        default:
            return "unknown";
    }
}

UWORD ConnectionServerNType (const char *type, char del)
{
    if (!strncmp (type, "icq8", 4)   && type[4] == del) return TYPE_SERVER;
    if (!strncmp (type, "peer", 4)   && type[4] == del) return TYPE_MSGLISTEN;
    if (!strncmp (type, "remote", 6) && type[6] == del) return TYPE_REMOTE;
    if (!strncmp (type, "msn", 3)    && type[3] == del) return TYPE_MSN_SERVER;
    if (!strncmp (type, "jabber", 6) && type[6] == del) return TYPE_XMPP_SERVER;
    if (!strncmp (type, "xmpp", 4)   && type[4] == del) return TYPE_XMPP_SERVER;
    return 0;
}


/*
 * Query an option for a contact group
 */
val_t ConnectionPrefVal (Server *serv, UDWORD flag)
{
    val_t res = 0;
    if (serv->contacts && OptGetVal (&serv->copts, flag, &res))
        return res;
    if (OptGetVal (&prG->copts, flag, &res))
        return res;
    return 0;
}

/*
 * Query an option for a contact group
 */
const char *ConnectionPrefStr (Server *serv, UDWORD flag)
{
    const char *res = 0;
    if (serv->contacts && OptGetStr (&serv->copts, flag, &res))
        return res;
    if (OptGetStr (&prG->copts, flag, &res))
        return res;
    return 0;
}

