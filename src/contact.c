
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

    memset (cont, 0, sizeof (Contact));

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
    
    ContactByUIN (uin, 0);
}

/*
 * Returns the contact list entry for UIN or NULL.
 */
Contact *ContactByUIN (UDWORD uin, BOOL create)
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
    if (!create)
        return NULL;

    return ContactAdd (uin, NULL);
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

void ContactSetCap (Contact *cont, Cap *cap)
{
    if (!cap->id)
        return;
    if (cap->id == CAP_SIM && cap->var)
    {
        UBYTE ver;
        
        ver = cap->var[15];
        cont->v1 = ver ? (ver >> 6) - 1 : 0;
        cont->v2 = ver & 0x1f;
        cont->v3 = cont->v4 = 0;
    }
    else if (cap->id == CAP_MICQ && cap->var)
    {
        cont->v1 = cap->var[12];
        cont->v2 = cap->var[13];
        cont->v3 = cap->var[14];
        cont->v4 = cap->var[15];
    }
    cont->caps |= 1 << cap->id;
}

/*
 * Guess the contacts client from time stamps.
 */
void ContactSetVersion (Contact *cont)
{
    char buf[100];
    char *new = NULL, *tail = NULL;
    unsigned int ver;
    ContactDC *dc;
    
    if (!(dc = cont->dc))
    {
        s_repl (&cont->version, NULL);
        return;
    }
    
    ver = dc->id1 & 0xffff;
    
    if (!HAS_CAP (cont->caps, CAP_SIM) && !HAS_CAP (cont->caps, CAP_MICQ))
        cont->v1 = cont->v2 = cont->v3 = cont->v4 = 0;

    if ((dc->id1 & 0xff7f0000) == BUILD_LICQ && ver > 1000)
    {
        new = "licq";
        if (dc->id1 & BUILD_SSL)
            tail = "/SSL";
        cont->v1 = ver / 1000;
        cont->v2 = (ver / 10) % 100;
        cont->v3 = ver % 10;
        cont->v4 = 0;
    }
#ifdef WIP
    else if ((dc->id1 & 0xff7f0000) == BUILD_MICQ || (dc->id1 & 0xff7f0000) == BUILD_LICQ)
    {
        new = "mICQ";
        cont->v1 = ver / 10000;
        cont->v2 = (ver / 100) % 100;
        cont->v3 = (ver / 10) % 10;
        cont->v4 = ver % 10;
        if (ver >= 489 && dc->id2)
            dc->id1 = BUILD_MICQ;
    }
#endif

    else if ((dc->id1 & 0xffff0000) == 0xffff0000)
    {
        cont->v1 = (dc->id2 & 0x7f000000) >> 24;
        cont->v2 = (dc->id2 &   0xff0000) >> 16;
        cont->v3 = (dc->id2 &     0xff00) >> 8;
        cont->v4 =  dc->id2 &       0xff;
        switch (dc->id1)
        {
            case BUILD_MIRANDA:
                if (dc->id2 <= 0x00010202 && dc->version >= 8)
                    dc->version = 7;
                new = "Miranda";
                if (dc->id2 & 0x80000000)
                    tail = " cvs";
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
                if ((cont->v1 | cont->v2 | cont->v3 | cont->v4) & 0x80)
                    cont->v1 = cont->v2 = cont->v3 = cont->v4 = 0;
                break;
            case BUILD_ARQ:
                new = "&RQ";
                break;
            default:
                snprintf (buf, sizeof (buf), "%08lx", dc->id1);
                new = buf;
        }
    }
    else if (dc->id1 == BUILD_VICQ)
    {
        cont->v1 = 0;
        cont->v2 = 43;
        cont->v3 =  dc->id2 &     0xffff;
        cont->v4 = (dc->id2 & 0x7fff0000) >> 16;
        new = "vICQ";
    }
    else if (dc->id1 == BUILD_TRILLIAN_ID1 &&
             dc->id2 == BUILD_TRILLIAN_ID2 &&
             dc->id3 == BUILD_TRILLIAN_ID3)
    {
        new = "Trillian";
    }
    else if (HAS_CAP (cont->caps, CAP_TRILL_CRYPT | CAP_TRILL_2))
        new = "Trillian";
    else if (HAS_CAP (cont->caps, CAP_LICQ))
        new = "licq";
    else if (HAS_CAP (cont->caps, CAP_SIM))
    {
        if (cont->v1 || cont->v2)
            new = "sim";
        else
            new = "Kopete";
    }
    else if (HAS_CAP (cont->caps, CAP_MICQ))
        new = "mICQ";
    else if (dc->id1 == dc->id2 && dc->id2 == dc->id3 && dc->id1 == 0xffffffff)
        new = "vICQ/GAIM(?)";
    else if (dc->version == 7 && HAS_CAP (cont->caps, CAP_IS_WEB))
        new = "ICQ2go";
    else if (dc->version == 9 && HAS_CAP (cont->caps, CAP_IS_WEB))
        new = "ICQ Lite";
    else if (HAS_CAP (cont->caps, CAP_STR_2002) && HAS_CAP (cont->caps, CAP_UTF8))
        new = "ICQ 2002";
    else if (HAS_CAP (cont->caps, CAP_STR_2001) && HAS_CAP (cont->caps, CAP_IS_2001))
        new = "ICQ 2001";
    else if (HAS_CAP (cont->caps, CAP_MACICQ))
        new = "ICQ for Mac";
    else if (dc->version == 8 && HAS_CAP (cont->caps, CAP_UTF8))
        new = "ICQ 2002 (?)";
    else if (dc->version == 8 && HAS_CAP (cont->caps, CAP_IS_2001))
        new = "ICQ 2001 (?)";
    else if (HAS_CAP (cont->caps, CAP_AIM_CHAT))
        new = "AIM(?)";
    else if (dc->version == 7 && !HAS_CAP (cont->caps, CAP_RTFMSGS))
        new = "ICQ 2000 (?)";
    
    if (new)
    {
        if (new != buf)
            strcpy (buf, new);
        if (cont->v1 || cont->v2 || cont->v3 || cont->v4)
        {
            strcat (buf, " ");
                                      sprintf (buf + strlen (buf), "%d.%d", cont->v1, cont->v2);
            if (cont->v3 || cont->v4) sprintf (buf + strlen (buf), ".%d", cont->v3);
            if (cont->v4)             sprintf (buf + strlen (buf), ".%d", cont->v4);
        }
        if (tail) strcat (buf, tail);
    }
    else
        buf[0] = '\0';

    s_repl (&cont->version, strlen (buf) ? buf : NULL);
}
