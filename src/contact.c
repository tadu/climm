
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
#include "packet.h"    /* for capabilities */
#include "buildmark.h" /* for versioning */

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

/*
 * Guess the contacts client from time stamps.
 */
void ContactSetVersion (Contact *cont)
{
    char buf[100];
    char *new = NULL;
    unsigned int ver = cont->id1 & 0xffff, ssl = 0;
    unsigned char v1 = 0, v2 = 0, v3 = 0, v4 = 0;

    if ((cont->id1 & 0xff7f0000) == BUILD_LICQ && ver > 1000)
    {
        new = "licq";
        if (cont->id1 & BUILD_SSL)
            ssl = 1;
        v1 = ver / 1000;
        v2 = (ver / 10) % 100;
        v3 = ver % 10;
        v4 = 0;
    }
#ifdef WIP
    else if ((cont->id1 & 0xff7f0000) == BUILD_MICQ || (cont->id1 & 0xff7f0000) == BUILD_LICQ)
    {
        new = "mICQ";
        v1 = ver / 10000;
        v2 = (ver / 100) % 100;
        v3 = (ver / 10) % 10;
        v4 = ver % 10;
        if (ver >= 489 && cont->id2)
            cont->id1 = BUILD_MICQ;
    }
#endif

    else if ((cont->id1 & 0xffff0000) == 0xffff0000)
    {
        v1 = (cont->id2 & 0x7f000000) >> 24;
        v2 = (cont->id2 &   0xff0000) >> 16;
        v3 = (cont->id2 &     0xff00) >> 8;
        v4 =  cont->id2 &       0xff;
        switch (cont->id1)
        {
            case BUILD_MIRANDA:
                if (cont->id2 <= 0x00010202 && cont->TCP_version >= 8)
                    cont->TCP_version = 7;
                new = "Miranda";
                break;
            case BUILD_STRICQ:
                new = "StrICQ";
                break;
            case BUILD_MICQ:
                cont->seen_micq_time = time (NULL);
                new = "mICQ";
                break;
            case BUILD_YSM:
                new = "YSM";
                if ((v1 | v2 | v3 | v4) & 0x80)
                    v1 = v2 = v3 = v4 = 0;
                break;
            case BUILD_ARQ:
                new = "&RQ";
                break;
            default:
                snprintf (buf, sizeof (buf), "%08lx", cont->id1);
                new = buf;
        }
    }
    else if (cont->id1 == BUILD_VICQ)
    {
        v1 = 0;
        v2 = 43;
        v3 =  cont->id2 &     0xffff;
        v4 = (cont->id2 & 0x7fff0000) >> 16;
        new = "vICQ";
    }
    else if (cont->id1 == BUILD_TRILLIAN_ID1 &&
             cont->id2 == BUILD_TRILLIAN_ID2 &&
             cont->id3 == BUILD_TRILLIAN_ID3)
    {
        new = "Trillian 0.73/0.74/Pro 1.0";
    }
    else if (cont->id1 == cont->id2 && cont->id2 == cont->id3 && cont->id1 == 0xffffffff)
        new = "vICQ/GAIM(?)";
    else if (HAS_CAP (cont->caps, CAP_IS_WEB))
        new = "ICQ2go";
    else if (HAS_CAP (cont->caps, CAP_TRILL_CRYPT | CAP_TRILL_2))
        new = "Trillian";
    else if (HAS_CAP (cont->caps, CAP_LICQ))
        new = "licq";
    else if (HAS_CAP (cont->caps, CAP_SIM))
        new = "sim";
    else if (HAS_CAP (cont->caps, CAP_STR_2002))
        new = "ICQ 2002";
    else if (HAS_CAP (cont->caps, CAP_STR_2001))
        new = "ICQ 2001";
    else if (HAS_CAP (cont->caps, CAP_MACICQ))
        new = "ICQ for Mac";
    else if (cont->TCP_version == 8 && HAS_CAP (cont->caps, CAP_IS_2002))
        new = "ICQ 2002 (?)";
    else if (cont->TCP_version == 8 && HAS_CAP (cont->caps, CAP_IS_2001))
        new = "ICQ 2001 (?)";
    else if (HAS_CAP (cont->caps, CAP_AIM_CHAT))
        new = "AIM(?)";
    else if (cont->TCP_version == 7 && !HAS_CAP (cont->caps, CAP_RTFMSGS))
        new = "ICQ 2000 (?)";
    
    if (new)
    {
        if (new != buf)
            strcpy (buf, new);
        if (v1 || v2 || v3 || v4)
        {
            strcat (buf, " ");
                          sprintf (buf + strlen (buf), "%d.%d", v1, v2);
            if (v3 || v4) sprintf (buf + strlen (buf), ".%d", v3);
            if (v4)       sprintf (buf + strlen (buf), ".%d", v4);
        }
        if (ssl) strcat (buf, "/SSL");
    }
    else
        buf[0] = '\0';

    s_repl (&cont->version, strlen (buf) ? buf : NULL);
}
