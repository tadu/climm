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
#include "util_ui.h"
#include <string.h>
#include <netdb.h>
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

    Debug (DEB_SESSION, "<=== %p[%d] create", slist[i], i);

    return slist[i];
}

/*
 * Clones an existing session, while blanking out some values.
 */
Session *SessionClone (Session *sess, UWORD type)
{
    Session *child;
    
    child = SessionC ();
    if (!child)
        return NULL;
    memcpy (child, sess, sizeof (*child));
    child->parent   = sess;
    child->assoc    = NULL;
    child->sok      = -1;
    child->connect  = 0;
    child->incoming = NULL;
    child->type     = type;
    
    Debug (DEB_SESSION, "<=*= %p (%s) clone from %p (%s)", child, SessionType (child), sess, SessionType (sess));

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
    else if (sess->spref->type == TYPE_MSGLISTEN)
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
 * Actually, you may specify TYPEF_* here that must all be set.
 * The parent is the session this one has to have as parent.
 */
Session *SessionFind (UWORD type, UDWORD uin, const Session *parent)
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
UDWORD SessionFindNr (Session *sess)
{
    int i;

    if (!sess)
        return -1;
    for (i = 0; i < listlen; i++)
        if (slist[i] == sess)
            return i;
    return -1;
}

/*
 * Closes and removes a session.
 */
void SessionClose (Session *sess)
{
    int i, j;
    
    i = SessionFindNr (sess);
    
    assert (sess);
    assert (i != -1);
    
    Debug (DEB_SESSION, "===> %p[%d] (%s) closing...", sess, i, SessionType (sess));

    if (sess->sok != -1)
        sockclose (sess->sok);
    sess->sok     = -1;
    sess->connect = 0;
    sess->type    = 0;
    sess->parent  = NULL;

    for (j = 0; j < listlen; j++)
        if (slist[j] && slist[j]->assoc == sess)
            slist[j]->assoc = NULL;

    for (j = 0; j < listlen; j++)
        if (slist[j] && slist[j]->parent == sess)
        {
            slist[j]->parent = NULL;
            SessionClose (slist[j]);
            j--;
        }

    QueueCancel (sess);

    for (j = i; j + 1 < listlen && slist[j]; j++)
        slist[j] = slist[j + 1];
    slist[j] = NULL;

    Debug (DEB_SESSION, "===> %p[%d] closed.", sess, i);

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
        default:
            return i18n (1745, "unknown");
    }
}
