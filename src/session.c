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
#include "preferences.h"
#include <assert.h>

#define listlen 10

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
    
    slist[i]->our_local_ip   = 0x0100007f;
    slist[i]->our_outside_ip = 0x0100007f;
    slist[i]->status = STATUS_OFFLINE;
    slist[i]->sok = -1;

    return slist[i];
}

void SessionClose (Session *sess)
{
    int i;
    
    for (i = 0; slist[i] != sess && i < listlen; i++)  ;
    
    assert (i < listlen);
    
    slist[i] = NULL;

    if (sess->sok != -1)
        sockclose (sess->sok);
    free (sess);
}

Session *SessionFind (UBYTE type, UDWORD uin)
{
    int i;
    
    for (i = 0; i < listlen; i++)
        if (slist[i] && slist[i]->spref && slist[i]->spref->type & type
                                        && slist[i]->spref->uin == uin)
            return slist[i];
    if (uin)
        return NULL;
    for (i = 0; i < listlen; i++)
        if (slist[i] && slist[i]->spref && slist[i]->spref->type & type)
            return slist[i];
    return (NULL);
}

Session *SessionNr (int i)
{
    if (i >= listlen)
        return NULL;
    return slist[i];
}
