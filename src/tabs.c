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

static Contact *tab_in[TAB_SLOTS + 1];
static Contact *tab_out[TAB_SLOTS + 1];

void TabInit (void)
{
    Contact *cont;
    int i;
    
    for (i = 0; i <= TAB_SLOTS; i++)
        tab_in[i] = tab_out[i] = NULL;
    for (i = 0; (cont = ContactIndex (NULL, i)); i++)
        if (ContactPrefVal (cont, CO_TABSPOOL))
            TabAddOut (cont);
}

/*
 * Appends an UIN to the incoming list. A previous occurrence is deleted.
 * Last entry might get lost.
 */
void TabAddIn (Contact *cont)
{
    int found;

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
void TabAddOut (Contact *cont)
{
    int found;

    for (found = 0; found < TAB_SLOTS; found++)
        if (tab_out[found] == cont)
            break;

    for (; found; found--)
        tab_out[found] = tab_out[found - 1];

    tab_out[0] = cont;
}

/*
 * Returns the nr.th entry, or NULL.
 */
Contact *TabGetIn (int nr)
{
    return nr > TAB_SLOTS ? NULL : tab_in[nr];
}

/*
 * Returns the nr.th entry, or NULL.
 */
Contact *TabGetOut (int nr)
{
    return nr > TAB_SLOTS ? NULL : tab_out[nr];
}

/*
 * Returns the nr.th entry of both list, or NULL.
 */
Contact *TabGet (int nr)
{
    static int off = TAB_SLOTS;
    int tnr;
    
    if (nr < off && tab_out[nr])
        return tab_out[nr];
    if (nr < TAB_SLOTS && !tab_out[nr])
        off = nr;
    tnr = nr - off;
    if (!tab_in[tnr])
        off = TAB_SLOTS;
    return tab_in[tnr];
}
