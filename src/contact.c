
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
#include "util_ui.h"   /* for Debug() and DEB_CONTACT */
#include "packet.h"    /* for capabilities */
#include "buildmark.h" /* for versioning */

static ContactGroup **cnt_groups = NULL;
static int            cnt_count = 0;

#define MAX_ENTRIES 32

#define CONTACTGROUP_GLOBAL (*cnt_groups)

#define BUILD_MIRANDA  0xffffffffL
#define BUILD_STRICQ   0xffffff8fL
#define BUILD_YSM      0xffffffabL
#define BUILD_ARQ      0xffffff7fL
#define BUILD_VICQ     0x04031980L
#define BUILD_ALICQ    0xffffffbeL

#define BUILD_LICQ     0x7d000000L
#define BUILD_SSL      0x00800000L

#define BUILD_TRILLIAN_ID1  0x3b75ac09
#define BUILD_TRILLIAN_ID2  0x3bae70b6
#define BUILD_TRILLIAN_ID3  0x3b744adb

#define BUILD_LIBICQ2K_ID1  0x3aa773ee
#define BUILD_LIBICQ2K_ID2  0x3aa66380
#define BUILD_LIBICQ2K_ID3  0x3a877a42

static void ContactGroupInit (void)
{
    if (!cnt_groups)
    {
        cnt_groups = calloc (cnt_count = 32, sizeof (ContactGroup *));
        cnt_groups[0] = calloc (1, sizeof (ContactGroup));
    }
}

/*
 * Iterates through the contact groups.
 */
ContactGroup *ContactGroupIndex (int i)
{
    if (!cnt_groups)
        ContactGroupInit ();
    if (i >= 0 && i + 1 < cnt_count && cnt_groups[i + 1])
        return cnt_groups[i + 1];
    return NULL;
}

/*
 * Finds and/or creates a contact group.
 */
ContactGroup *ContactGroupFind (UWORD id, Connection *serv, const char *name, BOOL create)
{
    int i, j;
    
    if (!cnt_groups)
        ContactGroupInit ();
    for (i = 1; i < cnt_count; i++)
        if (    cnt_groups[i]
            && (!id   || cnt_groups[i]->id   == id)
            && (!serv || cnt_groups[i]->serv == serv)
            && (!name || !strcmp (cnt_groups[i]->name, name)))
        {
            return cnt_groups[i];
        }
    if (!create || !serv || !name)
        return NULL;
    for (i = 1; i < cnt_count; i++)
        if (!cnt_groups[i])
            break;
    if (i >= cnt_count - 1)
    {
        ContactGroup **ncgp = realloc (cnt_groups, sizeof (ContactGroup *) * (2 * cnt_count));
        if (!ncgp)
            return NULL;
        cnt_groups = ncgp;
        cnt_groups[i] = NULL;
        cnt_count *= 2;
    }
    cnt_groups[i + 1] = NULL;
    cnt_groups[i] = calloc (1, sizeof (ContactGroup));
    if (!cnt_groups[i])
        return NULL;
    Debug (DEB_CONTACT, "grpadd #%d %p", i, cnt_groups[i]);
    cnt_groups[i]->id = id;
    cnt_groups[i]->serv = serv;
    cnt_groups[i]->name = strdup (name ? name : "");
    cnt_groups[i]->more = NULL;
    cnt_groups[i]->used = 0;
    for (j = 0; j < MAX_ENTRIES; j++)
        cnt_groups[i]->contacts[j] = NULL;
    return cnt_groups[i];
}

/*
 * Returns the group id, if necessary create one
 */
UWORD ContactGroupID (ContactGroup *group)
{
    while (!group->id)
    {
        ContactGroup *cg;
        int i;

        group->id = rand() % 0x7fff;
        for (i = 0; (cg = ContactGroupIndex (i)); i++)
            if (cg->id == group->id)
                group->id = 0;
    }
    return group->id;
}

/*
 * Count the number of contacts in this list.
 */
UDWORD ContactGroupCount (ContactGroup *group)
{
    UDWORD c = 0;
    while (group)
    {
        c += group->used;
        group = group->more;
    }
    return c;
}

/*
 * Remove a contact group.
 */
void ContactGroupD (ContactGroup *group)
{
    ContactGroup *tmp;
    int i;
    
    if (!cnt_groups)
        ContactGroupInit ();
    for (i = 1; i < cnt_count && cnt_groups[i]; i++)
        if (cnt_groups[i] == group)
        {
            Debug (DEB_CONTACT, "grprem #%d %p", i, group);
            memmove (cnt_groups + i, cnt_groups + i + 1, (cnt_count - i - 1) * sizeof (ContactGroup *));
            s_repl (&group->name, NULL);
            while (group)
            {
                tmp = group->more;
                free (group);
                group = tmp;
            }
            break;
        }
}

/*
 * Iterate through contacts on a contact group
 */
Contact *ContactIndex (ContactGroup *group, int i)
{
    static ContactGroup *old = NULL, *oldpos = NULL;
    static int oldoff = 0;
    ContactGroup *orig;
    int j = 0;
    
    if (!group)
        group = CONTACTGROUP_GLOBAL;
    orig = group;
    if (old == group && oldoff <= i)
    {
        group = oldpos;
        i -= oldoff;
        j = oldoff;
    }
    while (group && group->used <= i)
    {
        i -= group->used;
        j += group->used;
        group = group->more;
    }
    if (!group)
        return NULL;
    old = orig;
    oldpos = group;
    oldoff = j;
    return group->contacts[i];
}

/*
 * Finds a contact on a contact group, possibly creating it
 */
Contact *ContactFind (ContactGroup *group, UWORD id, UDWORD uin, const char *nick, BOOL create)
{
    ContactGroup *tmp, *fr = NULL;
    Contact *cont, *alias = NULL;
    int i;
    
    for (tmp = group ? group : CONTACTGROUP_GLOBAL; tmp; tmp = tmp->more)
    {
        if (tmp->used < MAX_ENTRIES)
            fr = tmp;
        for (i = 0; i < tmp->used; i++)
            for (cont = tmp->contacts[i]; cont; cont = cont->alias)
                if ((!uin  || uin == cont->uin) &&
                    (!nick || !strcmp (nick, cont->nick)) &&
                    (!id   || id == cont->id))
                {
                    return cont;
                }
        if (!tmp->more)
        {
            fr = tmp;
            break;
        }
    }
    if (!create)
        return NULL;
    if (nick && (alias = ContactFind (group, id, uin, NULL, 0))
             && alias->flags & CONT_TEMPORARY)
    {
        s_repl (&alias->nick, nick);
        alias->flags &= ~CONT_TEMPORARY;
        Debug (DEB_CONTACT, "new   #%d %ld '%s' %p in %p was temporary", id, uin, nick, alias, group);
        return alias;
    }
    if (group)
    {
        if ((cont = ContactFind (NULL, id, uin, nick, 1)))
            ContactAdd (group, cont);
        return cont;
    }
    cont = calloc (1, sizeof (Contact));
    if (!cont)
        return NULL;
    cont->uin = uin;
    cont->id = id;
    if (!nick)
    {
        cont->flags |= CONT_TEMPORARY;
        s_repl (&cont->nick, s_sprintf ("%ld", uin));
    }
    else
        s_repl (&cont->nick, nick);
    cont->status = STATUS_OFFLINE;
    cont->seen_time = -1L;
    cont->seen_micq_time = -1L;
    if (alias)
    {
        Debug (DEB_CONTACT, "alias #%d %ld '%s' %p in %p for #%d '%s'",
               id, uin, cont->nick, cont, group, alias->id, alias->nick);
        while (alias->alias)
            alias = alias->alias;
        alias->alias = cont;
        cont->flags |= CONT_ALIAS;
        return cont;
    }
    if (fr->used == MAX_ENTRIES)
        fr = fr->more = calloc (1, sizeof (ContactGroup));
    if (!fr)
        return NULL;
    fr->contacts[fr->used++] = cont;
    if (nick)
        Debug (DEB_CONTACT, "new   #%d %ld '%s' %p in %p", id, uin, nick, cont, group);
    else
        Debug (DEB_CONTACT, "temp  #%d %ld '' %p in %p", id, uin, cont, group);
    return cont;
}

/*
 * Adds a contact to a contact group.
 */
BOOL ContactAdd (ContactGroup *group, Contact *cont)
{
    ContactGroup *orig = group;
    if (!group)
        group = CONTACTGROUP_GLOBAL;
    if (!group || !cont || cont->flags & CONT_ALIAS)
        return FALSE;
    while (group->used == MAX_ENTRIES && group->more)
        group = group->more;
    if (group->used == MAX_ENTRIES)
    {
        group->more = calloc (1, sizeof (ContactGroup));
        if (!group->more)
            return FALSE;
        group = group->more;
    }
    group->contacts[group->used++] = cont;
    Debug (DEB_CONTACT, "add   #%d %ld '%s' %p to %p", cont->id, cont->uin, cont->nick, cont, orig);
    return TRUE;
}

/*
 * Removes a contact from a contact group.
 */
BOOL ContactRem (ContactGroup *group, Contact *cont)
{
    ContactGroup *orig = group;
    Contact *alias;
    int i;

    if (!group)
        group = CONTACTGROUP_GLOBAL;
    if (!group || !cont)
        return FALSE;
    while (group)
    {
        for (i = 0; i < group->used; i++)
            for (alias = group->contacts[i]; alias; alias = alias->alias)
                if (alias == cont)
                {
                    group->contacts[i] = group->contacts[--group->used];
                    group->contacts[group->used] = 0;
                    Debug (DEB_CONTACT, "rem   #%d %ld '%s' %p to %p", cont->id, cont->uin, cont->nick, cont, orig);
                    return TRUE;
                }
        group = group->more;
    }
    return FALSE;
}

/*
 * Removes an alias from a contact.
 */
BOOL ContactRemAlias (ContactGroup *group, Contact *alias)
{
    Contact *cont, *tmp;

    if (!group)
        group = CONTACTGROUP_GLOBAL;
    if (!group || !alias)
        return FALSE;
    if (alias->alias)
    {
        Debug (DEB_CONTACT, "remal #%d %ld '%s' %p to %p", alias->id, alias->uin, alias->nick, alias, group);
        tmp = alias->alias->alias;
        s_repl (&alias->nick, alias->alias->nick);
        s_repl (&alias->alias->nick, NULL);
        free (alias->alias);
        alias->alias = tmp;
        return TRUE;
    }
    cont = ContactFind (group, 0, alias->uin, NULL, 0);
    if (!cont)
        return FALSE;
    if (cont == alias)
        return ContactRem (group, alias);
    while (cont->alias != alias)
        if (!(cont = cont->alias))
            return FALSE;
    Debug (DEB_CONTACT, "remal #%d %ld '%s' %p to %p", alias->id, alias->uin, alias->nick, cont, group);
    s_repl (&alias->nick, NULL);
    free (alias);
    cont->alias = NULL;
    return TRUE;
}

/*
 * Returns the contact id, if necessary create one
 */
UWORD ContactID (Contact *cont)
{
    while (!cont->id)
    {
        Contact *c;
        int i;

        cont->id = rand() % 0x7fff;
        for (i = 0; (c = ContactIndex (NULL, i)); i++)
            if (c->id == cont->id)
                cont->id = 0;
    }
    return cont->id;
}

/*
 * Set a capability for the contact.
 */
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
            case BUILD_ALICQ:
                new = "alicq";
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
    else if (dc->id1 == BUILD_LIBICQ2K_ID1 &&
             dc->id2 == BUILD_LIBICQ2K_ID2 &&
             dc->id3 == BUILD_LIBICQ2K_ID3)
    {
        new = "libicq2000";
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
