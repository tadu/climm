/*
 * Handles a list of recently used UINs that are to be recalled by pressing
 * tab.
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
#include "tabs.h"
#include "contact.h"

#define TAB_SLOTS 16

static const Contact *tab_in[TAB_SLOTS + 1];
static const Contact *tab_out[TAB_SLOTS + 1];
static time_t tab_last;

void TabInit (void)
{
    Contact *cont;
    int i;
    
    for (i = 0; i <= TAB_SLOTS; i++)
        tab_in[i] = tab_out[i] = NULL;
    for (i = 0; (cont = ContactIndex (NULL, i)); i++)
        if (ContactPrefVal (cont, CO_TABSPOOL))
            TabAddOut (cont);
    tab_last = time (NULL);
}

/*
 * Appends an UIN to the incoming list. A previous occurrence is deleted.
 * Last entry might get lost.
 */
void TabAddIn (const Contact *cont)
{
    int found;

    for (found = 0; found < TAB_SLOTS + 1; found++)
        if (tab_out[found] == cont)
            return;

    for (found = 0; found < TAB_SLOTS; found++)
        if (tab_in[found] == cont)
            break;

    for (; found; found--)
        tab_in[found] = tab_in[found - 1];

    tab_in[0] = cont;
}

/*
 * Appends an UIN to the outgoing list. A previous occurrence is deleted.
 * Last entry might get lost.
 */
void TabAddOut (const Contact *cont)
{
    int found;

    for (found = 0; found < TAB_SLOTS; found++)
        if (tab_out[found] == cont)
            break;

    for (; found; found--)
        tab_out[found] = tab_out[found - 1];

    tab_out[0] = cont;

    for (found = 0; found < TAB_SLOTS + 1; found++)
        if (tab_in[found] == cont)
        {
            for ( ; found < TAB_SLOTS; found++)
                tab_in[found] = tab_in[found + 1];
            tab_in[TAB_SLOTS] = NULL;
        }
}

/*
 * Returns the nr.th entry, or NULL.
 */
const Contact *TabGetIn (int nr)
{
    return nr > TAB_SLOTS ? NULL : tab_in[nr];
}

/*
 * Returns the nr.th entry, or NULL.
 */
const Contact *TabGetOut (int nr)
{
    time_t now;
    if (!nr && (now = time (NULL)) - tab_last > 300)
    {
        int i, found;
        const Contact *cont;

        i = (now - tab_last) / 300;
        for (found = 0; found < TAB_SLOTS + 1; found++)
            if (!tab_out[found])
                break;
        while (i-- && found > 0)
        {
            found--;
            cont = tab_out[found];
            tab_out[found] = NULL;
            TabAddIn (cont);
        }
        tab_last = now;
    }
    return nr > TAB_SLOTS ? NULL : tab_out[nr];
}

/*
 * Check whether a contact is tabbed
 */
int TabHas (const Contact *cont)
{
    int found;

    for (found = 0; found < TAB_SLOTS; found++)
        if (tab_out[found] == cont || tab_in[found] == cont)
            return 1;
    return 0;
}
