
/*
 * This file implements the contact list and basic operations on it.
 *
 * This file is Copyright © Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "micq.h"
#include "contact.h"

static int cnt_number = 0;
static Contact cnt_contacts[MAX_CONTACTS];

/*
 * Adds a new contact list entry.
 */
Contact *ContactAdd (UDWORD uin, const char *nick)
{
    Contact *cont = &cnt_contacts[cnt_number++];

    if (cnt_number == MAX_CONTACTS)
        return 0;

    cont->uin = uin;
    strncpy (cont->nick, nick, 20);
    cont->nick[19] = '\0';
    (cont + 1)->uin = 0;

    cont->invis_list = FALSE;
    cont->vis_list = FALSE;
    cont->status = STATUS_OFFLINE;
    cont->last_time = -1L;
    cont->local_ip = 0xffffffff;
    cont->outside_ip = 0xffffffff;
    cont->port = 0;
    cont->version = NULL;

    return cont;
}

/*
 * Returns the contact list entry for UIN or NULL.
 */
Contact *ContactFind (UDWORD uin)
{
    int i;
    
    for (i = 0; i < cnt_number; i++)
        if (cnt_contacts[i].uin == uin)
            return &cnt_contacts[i];
    return NULL;
}

/*
 * Returns the contact list entry for UIN, a new entry, or NULL.
 */
Contact *ContactFindM (UDWORD uin)
{
    Contact *cont;
    
    cont = ContactFind (uin);
    if (cont)
        return cont;
    ContactAdd (uin, ContactFindName (uin));
    return ContactFind (uin);
}

/*
 * Returns the nick of UIN or "<all>" or NULL.
 */
const char *ContactFindNick (UDWORD uin)
{
    Contact *cont;

    if (uin == -1)
        return strdup (i18n (725, "<all>"));
    if ((cont = ContactFind (uin)))
        return cont->nick;
    return NULL;
}

/*
 * Returns the nick of UIN or "<all>" or UIN as string.
 * Returned string needs to be free()d.
 */
char *ContactFindName (UDWORD uin)
{
    Contact *cont;
    char buff[20];

    if (uin == -1)
        return strdup (i18n (725, "<all>"));
    if ((cont = ContactFind (uin)))
        return strdup (cont->nick);
    snprintf (buff, 19, "%lu", uin);
    buff[19] = '\0';
    return strdup (buff);
}

/*
 * Returns the UIN of nick, which can be numeric, or 0.
 */
UDWORD ContactFindByNick (const char *nick)
{
    char *mynick, *p;
    int i;

    mynick = strdup (nick);
    for (p = mynick + strlen (mynick) - 1; p >= mynick && isspace (*p); p--)
        *p = '\0';

    for (p = mynick; *p; p++)
    {
        if (!isdigit (*p))
        {
            for (i = 0; i < cnt_number; i++)
            {
                if (!strncasecmp (mynick, cnt_contacts[i].nick, 19))
                {
                    free (mynick);
                    if ((SDWORD) cnt_contacts[i].uin > 0)
                        return cnt_contacts[i].uin;
                    else
                        return -cnt_contacts[i].uin;        /* alias */
                }
            }
            free (mynick);
            return -1;
        }
    }
    i = atoi (mynick);
    free (mynick);
    return i;
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
