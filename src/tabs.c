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

static UDWORD tab_array[TAB_SLOTS + 1] = { 0 };
static int tab_pointer = 0;

/*
 * Appends an UIN to the list. A previous occurrence is deleted.
 * Last entry might get lost.
 */
void TabAddUIN (UDWORD uin)
{
    int found;

    if (!ContactFind (NULL, 0, uin, NULL, 0))
        return ;

    for (found = 0; found < TAB_SLOTS; found++)
        if (tab_array[found] == uin)
            break;

    for (; found; found--)
        tab_array[found] = tab_array[found - 1];

    tab_array[0] = uin;
    tab_array[TAB_SLOTS] = 0;
    tab_pointer = 0;
}

/*
 * Returns != 0, if there is another entry in the list.
 */
int TabHasNext (void)
{
    return tab_array[tab_pointer];
}

/*
 * Returns next UIN, if the list is not empty; wraps around if necessary.
 */
UDWORD TabGetNext (void)
{
    if (!tab_array[tab_pointer])
        tab_pointer = 0;
    return tab_array[tab_pointer++];
}

/*
 * Resets pointer of current UIN in list to the beginning.
 */
void TabReset (void)
{
    tab_pointer = 0;
}
