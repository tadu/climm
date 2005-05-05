/*
 * Glue code between mICQ and it's ICQv8 protocol code.
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
 * In addition, as a special exception permission is granted to link the
 * code of this release of mICQ with the OpenSSL project's "OpenSSL"
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

#include "micq.h"
#include "contact.h"
#include "connection.h"
#include "oscar_snac.h"
#include "oscar_roster.h"
#include "oscar_contact.h"
#include "preferences.h"
#include "im_icq8.h"

static void IMRosterCancel (Event *event)
{
    if (event->data)
        OscarRosterD (event->data);
    event->data = NULL;
    EventD (event);
}

static RosterEntry *IMRosterGroup (Roster *roster, UWORD id)
{
    RosterEntry *re;
    
    for (re = roster->groups; re; re = re->next)
        if (re->tag == id)
            return re;
    return NULL;
}

static Contact *IMRosterCheckCont (Connection *serv, RosterEntry *rc)
{
    Contact *cont;
    
    if (!serv || !rc)
        return NULL;
    
    cont = ContactScreen (serv, rc->name);
    if (!cont)
        return NULL;

    OptSetVal (&cont->copts, CO_ISSBL, 1);
    ContactIDSet (cont, rc->type, rc->id, rc->tag);
    return cont;
}

static ContactGroup *IMRosterCheckGroup (Connection *serv, RosterEntry *rg)
{
    ContactGroup *cg, *tcg;

    if (!serv || !rg)
        return NULL;
    
    cg = ContactGroupFind (serv, 0, rg->name);
    if (!cg)
        cg = ContactGroupFind (serv, rg->tag, NULL);
    if (!cg)
        return NULL;

    OptSetVal (&cg->copts, CO_ISSBL, 1);
    cg->id = 0;
    if ((tcg = ContactGroupFind (serv, rg->tag, NULL)))
    {
        OptSetVal (&cg->copts, CO_ISSBL, 0);
        tcg->id = 0;
    }
    cg->id = rg->tag;
    return cg;
}

static void IMRosterDoDelete (Event *event)
{
    Roster *roster = event->data;
    RosterEntry *re;
    Connection *serv = event->conn;
    
    if (!roster)
        return;

    if (roster->delname)
    {
        for (re = roster->groups; re; re = re->next)
            if (!strcmp (re->name, roster->delname))
                SnacCliRosterentrydelete (serv, re);

        for (re = roster->ignore; re; re = re->next)
            if (!strcmp (re->name, roster->delname))
                SnacCliRosterentrydelete (serv, re);

        for (re = roster->invisible; re; re = re->next)
            if (!strcmp (re->name, roster->delname))
                SnacCliRosterentrydelete (serv, re);

        for (re = roster->visible; re; re = re->next)
            if (!strcmp (re->name, roster->delname))
                SnacCliRosterentrydelete (serv, re);

        for (re = roster->normal; re; re = re->next)
            if (!strcmp (re->name, roster->delname))
                SnacCliRosterentrydelete (serv, re);

        for (re = roster->generic; re; re = re->next)
            if (!strcmp (re->name, roster->delname))
                SnacCliRosterentrydelete (serv, re);
    }
    else
    {
        for (re = roster->groups; re; re = re->next)
            if (re->tag == roster->deltag && re->id == roster->delid)
                SnacCliRosterentrydelete (serv, re);

        for (re = roster->ignore; re; re = re->next)
            if (re->tag == roster->deltag && re->id == roster->delid)
                SnacCliRosterentrydelete (serv, re);

        for (re = roster->invisible; re; re = re->next)
            if (re->tag == roster->deltag && re->id == roster->delid)
                SnacCliRosterentrydelete (serv, re);

        for (re = roster->visible; re; re = re->next)
            if (re->tag == roster->deltag && re->id == roster->delid)
                SnacCliRosterentrydelete (serv, re);

        for (re = roster->normal; re; re = re->next)
            if (re->tag == roster->deltag && re->id == roster->delid)
                SnacCliRosterentrydelete (serv, re);

        for (re = roster->generic; re; re = re->next)
            if (re->tag == roster->deltag && re->id == roster->delid)
                SnacCliRosterentrydelete (serv, re);
    }

    IMRosterCancel (event);
}

static void IMRosterShow (Event *event)
{
    Roster *roster = event->data;
    RosterEntry *rg, *rc;
    Connection *serv = event->conn;
    Contact *cont = NULL;
    int cnt_groups = 0, cnt_ignored = 0, cnt_hidden = 0, cnt_intimate = 0, cnt_normal = 0;
    
    if (!roster)
        return;
    
    rl_printf (i18n (2535, "Groups:\n"));
    for (rg = roster->groups; rg; rg = rg->next)
    {
        IMRosterCheckGroup (serv, rg);
        if (prG->verbose)
            rl_printf ("  %6d", rg->tag);
        rl_printf ("  %s%s%s\n",
            COLCONTACT, rg && rg->name ? rg->name : "", COLNONE);
        cnt_groups++;
    }

    rl_printf (i18n (2473, "Normal contacts:\n"));
    for (rc = roster->normal; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
        if (prG->verbose)
            rl_printf ("  %6d", rc->tag);
        rl_printf ("  %s%-25s%s %s%15s%s",
            COLCONTACT, rg && rg->name ? rg->name : "", COLNONE,
            COLCONTACT, rc->name, COLNONE);
        if (prG->verbose)
            rl_printf ("  %6d", rc->id);
        rl_printf (" %s%s%s\n",
            COLCONTACT, rc->nick ? rc->nick : "", COLNONE);
        if (cont)
            cnt_normal++;
    }
    rl_printf (i18n (2471, "Ignored contacts:\n"));
    for (rc = roster->ignore; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
        if (prG->verbose)
            rl_printf ("  %6d", rc->tag);
        rl_printf ("  %s%-25s%s %s%15s%s",
            COLCONTACT, rg && rg->name ? rg->name : "", COLNONE,
            COLCONTACT, rc->name, COLNONE);
        if (prG->verbose)
            rl_printf ("  %6d", rc->id);
        rl_printf (" %s%s%s\n",
            COLCONTACT, rc->nick ? rc->nick : "", COLNONE);
        if (cont)
            cnt_ignored++;
    }
    rl_printf (i18n (2472, "Hidden-from contacts:\n"));
    for (rc = roster->invisible; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
        if (prG->verbose)
            rl_printf ("  %6d", rc->tag);
        rl_printf ("  %s%-25s%s %s%15s%s",
            COLCONTACT, rg && rg->name ? rg->name : "", COLNONE,
            COLCONTACT, rc->name, COLNONE);
        if (prG->verbose)
            rl_printf ("  %6d", rc->id);
        rl_printf (" %s%s%s\n",
            COLCONTACT, rc->nick ? rc->nick : "", COLNONE);
        if (cont)
            cnt_hidden++;
    }
    rl_printf (i18n (2474, "Intimate contacts:\n"));
    for (rc = roster->visible; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
        if (prG->verbose)
            rl_printf ("  %6d", rc->tag);
        rl_printf ("  %s%-25s%s %s%15s%s",
            COLCONTACT, rg && rg->name ? rg->name : "", COLNONE,
            COLCONTACT, rc->name, COLNONE);
        if (prG->verbose)
            rl_printf ("  %6d", rc->id);
        rl_printf (" %s%s%s\n",
            COLCONTACT, rc->nick ? rc->nick : "", COLNONE);
        if (cont)
            cnt_intimate++;
    }
    IMRosterCancel (event);
    rl_printf (i18n (2475, "Showed %d contact groups, alltogether %d contacts, %d ignored, %d hidden, %d intimate.\n"),
               cnt_groups, cnt_ignored + cnt_hidden + cnt_intimate + cnt_normal, cnt_ignored, cnt_hidden, cnt_intimate);
}

static void IMRosterDownCont (Connection *serv, Roster *roster, ContactGroup *cg, RosterEntry *rc, int *pcnt, int *pmod, char force, UDWORD flag)
{
    ContactGroup *tcg = NULL;
    Contact *cont;
    RosterEntry *rg;
    char mod = 0;
    char cnt = 0;
    
    rg = IMRosterGroup (roster, rc->tag);
    cont = IMRosterCheckCont (serv, rc);

    rl_printf ("  %s%-25s%s %s%15s%s %s%s%s\n",
        COLCONTACT, rg && rg->name ? rg->name : "", COLNONE,
        COLCONTACT, rc->name, COLNONE,
        COLCONTACT, rc->nick ? rc->nick : "", COLNONE);

    if (!cont)
        return;

    if (!cont->group)
    {
        ContactCreate (serv, cont);
        ContactAdd (cg, cont);
        OptSetVal (&cont->copts, CO_IGNORE,   flag == CO_IGNORE);
        OptSetVal (&cont->copts, CO_HIDEFROM, flag == CO_HIDEFROM);
        OptSetVal (&cont->copts, CO_INTIMATE, flag == CO_INTIMATE);
        mod = cnt = 1;
        (*pcnt)++;
    }
    else if (force)
    {
        if (ContactPrefVal (cont, CO_LOCAL))
        {
            OptSetVal (&cont->copts, CO_LOCAL,    0);
            mod = 1;
        }
        if (ContactPrefVal (cont, CO_IGNORE) != (flag == CO_IGNORE))
        {
            OptSetVal (&cont->copts, CO_IGNORE,   flag == CO_IGNORE);
            mod = 1;
        }
        if (ContactPrefVal (cont, CO_HIDEFROM) != (flag == CO_HIDEFROM))
        {
            OptSetVal (&cont->copts, CO_HIDEFROM, flag == CO_HIDEFROM);
            mod = 1;
        }
        if (ContactPrefVal (cont, CO_INTIMATE) != (flag == CO_INTIMATE))
        {
            OptSetVal (&cont->copts, CO_INTIMATE, flag == CO_INTIMATE);
            mod = 1;
        }
    }
    if (rg)
        tcg = IMRosterCheckGroup (serv, rg);
    if (tcg && !ContactHas (tcg, cont) && (force || cont->group == serv->contacts))
    {
        ContactAdd (tcg, cont);
        cont->group = tcg;
        mod = 1;
    }
    ContactAddAlias (cont, rc->name);
    if (mod || (rc->nick && strcmp (rc->nick, rc->name) && cont != ContactFind (serv->contacts, 0, cont->uin, rc->nick)))
    {
        ContactAddAlias (cont, rc->nick);
        mod = 1;
    }
    if (mod && !cnt)
        (*pmod)++;
}

static void IMRosterAdddown (Event *event)
{
    Roster *roster = event->data;
    RosterEntry *rg, *rc;
    Connection *serv = event->conn;
    ContactGroup *cg;
    int cnt_groups = 0, cnt_ignored = 0, cnt_hidden = 0, cnt_intimate = 0, cnt_normal = 0;
    int mod_groups = 0, mod_ignored = 0, mod_hidden = 0, mod_intimate = 0, mod_normal = 0;
    
    if (!roster)
        return;

    for (rg = roster->groups; rg; rg = rg->next)
    {
        cg = IMRosterCheckGroup (serv, rg);
        if (!cg)
        {
            rl_printf ("## %d %s\n", rg->tag, rg->name);
            cg = ContactGroupC (serv, rg->tag, rg->name);
            OptSetVal (&cg->copts, CO_ISSBL, 1);
            cnt_groups++;
        }
        else if (cg && strcmp (rg->name, cg->name))
        {
            s_repl (&cg->name, rg->name);
            rl_printf ("##> %s\n", cg->name);
            mod_groups++;
        }
    }
    cg = ContactGroupC (NULL, 0, "");

    rl_printf (i18n (2473, "Normal contacts:\n"));
    for (rc = roster->normal; rc; rc = rc->next)
        IMRosterDownCont (serv, roster, cg, rc, &cnt_normal, &mod_normal, FALSE, 0);
    rl_printf (i18n (2471, "Ignored contacts:\n"));
    for (rc = roster->ignore; rc; rc = rc->next)
        IMRosterDownCont (serv, roster, cg, rc, &cnt_ignored, &mod_ignored, FALSE, CO_IGNORE);
    rl_printf (i18n (2472, "Hidden-from contacts:\n"));
    for (rc = roster->invisible; rc; rc = rc->next)
        IMRosterDownCont (serv, roster, cg, rc, &cnt_hidden, &mod_hidden, FALSE, CO_HIDEFROM);
    rl_printf (i18n (2474, "Intimate contacts:\n"));
    for (rc = roster->visible; rc; rc = rc->next)
        IMRosterDownCont (serv, roster, cg, rc, &cnt_intimate, &mod_intimate, FALSE, CO_INTIMATE);
    if (ContactIndex (cg, 0))
        SnacCliAddcontact (serv, NULL, cg);
    IMRosterCancel (event);
    rl_printf (i18n (2476, "Downloaded %d contact groups, alltogether %d contacts, %d ignored, %d hidden, %d intimate.\n"),
               cnt_groups, cnt_ignored + cnt_hidden + cnt_intimate + cnt_normal, cnt_ignored, cnt_hidden, cnt_intimate);
    rl_printf (i18n (2578, "Modified %d contact groups, alltogether %d contacts, %d ignored, %d hidden, %d intimate.\n"),
               mod_groups, mod_ignored + mod_hidden + mod_intimate + mod_normal, mod_ignored, mod_hidden, mod_intimate);
}

static void IMRosterOverwritedown (Event *event)
{
    Roster *roster = event->data;
    RosterEntry *rg, *rc;
    Connection *serv = event->conn;
    ContactGroup *cg;
    int cnt_groups = 0, cnt_ignored = 0, cnt_hidden = 0, cnt_intimate = 0, cnt_normal = 0;
    int mod_groups = 0, mod_ignored = 0, mod_hidden = 0, mod_intimate = 0, mod_normal = 0;
    
    if (!roster)
        return;

    for (rg = roster->groups; rg; rg = rg->next)
    {
        cg = IMRosterCheckGroup (serv, rg);
        if (!cg)
        {
            rl_printf ("## %d %s\n", rg->tag, rg->name);
            cg = ContactGroupC (serv, rg->tag, rg->name);
            OptSetVal (&cg->copts, CO_ISSBL, 1);
            cnt_groups++;
        }
        else if (strcmp (rg->name, cg->name))
        {
            s_repl (&cg->name, rg->name);
            rl_printf ("##> %s\n", cg->name);
            mod_groups++;
        }
    }
    cg = ContactGroupC (NULL, 0, "");

    rl_printf (i18n (2473, "Normal contacts:\n"));
    for (rc = roster->normal; rc; rc = rc->next)
        IMRosterDownCont (serv, roster, cg, rc, &cnt_intimate, &mod_intimate, TRUE, 0);
    rl_printf (i18n (2471, "Ignored contacts:\n"));
    for (rc = roster->ignore; rc; rc = rc->next)
        IMRosterDownCont (serv, roster, cg, rc, &cnt_ignored, &mod_ignored, TRUE, CO_IGNORE);
    rl_printf (i18n (2472, "Hidden-from contacts:\n"));
    for (rc = roster->invisible; rc; rc = rc->next)
        IMRosterDownCont (serv, roster, cg, rc, &cnt_hidden, &mod_hidden, TRUE, CO_HIDEFROM);
    rl_printf (i18n (2474, "Intimate contacts:\n"));
    for (rc = roster->visible; rc; rc = rc->next)
        IMRosterDownCont (serv, roster, cg, rc, &cnt_normal, &mod_normal, TRUE, CO_INTIMATE);
    if (ContactIndex (cg, 0))
        SnacCliAddcontact (serv, NULL, cg);
    IMRosterCancel (event);
    rl_printf (i18n (2476, "Downloaded %d contact groups, alltogether %d contacts, %d ignored, %d hidden, %d intimate.\n"),
               cnt_groups, cnt_ignored + cnt_hidden + cnt_intimate + cnt_normal, cnt_ignored, cnt_hidden, cnt_intimate);
    rl_printf (i18n (2578, "Modified %d contact groups, alltogether %d contacts, %d ignored, %d hidden, %d intimate.\n"),
               mod_groups, mod_ignored + mod_hidden + mod_intimate + mod_normal, mod_ignored, mod_hidden, mod_intimate);
}

static void IMRosterAddup (Event *event)
{
    Roster *roster = event->data;
    RosterEntry *rg, *rc;
    Connection *serv = event->conn;
    ContactGroup *cg;
    Contact *cont;
    int i, cnt_groups = 0, cnt_normal = 0;
    
    if (!roster)
        return;
    
    for (rg = roster->groups; rg; rg = rg->next)
        IMRosterCheckGroup (serv, rg);
    
    for (i = 0; (cg = ContactGroupIndex (i)); i++)
        if (cg->serv == serv && !ContactGroupPrefVal (cg, CO_ISSBL))
            if (ContactGroupPrefVal (cg, CO_WANTSBL))
            {
                cnt_groups++;
                SnacCliRosteradd (serv, cg, NULL);
            }
    
    for (rc = roster->ignore; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
        if (cont->group && cont->group->id && rg && rg->id && cont->group->id != rg->id)
            SnacCliRosterupdate (serv, cont->group, cont);
    }

    for (rc = roster->invisible; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
        if (cont->group && cont->group->id && rg && rg->id && cont->group->id != rg->id)
            SnacCliRosterupdate (serv, cont->group, cont);
    }

    for (rc = roster->normal; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
        if (cont->group && cont->group->id && rg && rg->id && cont->group->id != rg->id)
            SnacCliRosterupdate (serv, cont->group, cont);
    }

    for (rc = roster->visible; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
        if (cont->group && cont->group->id && rg && rg->id && cont->group->id != rg->id)
            SnacCliRosterupdate (serv, cont->group, cont);
    }

    cg = ContactGroupC (NULL, 0, "");
    for (i = 0; (cont = ContactIndex (serv->contacts, i)); i++)
        if (!ContactPrefVal (cont, CO_ISSBL))
            if (ContactPrefVal (cont, CO_WANTSBL) && !ContactPrefVal (cont, CO_IGNORE))
            {
                cnt_normal++;
                ContactAdd (cg, cont);
            }
    if (ContactIndex (cg, 0))
        SnacCliRosterbulkadd (serv, cg);

    IMRosterCancel (event);
    rl_printf (i18n (2477, "Uploading %d contact groups and %d contacts.\n"),
               cnt_groups, cnt_normal);
}

static void IMRosterDiff (Event *event)
{
    Roster *roster = event->data;
    RosterEntry *rg, *rc;
    Connection *serv = event->conn;
    ContactGroup *cg;
    Contact *cont;
    int i, cnt_more = 0;
    int cnt_groups = 0, cnt_ignored = 0, cnt_hidden = 0, cnt_intimate = 0, cnt_normal = 0;
    
    if (!roster)
        return;

    for (rg = roster->groups; rg; rg = rg->next)
    {
        cg = IMRosterCheckGroup (serv, rg);
        if (!cg)
        {
            rl_printf (i18n (2478, "Server: Group %s (#%d)\n"), rg->name, rg->id);
            cnt_groups++;
        }
        if (cg && strcmp (rg->name, cg->name))
        {
            rl_printf (i18n (2478, "Server: Group %s (#%d)\n"), rg->name, rg->id);
            rl_printf (i18n (2479, "Local:  Group %s (#%d)\n"), cg->name, cg->id);
            cnt_groups++;
        }
    }
    for (i = 0; (cg = ContactGroupIndex (i)); i++)
        if (cg->serv == serv && !ContactGroupPrefVal (cg, CO_ISSBL))
        {
            rl_printf (i18n (2479, "Local:  Group %s (#%d)\n"), cg->name, cg->id);
            cnt_groups++;
        }
    

    for (rc = roster->ignore; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
        if (!cont || !cont->group || (!ContactPrefVal (cont, CO_IGNORE) && ContactPrefVal (cont, CO_WANTSBL)))
        {
            rl_printf (i18n (2480, "Server: Group %s Contact %s (#%d) <ignored>\n"),
                       rg && rg->name ? rg->name : "?", rc->nick ? rc->nick : rc->name, rc->id);
            cnt_ignored++;
        }
    }
    for (rc = roster->invisible; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
        if (!cont || !cont->group || (!ContactPrefVal (cont, CO_HIDEFROM) && ContactPrefVal (cont, CO_WANTSBL)))
        {
            rl_printf (i18n (2481, "Server: Group %s Contact %s (#%d) <hidefrom>\n"),
                       rg && rg->name ? rg->name : "?", rc->nick ? rc->nick : rc->name, rc->id);
            cnt_hidden++;
        }
        if (cont && cont->group && !ContactPrefVal (cont, CO_HIDEFROM) && ContactPrefVal (cont, CO_WANTSBL))
        {
            rl_printf (i18n (2482, "Server: Group %s Contact %s/%s (#%d) <hidefrom>\n"),
                       rg && rg->name ? rg->name : "?", rc->name, rc->nick ? rc->nick : rc->name, rc->id);
            rl_printf (i18n (2483, "Local:  Group %s Contact %s/%s (#%d)\n"),
                       cont->group == serv->contacts ? "(none)" : cont->group->name,
                       s_sprintf ("%ld", cont->uin), cont->nick, 0);
        }
    }
    for (rc = roster->normal; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
        if (!cont || !cont->group)
        {
            rl_printf (i18n (2484, "Server: Group %s Contact %s (#%d)\n"),
                       rg && rg->name ? rg->name : "?", rc->nick ? rc->nick : rc->name, rc->id);
            cnt_normal++;
        }
        if (cont && cont->group && (ContactPrefVal (cont, CO_HIDEFROM) || ContactPrefVal (cont, CO_INTIMATE)) && ContactPrefVal (cont, CO_WANTSBL))
        {
            rl_printf (i18n (2485, "Server: Group %s Contact %s/%s (#%d)\n"),
                       rg && rg->name ? rg->name : "?", rc->name, rc->nick ? rc->nick : rc->name, rc->id);
            rl_printf (i18n (2486, "Local:  Group %s Contact %s/%s (#%d) <%s>\n"),
                       cont->group == serv->contacts ? "(none)" : cont->group->name,
                       s_sprintf ("%ld", cont->uin), cont->nick, 0,
                       ContactPrefVal (cont, CO_HIDEFROM) ? i18n (2487, "hidefrom") : i18n (2488, "intimate"));
        }
    }
    for (rc = roster->visible; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
        if (cont && !cont->group)
        {
            rl_printf (i18n (2484, "Server: Group %s Contact %s (#%d)\n"),
                       rg && rg->name ? rg->name : "?", rc->nick ? rc->nick : rc->name, rc->id);
            cnt_intimate++;
        }
        if (cont && cont->group && !ContactPrefVal (cont, CO_INTIMATE) && ContactPrefVal (cont, CO_WANTSBL))
        {
            rl_printf (i18n (2489, "Server: Group %s Contact %s/%s (#%d) <intimate>\n"),
                       rg && rg->name ? rg->name : "?", rc->name, rc->nick ? rc->nick : rc->name, rc->id);
            rl_printf (i18n (2483, "Local:  Group %s Contact %s/%s (#%d)\n"),
                       cont->group == serv->contacts ? "(none)" : cont->group->name,
                       s_sprintf ("%ld", cont->uin), cont->nick, 0);
        }
    }

    for (i = 0; (cont = ContactIndex (serv->contacts, i)); i++)    
        if (ContactPrefVal (cont, CO_WANTSBL))
        {
            if (ContactPrefVal (cont, CO_HIDEFROM) && !ContactID (cont, ROSTER_TYPE_INVISIBLE)->issbl)
                rl_printf (i18n (2486, "Local:  Group %s Contact %s/%s (#%d) <%s>\n"),
                           cont->group == serv->contacts ? "(none)" : cont->group->name,
                           s_sprintf ("%ld", cont->uin), cont->nick, ContactIDGet (cont, ROSTER_TYPE_INVISIBLE),
                           i18n (2487, "hidefrom"));
            else if (ContactPrefVal (cont, CO_INTIMATE) && !ContactID (cont, ROSTER_TYPE_VISIBLE)->issbl)
                rl_printf (i18n (2486, "Local:  Group %s Contact %s/%s (#%d) <%s>\n"),
                           cont->group == serv->contacts ? "(none)" : cont->group->name,
                           s_sprintf ("%ld", cont->uin), cont->nick, ContactIDGet (cont, ROSTER_TYPE_VISIBLE),
                           i18n (2488, "intimate"));
            else if (ContactPrefVal (cont, CO_IGNORE) && !ContactID (cont, ROSTER_TYPE_IGNORE)->issbl)
                rl_printf (i18n (2486, "Local:  Group %s Contact %s/%s (#%d) <%s>\n"),
                           cont->group == serv->contacts ? "(none)" : cont->group->name,
                           s_sprintf ("%ld", cont->uin), cont->nick, ContactIDGet (cont, ROSTER_TYPE_IGNORE),
                           i18n (2580, "ignore"));
            else if (!ContactID (cont, ROSTER_TYPE_NORMAL)->issbl || !ContactPrefVal (cont, CO_ISSBL))
                rl_printf (i18n (2486, "Local:  Group %s Contact %s/%s (#%d) <%s>\n"),
                           cont->group == serv->contacts ? "(none)" : cont->group->name,
                           s_sprintf ("%ld", cont->uin), cont->nick, ContactIDGet (cont, ROSTER_TYPE_IGNORE),
                           i18n (2490, "normal"));
            else
                cnt_more--;
            cnt_more++;
        }

    IMRosterCancel (event);
    rl_printf (i18n (2491, "Differences in %d contact groups, alltogether %d contacts, %d ignored, %d hidden, %d intimate, %d local.\n"),
               cnt_groups, cnt_ignored + cnt_hidden + cnt_intimate + cnt_normal + cnt_more,
               cnt_ignored, cnt_hidden, cnt_intimate, cnt_more);
}

UBYTE IMDeleteID (Connection *conn, int tag, int id, const char *name)
{
    Roster *roster;
    
    roster = OscarRosterC ();
    roster->deltag = tag;
    roster->delid = id;
    roster->delname = name ? strdup (name) : NULL;
    QueueEnqueueData2 (conn, QUEUE_REQUEST_ROSTER, SnacCliCheckroster (conn),
                       900, roster, IMRosterDoDelete, IMRosterCancel);
    return RET_OK;
}


UBYTE IMRoster (Connection *serv, int mode)
{
    ContactGroup *cg;
    ContactIDs *ids;
    Contact *cont;
    int i;
    
    for (i = 0, cg = serv->contacts; (cont = ContactIndex (cg, i)); i++)
    {
        OptSetVal(&cont->copts, CO_ISSBL, 0);
        for (ids = cont->ids; ids; ids = ids->next)
            ids->issbl = 0;
    }
    for (i = 0; (cg = ContactGroupIndex (i)); i++)
        if (cg->serv == serv)
            OptSetVal (&cg->copts, CO_ISSBL, 0);
    
    switch (mode)
    {
        case IMROSTER_IMPORT:
            QueueEnqueueData2 (serv, QUEUE_REQUEST_ROSTER, SnacCliCheckroster (serv),
                               900, NULL, IMRosterOverwritedown, IMRosterCancel);
            break;
        case IMROSTER_DOWNLOAD:
            QueueEnqueueData2 (serv, QUEUE_REQUEST_ROSTER, SnacCliCheckroster (serv),
                               900, NULL, IMRosterAdddown, IMRosterCancel);
            break;
        case IMROSTER_EXPORT:
        case IMROSTER_UPLOAD:
            QueueEnqueueData2 (serv, QUEUE_REQUEST_ROSTER, SnacCliCheckroster (serv),
                               900, NULL, IMRosterAddup, IMRosterCancel);
            break;
        case IMROSTER_DIFF:
            QueueEnqueueData2 (serv, QUEUE_REQUEST_ROSTER, SnacCliCheckroster (serv),
                               900, NULL, IMRosterDiff, IMRosterCancel);
            break;
        case IMROSTER_SYNC:
        case IMROSTER_SHOW:
            QueueEnqueueData2 (serv, QUEUE_REQUEST_ROSTER, SnacCliCheckroster (serv),
                               900, NULL, IMRosterShow, IMRosterCancel);
            return RET_OK;
    }
    return RET_DEFER;
}

