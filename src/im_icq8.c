/*
 * Glue code between mICQ and it's ICQv8 protocol code.
 *
 * This file is Copyright © Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 */

#include "micq.h"
#include "contact.h"
#include "connection.h"
#include "oscar_snac.h"
#include "oscar_roster.h"
#include "oscar_contact.h"
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
    Contact *cont, *tcont;
    
    if (!serv || !rc)
        return NULL;
    
    cont = ContactScreen (serv, rc->name);
    if (!cont)
        return NULL;

    OptSetVal (&cont->copts, CO_ISSBL, 1);
    cont->id = 0;
    if ((tcont = ContactFind (serv->contacts, rc->id, cont->uin, NULL)))
    {
        OptSetVal (&cont->copts, CO_ISSBL, 0);
        tcont->id = 0;
    }
    cont->id = rc->id;
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

static void IMRosterShow (Event *event)
{
    Roster *roster = event->data;
    RosterEntry *rg, *rc;
    Connection *serv = event->conn;
    Contact *cont;
    int cnt_groups = 0, cnt_ignored = 0, cnt_hidden = 0, cnt_intimate = 0, cnt_normal = 0;
    
    for (rg = roster->groups; rg; rg = rg->next)
    {
        IMRosterCheckGroup (serv, rg);
        cnt_groups++;
    }

    rl_printf (i18n (9999, "Ignored contacts:\n"));
    for (rc = roster->ignore; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
        rl_printf ("  %s%s%s %s%s%s %s%s%s\n",
            COLCONTACT, rg && rg->name ? rg->name : "", COLNONE,
            COLCONTACT, rc->name, COLNONE,
            COLCONTACT, rc->nick ? rc->nick : "", COLNONE);
        if (cont)
            cnt_ignored++;
    }
    rl_printf (i18n (9999, "Hidden-from contacts:\n"));
    for (rc = roster->invisible; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
        rl_printf ("  %s%s%s %s%s%s %s%s%s\n",
            COLCONTACT, rg && rg->name ? rg->name : "", COLNONE,
            COLCONTACT, rc->name, COLNONE,
            COLCONTACT, rc->nick ? rc->nick : "", COLNONE);
        if (cont)
            cnt_hidden++;
    }
    rl_printf (i18n (9999, "Normal contacts:\n"));
    for (rc = roster->normal; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
        rl_printf ("  %s%s%s %s%s%s %s%s%s\n",
            COLCONTACT, rg && rg->name ? rg->name : "", COLNONE,
            COLCONTACT, rc->name, COLNONE,
            COLCONTACT, rc->nick ? rc->nick : "", COLNONE);
        if (cont)
            cnt_normal++;
    }
    rl_printf (i18n (9999, "Intimate contacts:\n"));
    for (rc = roster->visible; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
        rl_printf ("  %s%s%s %s%s%s %s%s%s\n",
            COLCONTACT, rg && rg->name ? rg->name : "", COLNONE,
            COLCONTACT, rc->name, COLNONE,
            COLCONTACT, rc->nick ? rc->nick : "", COLNONE);
        if (cont)
            cnt_intimate++;
    }
    IMRosterCancel (event);
    rl_printf (i18n (9999, "Showed %d contact groups, alltogether %d contacts, %d ignored, %d hidden, %d intimate.\n"),
               cnt_groups, cnt_ignored + cnt_hidden + cnt_intimate + cnt_normal, cnt_ignored, cnt_hidden, cnt_intimate);
}

static void IMRosterAdddown (Event *event)
{
    Roster *roster = event->data;
    RosterEntry *rg, *rc;
    Connection *serv = event->conn;
    ContactGroup *cg;
    Contact *cont;
    int cnt_groups = 0, cnt_ignored = 0, cnt_hidden = 0, cnt_intimate = 0, cnt_normal = 0;
    
    for (rg = roster->groups; rg; rg = rg->next)
    {
        cg = IMRosterCheckGroup (serv, rg);
        if (!cg)
        {
            rl_printf ("## %d %s\n", rg->id, rg->name);
            cg = ContactGroupC (serv, rg->id, rg->name);
            OptSetVal (&cg->copts, CO_ISSBL, 1);
            cnt_groups++;
        }
        if (cg && strcmp (rg->name, cg->name))
        {
            s_repl (&cg->name, rg->name);
            rl_printf ("##> %s\n", cg->name);
            cnt_groups++;
        }
    }
    cg = ContactGroupC (NULL, 0, "");

    rl_printf (i18n (9999, "Ignored contacts:\n"));
    for (rc = roster->ignore; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
        if (cont && !cont->group)
        {
            ContactCreate (serv, cont);
            ContactAdd (cg, cont);
            cnt_ignored++;
        }
        if (cont)
        {
            OptSetVal (&cont->copts, CO_IGNORE, 1);
            ContactAddAlias (cont, rc->nick);
            ContactAddAlias (cont, rc->name);
        }
        rl_printf ("  %s%s%s %s%s%s %s%s%s\n",
            COLCONTACT, rg && rg->name ? rg->name : "", COLNONE,
            COLCONTACT, rc->name, COLNONE,
            COLCONTACT, rc->nick ? rc->nick : "", COLNONE);
    }
    rl_printf (i18n (9999, "Hidden-from contacts:\n"));
    for (rc = roster->invisible; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
        if (cont && !cont->group)
        {
            ContactCreate (serv, cont);
            ContactAdd (cg, cont);
            cnt_hidden++;
        }
        if (cont)
        {
            OptSetVal (&cont->copts, CO_HIDEFROM, 1);
            OptSetVal (&cont->copts, CO_INTIMATE, 0);
            ContactAddAlias (cont, rc->nick);
            ContactAddAlias (cont, rc->name);
        }
        rl_printf ("  %s%s%s %s%s%s %s%s%s\n",
            COLCONTACT, rg && rg->name ? rg->name : "", COLNONE,
            COLCONTACT, rc->name, COLNONE,
            COLCONTACT, rc->nick ? rc->nick : "", COLNONE);
    }
    rl_printf (i18n (9999, "Normal contacts:\n"));
    for (rc = roster->normal; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
        if (cont && !cont->group)
        {
            ContactCreate (serv, cont);
            ContactAdd (cg, cont);
            cnt_normal++;
        }
        if (cont)
        {
            OptSetVal (&cont->copts, CO_HIDEFROM, 0);
            OptSetVal (&cont->copts, CO_INTIMATE, 0);
            ContactAddAlias (cont, rc->nick);
            ContactAddAlias (cont, rc->name);
        }
        rl_printf ("  %s%s%s %s%s%s %s%s%s\n",
            COLCONTACT, rg && rg->name ? rg->name : "", COLNONE,
            COLCONTACT, rc->name, COLNONE,
            COLCONTACT, rc->nick ? rc->nick : "", COLNONE);
    }
    rl_printf (i18n (9999, "Intimate contacts:\n"));
    for (rc = roster->visible; rc; rc = rc->next)
    {
        cont = IMRosterCheckCont (serv, rc);
        rg = IMRosterGroup (roster, rc->tag);
        if (cont && !cont->group)
        {
            ContactCreate (serv, cont);
            ContactAdd (cg, cont);
            cnt_intimate++;
        }
        if (cont)
        {
            OptSetVal (&cont->copts, CO_HIDEFROM, 0);
            OptSetVal (&cont->copts, CO_INTIMATE, 1);
            ContactAddAlias (cont, rc->nick);
            ContactAddAlias (cont, rc->name);
        }
        rl_printf ("  %s%s%s %s%s%s %s%s%s\n",
            COLCONTACT, rg && rg->name ? rg->name : "", COLNONE,
            COLCONTACT, rc->name, COLNONE,
            COLCONTACT, rc->nick ? rc->nick : "", COLNONE);
    }
    if (ContactIndex (cg, 0))
        SnacCliAddcontact (serv, NULL, cg);
    IMRosterCancel (event);
    rl_printf (i18n (9999, "Downloaded %d contact groups, alltogether %d contacts, %d ignored, %d hidden, %d intimate.\n"),
               cnt_groups, cnt_ignored + cnt_hidden + cnt_intimate + cnt_normal, cnt_ignored, cnt_hidden, cnt_intimate);
}

static void IMRosterAddup (Event *event)
{
    Roster *roster = event->data;
    RosterEntry *rg, *rc;
    Connection *serv = event->conn;
    ContactGroup *cg;
    Contact *cont;
    int i, cnt_groups = 0, cnt_normal = 0;
    
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
        cont = IMRosterCheckCont (serv, rc);

    for (rc = roster->invisible; rc; rc = rc->next)
        cont = IMRosterCheckCont (serv, rc);

    for (rc = roster->normal; rc; rc = rc->next)
        cont = IMRosterCheckCont (serv, rc);

    for (rc = roster->visible; rc; rc = rc->next)
        cont = IMRosterCheckCont (serv, rc);

    for (i = 0; (cont = ContactIndex (serv->contacts, i)); i++)    
    {
        if (!ContactPrefVal (cont, CO_ISSBL))
        {
            if (ContactPrefVal (cont, CO_WANTSBL))
            {
                cnt_normal++;
                SnacCliRosteradd (serv, cont->group, cont);
            }
            else
                rl_printf ("##Can't get value, but !issbl.\n");
        }
        else
            rl_printf ("##Can't get value.\n");
    }

    IMRosterCancel (event);
    rl_printf (i18n (9999, "Uploading %d contact groups and %d contacts.\n"),
               cnt_groups, cnt_normal);
}

UBYTE IMRoster (Connection *serv, int mode)
{
    ContactGroup *cg;
    Contact *cont;
    int i;
    
    for (i = 0, cg = serv->contacts; (cont = ContactIndex (cg, i)); i++)
        OptSetVal(&cont->copts, CO_ISSBL, 0);
    for (i = 0; (cg = ContactGroupIndex (i)); i++)
        if (cg->serv == serv)
            OptSetVal (&cg->copts, CO_ISSBL, 0);
    
    switch (mode)
    {
        case IMROSTER_DOWNLOAD:
            QueueEnqueueData2 (serv, QUEUE_REQUEST_ROSTER, SnacCliCheckroster (serv),
                               900, NULL, IMRosterAdddown, IMRosterCancel);
            break;
        case IMROSTER_UPLOAD:
            QueueEnqueueData2 (serv, QUEUE_REQUEST_ROSTER, SnacCliCheckroster (serv),
                               900, NULL, IMRosterAddup, IMRosterCancel);
            break;
        case IMROSTER_EXPORT:
        case IMROSTER_SYNC:
        case IMROSTER_SHOW:
        case IMROSTER_DIFF:
        case IMROSTER_IMPORT:
            QueueEnqueueData2 (serv, QUEUE_REQUEST_ROSTER, SnacCliCheckroster (serv),
                               900, NULL, IMRosterShow, IMRosterCancel);
            return RET_OK;
    }
    return RET_DEFER;
}

