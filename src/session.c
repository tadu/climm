/*
 * Manages the available connections (note the file is misnamed)
 *
 * This file is Copyright © Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 */

#include "micq.h"
#include "session.h"
#include "cmd_pkt_cmd_v5_util.h"
#include "cmd_pkt_v8.h"
#include "tcp.h"
#include "preferences.h"
#include <string.h>
#include <assert.h>

#define listlen 40

static Session *slist[listlen] = { 0 };

/*
 * Creates a new session.
 */
Session *SessionC (void)
{
    int i;
    
    for (i = 0; slist[i] && i < listlen; i++)  ;
     
    if (i == listlen)
        return NULL;

    slist[i] = calloc (1, sizeof (Session));
    
    assert (slist[i]);
    
    slist[i]->our_local_ip   = 0x7f000001;
    slist[i]->our_outside_ip = 0x7f000001;
    slist[i]->status = STATUS_OFFLINE;
    slist[i]->sok = -1;

    return slist[i];
}

/*
 * Clones an existing session, while blanking out some values.
 */
Session *SessionClone (Session *sess)
{
    Session *child;
    
    child = SessionC ();
    if (!child)
        return NULL;
    memcpy (child, sess, sizeof (*child));
    child->assoc = sess;
    child->sok = -1;
    child->connect = 0;
    
    return child;
}

/*
 * Initializes a session.
 */
void SessionInit (Session *sess)
{
    if (sess->connect & CONNECT_OK)
        return;
    if (!sess->spref)
        return;
    if (sess->spref->type == TYPE_SERVER)
        SessionInitServer (sess);
    else if (sess->spref->type == TYPE_SERVER_OLD)
        SessionInitServerV5 (sess);
    else
        SessionInitPeer (sess);
}

/*
 * Returns the n-th session.
 */
Session *SessionNr (int i)
{
    if (i >= listlen)
        return NULL;
    return slist[i];
}

/*
 * Finds a session of given type and/or given uin.
 */
Session *SessionFind (UBYTE type, UDWORD uin)
{
    int i;
    
    for (i = 0; i < listlen; i++)
        if (slist[i] && (slist[i]->type == type) && (slist[i]->uin == uin) && uin)
            return slist[i];
    if (uin)
        return NULL;
    for (i = 0; i < listlen; i++)
        if (slist[i] && (slist[i]->type == type) && (slist[i]->connect & CONNECT_OK))
            return slist[i];
    return NULL;
}

/*
 * Closes and removes a session.
 */
void SessionClose (Session *sess)
{
    int i;
    
    for (i = 0; slist[i] != sess && i < listlen; i++)  ;
    
    assert (sess);
    assert (i < listlen);
    
    while (i + 1 < listlen && slist[i])
    {
        slist[i] = slist[i + 1];
        i++;
    }
    slist[i] = NULL;

    if (sess->sok != -1)
        sockclose (sess->sok);
    free (sess);
}

/*
 * Returns a string describing the session's type.
 */
const char *SessionType (Session *sess)
{
    switch (sess->type) {
        case TYPE_SERVER:
            return i18n (1889, "server");
        case TYPE_SERVER_OLD:
            return i18n (1744, "server (v5)");
        case TYPE_PEER:
            return i18n (1947, "listener");
        case TYPE_DIRECT:
            return i18n (1890, "peer-to-peer");
        default:
            return i18n (1745, "unknown");
    }
}
