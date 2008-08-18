/*
 * Glue code between climm and it's ICQv8 protocol code.
 *
 * climm Copyright (C) © 2001-2007 Rüdiger Kuhlmann
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

static Contact *IMRosterCheckCont (Server *serv, RosterEntry *rc)
{
    Contact *cont;
    
    if (!serv || !rc)
        return NULL;
    
    cont = ContactScreen (serv, rc->name);
    if (!cont)
        return NULL;

    if (rc->type == roster_normal)
    {
        OptSetVal (&cont->copts, CO_ISSBL, 1);
        if (rc->reqauth)
            cont->oldflags |= CONT_REQAUTH;
    }
    ContactIDSet (cont, rc->type, rc->id, rc->tag);
    return cont;
}

static ContactGroup *IMRosterCheckGroup (Server *serv, RosterEntry *rg)
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
    Server *serv = event->conn->serv;
    
    if (!roster)
        return;

    if (roster->delname)
    {
        for (re = roster->groups; re; re = re->next)
            if (!strcmp (re->name, roster->delname) || (re->nick && !strcmp (re->nick, roster->delname)))
                SnacCliRosterentrydelete (serv, re);

        for (re = roster->ignore; re; re = re->next)
            if (!strcmp (re->name, roster->delname) || (re->nick && !strcmp (re->nick, roster->delname)))
                SnacCliRosterentrydelete (serv, re);

        for (re = roster->invisible; re; re = re->next)
            if (!strcmp (re->name, roster->delname) || (re->nick && !strcmp (re->nick, roster->delname)))
                SnacCliRosterentrydelete (serv, re);

        for (re = roster->visible; re; re = re->next)
            if (!strcmp (re->name, roster->delname) || (re->nick && !strcmp (re->nick, roster->delname)))
                SnacCliRosterentrydelete (serv, re);

        for (re = roster->normal; re; re = re->next)
            if (!strcmp (re->name, roster->delname) || (re->nick && !strcmp (re->nick, roster->delname)))
                SnacCliRosterentrydelete (serv, re);

        for (re = roster->generic; re; re = re->next)
            if (!strcmp (re->name, roster->delname) || (re->nick && !strcmp (re->nick, roster->delname)))
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
    Server *serv = event->conn->serv;
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

static void IMRosterDownCont (Server *serv, Roster *roster, ContactGroup *cg, RosterEntry *rc, int *pcnt, int *pmod, char force, UDWORD flag)
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
    if (rc->nick && strcmp (rc->nick, rc->name) && !ContactFindAlias (cont, rc->nick))
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
    Server *serv = event->conn->serv;
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
            cg = ContactGroupC (serv, rg->tag, rg->name);
            OptSetVal (&cg->copts, CO_ISSBL, 1);
            rl_printf (i18n (9999, "Added contact group %s (#%d).\n"), rg->name, rg->tag);
            cnt_groups++;
        }
        else if (cg && strcmp (rg->name, cg->name))
        {
            s_repl (&cg->name, rg->name);
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
 /*   if (ContactIndex (cg, 0))               */
 /*       SnacCliAddcontact (serv, NULL, cg); */
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
    Server *serv = event->conn->serv;
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
            cg = ContactGroupC (serv, rg->tag, rg->name);
            OptSetVal (&cg->copts, CO_ISSBL, 1);
            rl_printf (i18n (9999, "Added contact group %s (#%d).\n"), rg->name, rg->tag);
            cnt_groups++;
        }
        else if (strcmp (rg->name, cg->name))
        {
            s_repl (&cg->name, rg->name);
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
 /*   if (ContactIndex (cg, 0))               */
 /*       SnacCliAddcontact (serv, NULL, cg); */
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
    Server *serv = event->conn->serv;
    ContactGroup *cg, *tcg;
    Contact *cont;
    int i, cnt_groups = 0, cnt_normal = 0;
    
    if (!roster)
        return;
    
    for (rg = roster->groups; rg; rg = rg->next)
        cg = IMRosterCheckGroup (serv, rg);

    for (i = 0; (cg = ContactGroupIndex (i)); i++)
        if (cg->serv == serv && !ContactGroupPrefVal (cg, CO_ISSBL))
            if (ContactGroupPrefVal (cg, CO_WANTSBL))
            {
                cnt_groups++;
                SnacCliRosteraddgroup (serv, cg, 3);
            }
    
    for (rc = roster->ignore; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
    }

    for (rc = roster->invisible; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
    }

    cg = ContactGroupC (NULL, 0, "");
    for (rc = roster->normal; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
        tcg = IMRosterCheckGroup (serv, rg);
        if (tcg && cont->group && tcg != cont->group && tcg == cont->group->serv->contacts)
            ContactAdd (cg, cont);
    }
    SnacCliRosterbulkmove (serv, cg, 3);
    ContactGroupD (cg);

    for (rc = roster->visible; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
    }

    cg = ContactGroupC (NULL, 0, "");
    for (i = 0; (cont = ContactIndex (serv->contacts, i)); i++)
        if (!ContactPrefVal (cont, CO_ISSBL))
            if (ContactPrefVal (cont, CO_WANTSBL) && !ContactPrefVal (cont, CO_IGNORE) && cnt_normal < 25)
            {
                cnt_normal++;
                cont->oldflags |= CONT_REQAUTH;
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
    Server *serv = event->conn->serv;
    ContactGroup *cg, *tcg;
    Contact *cont;
    int i, cnt_more = 0, cnt_auto = 0;
    int cnt_groups = 0, cnt_ignored = 0, cnt_hidden = 0, cnt_intimate = 0, cnt_normal = 0;
    
    if (!roster)
        return;

    for (rg = roster->groups; rg; rg = rg->next)
    {
        cg = IMRosterCheckGroup (serv, rg);
        if (!cg)
        {
            rl_printf (i18n (2694, "Group %s (#%d) exists only on the server.\n"), rg->name, rg->id);
            cnt_groups++;
        }
        else if (cg && strcmp (rg->name, cg->name))
        {
            rl_printf (i18n (2695, "Group %s (#%d) on the server is group %s (#%d) locally.\n"), rg->name, rg->id, cg->name, cg->id);
            cnt_groups++;
        }
    }
    for (i = 0; (cg = ContactGroupIndex (i)); i++)
        if (cg->serv == serv && !ContactGroupPrefVal (cg, CO_ISSBL))
        {
            rl_printf (i18n (2696, "Group %s (#%d) exists only locally.\n"), cg->name, cg->id);
            cnt_groups++;
        }
    

    for (rc = roster->ignore; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
        if (!cont || !cont->group || (!ContactPrefVal (cont, CO_IGNORE) && ContactPrefVal (cont, CO_WANTSBL)))
        {
            rl_printf (i18n (2697, "Contact %s/%s exists only on the server as ignored (#%d).\n"),
                       rc->name, rc->nick ? rc->nick : rc->name, rc->id);
            cnt_ignored++;
        }
    }
    for (rc = roster->invisible; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
        if (!cont || !cont->group)
        {
            rl_printf (i18n (2698, "Contact %s/%s exists only on the server as invisible (#%d).\n"),
                       rc->name, rc->nick ? rc->nick : rc->name, rc->id);
            cnt_hidden++;
        }
        else if (!ContactPrefVal (cont, CO_HIDEFROM) && ContactPrefVal (cont, CO_WANTSBL))
        {
            rl_printf (i18n (2699, "Contact %s/%s is invisible (#%d) on the server, but locally (#%d) normal.\n"),
                       rc->name, cont->screen, rc->id, ContactID (cont, roster_normal)->id);
            cnt_hidden++;
        }
    }
    for (rc = roster->visible; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
        if (!cont || !cont->group)
        {
            rl_printf (i18n (2700, "Contact %s/%s exists only on the server as visible (#%d).\n"),
                       rc->name, rc->nick ? rc->nick : rc->name, rc->id);
            cnt_intimate++;
        }
        else if (!ContactPrefVal (cont, CO_INTIMATE) && ContactPrefVal (cont, CO_WANTSBL))
        {
            rl_printf (i18n (2701, "Contact %s/%s is visible (#%d) on the server, but locally (#%d) normal.\n"),
                       rc->name, cont->screen, rc->id, ContactID (cont, roster_normal)->id);
            cnt_intimate++;
        }
    }

    for (rc = roster->normal; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
        if (!cont || !cont->group)
        {
            rl_printf (i18n (2702, "Contact %s/%s exists only on the server (#%d).\n"),
                       rc->name, rc->nick ? rc->nick : rc->name, rc->id);
            cnt_normal++;
            continue;
        }
        if (!ContactPrefVal (cont, CO_WANTSBL))
            continue;
        if (strcmp (rc->name, cont->screen))
        {
            rl_printf (i18n (2703, "Contact %s (#%d) is screen name %s (#%d) locally. Huh??\n"),
                rc->name, rc->id, cont->screen, ContactID (cont, roster_normal)->id);
            cnt_normal++;
        }
        if (strcmp (rc->nick ? rc->nick : rc->name, cont->nick))
        {
            rl_printf (i18n (2704, "Contact %s is %s (#%d) on the server and %s (#%d) locally.\n"),
                cont->screen, rc->nick ? rc->nick : rc->name, rc->id, cont->nick, ContactID (cont, roster_normal)->id);
            cnt_normal++;
        }
        if (ContactPrefVal (cont, CO_HIDEFROM) && !ContactID (cont, roster_invisible)->issbl)
        {
            rl_printf (i18n (2705, "Contact %s/%s is normal (#%d) on the server, but invisible (#%d,#%d) locally.\n"),
                       cont->screen, cont->nick, rc->id, ContactID (cont, roster_normal)->id, ContactID (cont, roster_invisible)->id);
            cnt_hidden++;
        }
        if (ContactPrefVal (cont, CO_INTIMATE) && !ContactID (cont, roster_visible)->issbl)
        {
            rl_printf (i18n (2706, "Contact %s/%s is normal (#%d) on the server, but visible (#%d,#%d) locally.\n"),
                       cont->screen, cont->nick, rc->id, ContactID (cont, roster_normal)->id, ContactID (cont, roster_visible)->id);
            cnt_intimate++;
        }
        if (ContactPrefVal (cont, CO_IGNORE) && !ContactID (cont, roster_ignore)->issbl)
        {
            rl_printf (i18n (2710, "Contact %s/%s is normal (#%d) on the server, but ignored (#%d,#%d) locally.\n"),
                       cont->screen, cont->nick, rc->id, ContactID (cont, roster_normal)->id, ContactID (cont, roster_ignore)->id);
            cnt_ignored++;
        }
        if (rg->tag != cont->group->id)
        {
            tcg = IMRosterCheckGroup (serv, rg);
            if (tcg && cont->group && tcg != cont->group && tcg == cont->group->serv->contacts)
                cnt_auto++;

            rl_printf (i18n (2707, "Contact %s/%s (#%d) is in group %s (#%d) on the server, but in group %s (#%d) locally.\n"),
                       cont->screen, cont->nick, rc->id, rg && rg->name ? rg->name : "?", rg->tag,
                       cont->group == serv->contacts ? "(none)" : cont->group->name, cont->group->id);
            cnt_normal++;
        }
    }
    for (i = 0; (cont = ContactIndex (serv->contacts, i)); i++)    
        if (ContactPrefVal (cont, CO_WANTSBL) && (!ContactID (cont, roster_normal)->issbl || !ContactPrefVal (cont, CO_ISSBL)))
        {
            rl_printf (i18n (2708, "Contact %s/%s (#%d) exists only locally.\n"),
                       cont->screen, cont->nick, ContactID (cont, roster_normal)->id);
            cnt_more++;
        }

    IMRosterCancel (event);
    rl_printf (i18n (2491, "Differences in %d contact groups, alltogether %d contacts, %d ignored, %d hidden, %d intimate, %d local.\n"),
               cnt_groups, cnt_ignored + cnt_hidden + cnt_intimate + cnt_normal + cnt_more,
               cnt_ignored, cnt_hidden, cnt_intimate, cnt_more);
    if (prG->autoupdate == 5)
    {
        if (cnt_more || cnt_auto)
            IMRoster (serv, IMROSTER_UPLOAD);
        else
            prG->autoupdate++;
    }
}

UBYTE IMDeleteID (Server *serv, int tag, int id, const char *name)
{
    Roster *roster;
    
    roster = OscarRosterC ();
    roster->deltag = tag;
    roster->delid = id;
    roster->delname = name ? strdup (name) : NULL;
    QueueEnqueueData2 (serv->conn, QUEUE_REQUEST_ROSTER, SnacCliCheckroster (serv),
                       900, roster, IMRosterDoDelete, IMRosterCancel);
    return RET_OK;
}


UBYTE IMRoster (Server *serv, int mode)
{
    ContactGroup *cg;
    ContactIDs *ids;
    Contact *cont;
    int i;
    
    for (i = 0, cg = serv->contacts; (cont = ContactIndex (cg, i)); i++)
    {
        OptSetVal(&cont->copts, CO_ISSBL, 0);
        cont->oldflags &= ~CONT_REQAUTH;
        for (ids = cont->ids; ids; ids = ids->next)
            ids->issbl = 0;
    }
    for (i = 0; (cg = ContactGroupIndex (i)); i++)
        if (cg->serv == serv)
            OptSetVal (&cg->copts, CO_ISSBL, 0);
    
    switch (mode)
    {
        case IMROSTER_IMPORT:
            QueueEnqueueData2 (serv->conn, QUEUE_REQUEST_ROSTER, SnacCliCheckroster (serv),
                               900, NULL, IMRosterOverwritedown, IMRosterCancel);
            break;
        case IMROSTER_DOWNLOAD:
            QueueEnqueueData2 (serv->conn, QUEUE_REQUEST_ROSTER, SnacCliCheckroster (serv),
                               900, NULL, IMRosterAdddown, IMRosterCancel);
            break;
        case IMROSTER_EXPORT:
        case IMROSTER_UPLOAD:
            QueueEnqueueData2 (serv->conn, QUEUE_REQUEST_ROSTER, SnacCliCheckroster (serv),
                               900, NULL, IMRosterAddup, IMRosterCancel);
            break;
        case IMROSTER_DIFF:
            QueueEnqueueData2 (serv->conn, QUEUE_REQUEST_ROSTER, SnacCliCheckroster (serv),
                               900, NULL, IMRosterDiff, IMRosterCancel);
            break;
        case IMROSTER_SYNC:
        case IMROSTER_SHOW:
            QueueEnqueueData2 (serv->conn, QUEUE_REQUEST_ROSTER, SnacCliCheckroster (serv),
                               900, NULL, IMRosterShow, IMRosterCancel);
            return RET_OK;
    }
    return RET_DEFER;
}

