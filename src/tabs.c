/* $Id$ */

/*
 * Handles a list of recently used UINs that are to be recalled by pressing
 * tab.
 *
 * This file is Copyright © Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
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

    if (!ContactFind (uin))
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
