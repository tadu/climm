/*
 * Handles a list of recently used UINs that are to be recalled by pressing
 * tab.
 *
 * climm Copyright (C) © 2001,2002,2003,2004 Rüdiger Kuhlmann
 *
 * climm is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 dated June, 1991.
 *
 * climm is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * In addition, as a special exception permission is granted to link the
 * code of this release of climm with the OpenSSL project's "OpenSSL"
 * library, and distribute the linked executables.  You must obey the GNU
 * General Public License in all respects for all of the code used other
 * than "OpenSSL".  If you modify this file, you may extend this exception
 * to your version of the file, but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your version
 * of this file.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * $Id$
 */

#include "climm.h"
#include "tabs.h"
#include "contact.h"

#define TAB_SLOTS 32
#define TAB_IN_OFFSET (5 * 60)

static const Contact *tab_list[TAB_SLOTS + 1];
static time_t         tab_stamp[TAB_SLOTS + 1];

void TabInit (void)
{
    Contact *cont;
    int i;
    
    for (i = 0; i <= TAB_SLOTS; i++)
    {
        tab_list[i] = NULL;
        tab_stamp[i] = 0;
    }
    for (i = 0; (cont = ContactIndex (NULL, i)); i++)
        if (ContactPrefVal (cont, CO_TABSPOOL))
            TabAddOut (cont);
}

/*
 * Inserts an UIN into the tab list. Last entry might get lost.
 */
static void tab_add (const Contact *cont, int offset)
{
    time_t istamp = time (NULL) - offset, tstamp;
    const Contact *tcont, *icont = cont;
    int found = 0;
    
    for (found = 0; found <= TAB_SLOTS && tab_stamp[found] > istamp; found++)
        if (tab_list[found] == cont)
            return;

    for ( ; found <= TAB_SLOTS; found++)
    {
        tstamp = tab_stamp[found];
        tcont  = tab_list[found];
        tab_stamp[found] = istamp;
        tab_list[found]  = icont;
        if (!tcont || tcont == cont)
            return;
        icont  = tcont;
        istamp = tstamp;
    }
}

/*
 * Inserts an UIN into the list, applying time delta for incoming messages.
 * Last entry might get lost.
 */
void TabAddIn (const Contact *cont)
{
    tab_add (cont, TAB_IN_OFFSET);
}

/*
 * Inserts an UIN into the list without time delta.
 * Last entry might get lost.
 */
void TabAddOut (const Contact *cont)
{
    tab_add (cont, 0);
}

/*
 * Returns the nr.th entrys time stamp, or NULL.
 */
const Contact *TabGet (int nr)
{
    return nr > TAB_SLOTS ? NULL : tab_list[nr];
}

/*
 * Returns the nr.th entry, or NULL.
 */
time_t TabTime (int nr)
{
    return nr > TAB_SLOTS ? 0 : tab_stamp[nr];
}

/*
 * Check whether a contact is tabbed
 */
int TabHas (const Contact *cont)
{
    int found;

    for (found = 0; found <= TAB_SLOTS; found++)
        if (tab_list[found] == cont)
            return 1;
    return 0;
}
