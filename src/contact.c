
/*
 * This file implements the contact list and basic operations on it.
 *
 * This file is Copyright � R�diger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "micq.h"
#include "contact.h"
#include "util_str.h"

static int cnt_number = 0;
static Contact cnt_contacts[MAX_CONTACTS];

/*
 * Adds a new contact list entry.
 */
Contact *ContactAdd (UDWORD uin, const char *nick)
{
    Contact *cont;
    int i;
    int flags = 0;
    char buf[20];
    
    if (nick)
        for (i = 0; i < cnt_number; i++)
            if (cnt_contacts[i].uin == uin)
            {
                if (!strcmp (cnt_contacts[i].nick, nick))
                    return &cnt_contacts[i];
                flags = CONT_ALIAS;
            }
    
    cont = &cnt_contacts[cnt_number++];

    if (cnt_number == MAX_CONTACTS)
        return NULL;

    cont->uin = uin;
    if (!nick)
    {
        snprintf (buf, sizeof (buf), "%ld", uin);
        nick = buf;
        flags |= CONT_TEMPORARY;
    }
    s_repl (&cont->nick, nick);

    if (cnt_number != MAX_CONTACTS - 1)
        (cont + 1)->uin = 0;

    cont->flags = flags;
    cont->status = STATUS_OFFLINE;
    cont->seen_time = -1L;
    cont->seen_micq_time = -1L;
    cont->local_ip = 0xffffffff;
    cont->outside_ip = 0xffffffff;
    cont->port = 0;
    cont->version = 0;

    return cont;
}

/*
 * Removes a contact list entry.
 */
void ContactRem (Contact *cont)
{
    UDWORD uin;
    int i;
    
    for (i = 0; i < cnt_number; i++)
        if (&cnt_contacts[i] == cont)
            break;
    if (i == cnt_number)
        return;

    s_repl (&cont->nick, NULL);

    uin = cont->uin;
    for (cnt_number--; i < cnt_number; i++)
        cnt_contacts[i] = cnt_contacts[i + 1];

    cnt_contacts[cnt_number].uin = 0;
    
    ContactFind (uin);
}

/*
 * Returns the contact list entry for UIN or NULL.
 */
Contact *ContactFind (UDWORD uin)
{
    int i;
    
    for (i = 0; i < cnt_number; i++)
        if (cnt_contacts[i].uin == uin && ~cnt_contacts[i].flags & CONT_ALIAS)
            return &cnt_contacts[i];
    for (i = 0; i < cnt_number; i++)
        if (cnt_contacts[i].uin == uin)
        {
            cnt_contacts[i].flags &= ~CONT_ALIAS;
            return &cnt_contacts[i];
        }
    return NULL;
}

/*
 * Returns the nick of UIN or "<all>" or NULL.
 */
const char *ContactFindNick (UDWORD uin)
{
    Contact *cont;

    if (uin == -1)
        return i18n (1617, "<all>");
    if ((cont = ContactFind (uin)))
        return cont->nick;
    return NULL;
}

/*
 * Returns the nick of UIN or "<all>" or UIN as string.
 */
const char *ContactFindName (UDWORD uin)
{
    static char buff[20];
    Contact *cont;

    if (uin == -1)
        return i18n (1617, "<all>");
    if ((cont = ContactFind (uin)))
        return cont->nick;
    snprintf (buff, sizeof (buff), "%lu", uin);
    return buff;
}

/*
 * Returns the given contact/alias.
 */
Contact *ContactFindAlias (UDWORD uin, const char *nick)
{
    int i;
    
    for (i = 0; i < cnt_number; i++)
        if (cnt_contacts[i].uin == uin && !strcmp (cnt_contacts[i].nick, nick))
            return &cnt_contacts[i];
    return NULL;
}


/*
 * Returns (possibly temporary) contact for nick.
 */
Contact *ContactFindContact (const char *nick)
{
    Contact *cont;
    char *mynick, *p;
    int i;

    mynick = strdup (nick);
    for (p = mynick + strlen (mynick) - 1; p >= mynick && isspace ((int)*p); p--)
        *p = '\0';

    for (p = mynick; *p; p++)
    {
        if (!isdigit ((int)*p))
        {
            for (i = 0; i < cnt_number; i++)
            {
                if (!strncasecmp (mynick, cnt_contacts[i].nick, 19))
                {
                    free (mynick);
                    return &cnt_contacts[i];
                }
            }
            free (mynick);
            return NULL;
        }
    }
    if ((cont = ContactFind (atoi (mynick))))
        return cont;

    cont = ContactAdd (atoi (mynick), NULL);
    free (mynick);
    return cont ? cont : NULL;
}

/*
 * Returns the UIN of nick, which can be numeric, or -1.
 */
UDWORD ContactFindByNick (const char *nick)
{
    char *mynick, *p;
    int i;

    mynick = strdup (nick);
    for (p = mynick + strlen (mynick) - 1; p >= mynick && isspace ((int)*p); p--)
        *p = '\0';

    for (p = mynick; *p; p++)
    {
        if (!isdigit ((int)*p))
        {
            for (i = 0; i < cnt_number; i++)
            {
                if (!strncasecmp (mynick, cnt_contacts[i].nick, 19))
                {
                    free (mynick);
                    return cnt_contacts[i].uin;
                }
            }
            free (mynick);
            return -1;
        }
    }
    i = atoi (mynick);
    free (mynick);
    return i ? i : -1;
}

/*
 * Iterates through the contact list.
 */
Contact *ContactStart ()
{
    return &cnt_contacts[0];
}

Contact *ContactNext (Contact *cont)
{
    return cont + 1;
}

BOOL ContactHasNext (Contact *cont)
{
    return cont->uin != 0;
}
