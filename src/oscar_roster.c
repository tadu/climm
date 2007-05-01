/*
 * Handles incoming and creates outgoing SNAC packets
 * for the family 19 (roster) commands.
 *
 * mICQ Copyright (C) © 2001-2005 Rüdiger Kuhlmann
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
 * As a special exemption, you may link your binary against OpenSSL,
 * provided your operating system distribution doesn't ship libgnutls.
 *
 * $Id$
 */

#include "micq.h"
#include <assert.h>
#include "oscar_base.h"
#include "oscar_tlv.h"
#include "oscar_snac.h"
#include "oscar_service.h"
#include "oscar_contact.h"
#include "oscar_roster.h"
#include "oscar_oldicq.h"
#include "packet.h"
#include "contact.h"
#include "connection.h"
#include "im_icq8.h"
#include "conv.h"
#include "file_util.h"
#include "util_ui.h"
#include "icq_response.h"

#define TLV_REQAUTH    102
#define TLV_GROUPITEMS 200
#define TLV_UNKNIDLE   201
#define TLV_PRIVACY    202
#define TLV_VISIBILITY 203
#define TLV_ALLOWIDLE  204
#define TLV_ICQTIC     205
#define TLV_IMPORT     212
#define TLV_ICON       213
#define TLV_NICK       305
#define TLV_LOCALMAIL  311
#define TLV_LOCALSMS   314
#define TLV_LOCALCOMM  316
#define TLV_LOCALACT   317
#define TLV_LOCALSOUND 318
#define TLV_LASTUPD    325

Roster *OscarRosterC (void)
{
    return calloc (sizeof (Roster), 1);
}

static void OscarRosterEntryD (RosterEntry *entry)
{
    RosterEntry *next;
    
    while (entry)
    {
        s_free (entry->name);
        s_free (entry->nick);
        TLVD (entry->tlv);
        next = entry->next;
        free (entry);
        entry = next;
    }
}

void OscarRosterD (Roster *roster)
{
    OscarRosterEntryD (roster->generic);
    OscarRosterEntryD (roster->groups);
    OscarRosterEntryD (roster->normal);
    OscarRosterEntryD (roster->visible);
    OscarRosterEntryD (roster->invisible);
    OscarRosterEntryD (roster->ignore);
    s_free (roster->delname);
    free (roster);
}

/*
 * CLI_REQLISTS - SNAC(13,2)
 */

/* implemented as macro */

/*
 * SRV_REPLYLISTS - SNAC(13,3)
 */
JUMP_SNAC_F(SnacSrvReplylists)
{
    Connection *serv = event->conn;

    SnacCliSetstatus (serv, serv->status, 3);
    SnacCliReady (serv);
    SnacCliAddcontact (serv, NULL, serv->contacts);
    SnacCliReqofflinemsgs (serv);
    SnacCliReqinfo (serv);
    if (serv->flags & CONN_WIZARD)
    {
        IMRoster (serv, IMROSTER_IMPORT);
        if (serv->flags & CONN_INITWP)
        {
            Contact *cont = ContactScreen (serv, serv->screen);
            CONTACT_GENERAL (cont);
            CONTACT_MORE (cont);
            SnacCliMetasetabout (serv, "mICQ");
            SnacCliMetasetgeneral (serv, cont);
            SnacCliMetasetmore (serv, cont);
        }
    }
    else if (ContactGroupPrefVal (serv->contacts, CO_WANTSBL))
        IMRoster (serv, IMROSTER_DIFF);
}

/*
 * CLI_REQROSTER - SNAC(13,4)
 */

/* implemented as macro */

/*
 * CLI_CHECKROSTER - SNAC(13,5)
 */
UDWORD SnacCliCheckroster (Connection *serv)
{
    ContactGroup *cg;
    Packet *pak;
    UDWORD ref;
    int i;
    
    cg = serv->contacts;
    pak = SnacC (serv, 19, 5, 0, 0);
    for (i = 0; (/*cont =*/ ContactIndex (cg, i)); )
        i++;

    PacketWriteB4 (pak, 0);  /* last modification of server side contact list */
    PacketWriteB2 (pak, 0);  /* # of entries */
    ref = pak->ref;
    SnacSend (serv, pak);
    return ref;
}

/*
 * SRV_REPLYROSTER - SNAC(13,6)
 */
JUMP_SNAC_F(SnacSrvReplyroster)
{
    Connection *serv;
    Packet *pak;
    Event *event2;
    Roster *roster;
    RosterEntry **rref;
    RosterEntry *re;
    int i, k, l;
    int cnt_sbl_add, cnt_sbl_chn, cnt_sbl_del;
    int cnt_loc_add, cnt_loc_chn, cnt_loc_del;
    strc_t cname;
    UWORD count, j, TLVlen;
    time_t stmp;

    if (!event)
        return;

    serv = event->conn;
    if (!serv)
        return;

    pak = event->pak;
    
    if (!pak)
        return;
    
    event2 = QueueDequeue2 (serv, QUEUE_REQUEST_ROSTER, 0, 0);
    if (!event2 || !event2->callback)
    {
        DebugH (DEB_PROTOCOL, "Unrequested roster packet received.\n");
        return;
    }

    roster = event2->data;
    if (!roster)
    {
        roster = OscarRosterC ();
        event2->data = roster;
    }
    
    PacketRead1 (pak);

    count = PacketReadB2 (pak);          /* COUNT */
    cnt_sbl_add = cnt_sbl_chn = cnt_sbl_del = 0;
    cnt_loc_add = cnt_loc_chn = cnt_loc_del = 0;
    for (i = k = l = 0; i < count; i++)
    {
        re = calloc (sizeof (RosterEntry), 1);
        
        cname  = PacketReadB2Str (pak, NULL);   /* GROUP NAME */
        re->tag  = PacketReadB2 (pak);     /* TAG  */
        re->id   = PacketReadB2 (pak);     /* ID   */
        re->type = PacketReadB2 (pak);     /* TYPE */
        TLVlen = PacketReadB2 (pak);     /* TLV length */
        re->tlv  = TLVRead (pak, TLVlen, -1);
        
        re->name = strdup (ConvFromServ (cname));

        j = TLVGet (re->tlv, TLV_NICK);
        assert (j < 200 || j == (UWORD)-1);
        if (j != (UWORD)-1 && re->tlv[j].str.len)
            re->nick = strdup (ConvFromServ (&re->tlv[j].str));
        else
            re->nick = NULL;
        
        rref = &roster->generic;
        switch (re->type)
        {
            case roster_normal:
                /* TLV_REQAUTH */
                /* TLV_NICK */
                /* TLV_LOCALMAIL */
                /* TLV_LOCALSMS */
                /* TLV_LOCALCOMM */
                /* TLV_LOCALACT */
                /* TLV_LOCALSOUND */
                rref = &roster->normal;
                break;
            case roster_group:
                /* TLV_GROUPITEMS */
                if (re->tag || re->id)
                    rref = &roster->groups;
                break;
            case roster_visible:
                rref = &roster->visible;
                break;
            case roster_invisible:
                rref = &roster->invisible;
                break;
            case roster_ignore:
                rref = &roster->ignore;
                break;
            case roster_lastupd: /* LastUpdateDate */
                /* TLV_LASTUPD */
                break;
            case roster_wierd17: /* wierd */
            case roster_icon: /* buddy icon */
                /* TLV_ICON */
                break;
            case roster_importt: /* ImportTime */
                j = TLVGet (re->tlv, TLV_IMPORT);
                if (j != (UWORD)-1 && re->tlv[j].str.len == 4)
                    roster->import = re->tlv[j].nr;
                else
                    rl_printf ("#Bogus ImportTime item: %d: %s %d %d.\n", re->type, re->name, re->tag, re->id);
                break;
            case roster_icqtic:
                j = TLVGet (re->tlv, TLV_ICQTIC);
                if (j != (UWORD)-1)
                    s_repl (&roster->ICQTIC, re->tlv[j].str.txt);
                break;
            case roster_visibility:
            case roster_presence:
            case roster_noncont:
                /* TLV_UNKNIDLE   */
                /* TLV_PRIVACY    */
                /* TLV_VISIBILITY */
                /* TLV_ALLOWIDLE  */
            case roster_wierd25:
            case roster_wierd29:
                break;
            default:
                rl_printf ("#Unknown type %d: %s %d %d.\n", re->type, re->name, re->tag, re->id);
        }
        
        re->next = *rref;
        *rref = re;
    }
    stmp = PacketReadB4 (pak);
    if (stmp)
    {
        if (!roster->import)
        {
            time_t now = time (NULL);
            UDWORD da = now;
            SnacCliRosterentryadd (serv, "ImportTime", 0, 1, roster_importt, TLV_IMPORT, &da, 4);
        }
        if (!roster->ICQTIC)
            SnacCliRosterentryadd (serv, "ICQTIC", 0, 2, roster_icqtic, TLV_ICQTIC, "3608,0,0,0,60,null", 18);
        event2->callback (event2);
        if (ContactGroupPrefVal (serv->contacts, CO_OBEYSBL))
        {
            rl_printf ("#\n# Server side contact list activated, authorization restrictions apply.\n#\n");
            SnacCliRosterack (serv);
        }
    }
    else
        QueueEnqueue (event2);
}

/*
 * CLI_ROSTERACK - SNAC(13,7)
 */

/* implemented as macro */

/*
 * CLI_ADDBUDDY - SNAC(13,8)
 */

static void SnacCliRosterbulkone (Connection *serv, Contact *cont, Packet *pak, UWORD type)
{
    PacketWriteStrB     (pak, cont->screen);
    PacketWriteB2       (pak, 0);
    PacketWriteB2       (pak, ContactIDGet (cont, type));
    PacketWriteB2       (pak, type);
    PacketWriteBLen     (pak);
    PacketWriteBLenDone (pak);
    QueueEnqueueData (serv, QUEUE_CHANGE_ROSTER, pak->ref, 0x7fffffffL, NULL, cont, NULL, NULL);
}

void SnacCliRosterbulkadd (Connection *serv, ContactGroup *cs)
{
    Packet *pak;
    Contact *cont;
    ContactGroup *cg;
    int i;
    
    for (i = 0; (cont = ContactIndex (cs, i)); i++)
    {
        if (cont->group && cont->group->serv && !ContactGroupPrefVal (cont->group, CO_ISSBL))
            SnacCliRosteradd (serv, cont->group, NULL);
    }
    
    SnacCliAddstart (serv);
    pak = SnacC (serv, 19, 8, 0, 0);

    for (i = 0; (cont = ContactIndex (cs, i)); i++)
    {
        if (!(cg = cont->group))
            continue;
        
        if (i && !(i % 64))
        {
            SnacSend (serv, pak);
            pak = SnacC (serv, 19, 8, 0, 0);
        }

        
        PacketWriteStrB     (pak, cont->screen);
        PacketWriteB2       (pak, ContactGroupID (cg));
        PacketWriteB2       (pak, ContactIDGet (cont, roster_normal));
        PacketWriteB2       (pak, roster_normal);
        PacketWriteBLen     (pak);
        PacketWriteTLVStr   (pak, TLV_NICK, cont->nick);
        if (cont->oldflags & CONT_REQAUTH)
        {
            PacketWriteTLV     (pak, TLV_REQAUTH);
            PacketWriteTLVDone (pak);
        }
        PacketWriteBLenDone (pak);
        QueueEnqueueData (serv, QUEUE_CHANGE_ROSTER, pak->ref, 0x7fffffffL, NULL, cont, NULL, NULL);

        if (ContactPrefVal (cont, CO_HIDEFROM))
        {
            i++;
            SnacCliRosterbulkone (serv, cont, pak, roster_invisible);
        }
        if (ContactPrefVal (cont, CO_INTIMATE))
        {
            i++;
            SnacCliRosterbulkone (serv, cont, pak, roster_visible);
        }
        if (ContactPrefVal (cont, CO_IGNORE))
        {
            i++;
            SnacCliRosterbulkone (serv, cont, pak, roster_ignore);
        }
    }
    SnacSend (serv, pak);
    SnacCliAddend (serv);
}

/*
 * CLI_ADDBUDDY - SNAC(13,8)
 */
void SnacCliRosteradd (Connection *serv, ContactGroup *cg, Contact *cont)
{
    Packet *pak;
    
    if (cont)
    {
        UWORD type = 0;
        
        if (!ContactGroupPrefVal (cg, CO_ISSBL))
            SnacCliRosteradd (serv, cg, NULL);
        
        SnacCliAddstart (serv);

        if (ContactPrefVal (cont, CO_HIDEFROM))
            type = 3;
        else if (ContactPrefVal (cont, CO_INTIMATE))
            type = 2;

        
        pak = SnacC (serv, 19, 8, 0, 0);
        PacketWriteStrB     (pak, cont->screen);
        PacketWriteB2       (pak, ContactGroupID (cg));
        PacketWriteB2       (pak, ContactIDGet (cont, roster_normal));
        PacketWriteB2       (pak, type);
        PacketWriteBLen     (pak);
        PacketWriteTLVStr   (pak, TLV_NICK, cont->nick);
        if (cont->oldflags & CONT_REQAUTH)
        {
            PacketWriteTLV     (pak, TLV_REQAUTH);
            PacketWriteTLVDone (pak);
        }
        PacketWriteBLenDone (pak);
        QueueEnqueueData (serv, QUEUE_CHANGE_ROSTER, pak->ref, 0x7fffffffL, NULL, cont, NULL, NULL);

        if (ContactPrefVal (cont, CO_HIDEFROM))
            SnacCliRosterbulkone (serv, cont, pak, roster_invisible);
        if (ContactPrefVal (cont, CO_INTIMATE))
            SnacCliRosterbulkone (serv, cont, pak, roster_visible);
        if (ContactPrefVal (cont, CO_IGNORE))
            SnacCliRosterbulkone (serv, cont, pak, roster_ignore);

        SnacSend (serv, pak);
        SnacCliAddend (serv);
    }
    else
    {
        pak = SnacC (serv, 19, 8, 0, 0);
        PacketWriteStrB     (pak, cg->name);
        PacketWriteB2       (pak, ContactGroupID (cg));
        PacketWriteB2       (pak, 0);
        PacketWriteB2       (pak, roster_group);
        PacketWriteBLen     (pak);
        PacketWriteBLenDone (pak);
        SnacSend (serv, pak);
        OptSetVal (&cg->copts, CO_ISSBL, 1);
    }
}

void SnacCliRosterentryadd (Connection *serv, const char *name, UWORD tag, UWORD id, UWORD type, UWORD tlv, void *data, UWORD len)
{
    Packet *pak;

    SnacCliAddstart (serv);
    pak = SnacC (serv, 19, 8, 0, 0);
    PacketWriteStrB     (pak, name);
    PacketWriteB2       (pak, tag);
    PacketWriteB2       (pak, id);
    PacketWriteB2       (pak, type);
    PacketWriteBLen     (pak);
    if (tlv)
        PacketWriteTLVData (pak, tlv, data, len);
    PacketWriteBLenDone (pak);
    SnacSend (serv, pak);
    SnacCliAddend (serv);
}

/*
 * CLI_ROSTERUPDATE - SNAC(13,9)
 */
void SnacCliRosterupdate (Connection *serv, ContactGroup *cg, Contact *cont)
{
    Packet *pak;
    int i;

    pak = SnacC (serv, 19, 9, 0, 0);
    if (cont)
    {
        UWORD type = 0;
        
        if (ContactPrefVal (cont, CO_HIDEFROM))
            type = 3;
        else if (ContactPrefVal (cont, CO_INTIMATE))
            type = 2;

        PacketWriteStrB     (pak, cont->screen);
        PacketWriteB2       (pak, ContactGroupID (cg));
        PacketWriteB2       (pak, ContactIDGet (cont, roster_normal));
        PacketWriteB2       (pak, type);
        PacketWriteBLen     (pak);
        PacketWriteTLVStr   (pak, TLV_NICK, cont->nick);
        PacketWriteBLenDone (pak);

        if (ContactPrefVal (cont, CO_HIDEFROM))
            SnacCliRosterbulkone (serv, cont, pak, roster_invisible);
        if (ContactPrefVal (cont, CO_INTIMATE))
            SnacCliRosterbulkone (serv, cont, pak, roster_visible);
        if (ContactPrefVal (cont, CO_IGNORE))
            SnacCliRosterbulkone (serv, cont, pak, roster_ignore);
    }
    else
    {
        PacketWriteStrB     (pak, cg->name);
        PacketWriteB2       (pak, ContactGroupID (cg));
        PacketWriteB2       (pak, 0);
        PacketWriteB2       (pak, roster_group);
        PacketWriteBLen     (pak);
        PacketWriteTLV      (pak, TLV_GROUPITEMS);
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
            PacketWriteB2   (pak, ContactIDGet (cont, roster_normal));
        PacketWriteTLVDone  (pak);
        PacketWriteBLenDone (pak);
    }
    SnacSend (serv, pak);
}

/*
 * CLI_ROSTERUPDATE - SNAC(13,9)
 */
void SnacCliSetvisibility (Connection *serv)
{
    Packet *pak;
    
    pak = SnacC (serv, 19, 9, 0, 0);
    PacketWriteStrB     (pak, "");
    PacketWriteB2       (pak, 0);
    PacketWriteB2       (pak, 0x4242);
    PacketWriteB2       (pak, 4);
    PacketWriteBLen     (pak);
//    PacketWriteB2       (pak, 5);
    PacketWriteTLV      (pak, TLV_PRIVACY);
    PacketWrite1        (pak, 4);
    PacketWriteTLVDone  (pak);
    PacketWriteBLenDone (pak);
    SnacSend (serv, pak);
}

/*
 * CLI_ROSTERUPDATE - SNAC(13,9)
 */
void SnacCliSetlastupdate (Connection *serv)
{
    Packet *pak;
    
    pak = SnacC (serv, 19, 9, 0, 0);
    PacketWriteStrB     (pak, "LastUpdateDate");
    PacketWriteB2       (pak, 0);
    PacketWriteB2       (pak, 0x4141);
    PacketWriteB2       (pak, roster_lastupd);
    PacketWriteBLen     (pak);
    PacketWriteTLV      (pak, TLV_LASTUPD);
    PacketWrite4        (pak, time (NULL));
    PacketWriteTLVDone  (pak);
    PacketWriteBLenDone (pak);
    SnacSend (serv, pak);
}

/*
 * CLI_ROSTERDELETE - SNAC(13,a)
 */
void SnacCliRosterdeletegroup (Connection *serv, ContactGroup *cg)
{
    Packet *pak;

    if (!cg->id)
        return;

    SnacCliAddstart (serv);
    pak = SnacC (serv, 19, 10, 0, 0);
    PacketWriteStrB     (pak, cg->name);
    PacketWriteB2       (pak, ContactGroupID (cg));
    PacketWriteB2       (pak, 0);
    PacketWriteB2       (pak, roster_group);
    PacketWriteBLen     (pak);
    PacketWriteBLenDone (pak);
    SnacSend (serv, pak);
    SnacCliAddend (serv);
}

/*
 * CLI_ROSTERDELETE - SNAC(13,a)
 */
void SnacCliRosterdeletecontact (Connection *serv, Contact *cont)
{
    Packet *pak;
    ContactIDs *ids;
    
    if (!cont->ids)
        return;
    
    SnacCliAddstart (serv);
    pak = SnacC (serv, 19, 10, 0, 0);

    if ((ids = ContactIDHas (cont, roster_normal)) && ids->issbl)
    {
        PacketWriteStrB     (pak, cont->screen);
        PacketWriteB2       (pak, ids->tag);
        PacketWriteB2       (pak, ids->id);
        PacketWriteB2       (pak, roster_normal);
        PacketWriteBLen     (pak);
        PacketWriteBLenDone (pak);
    }
    if ((ids = ContactIDHas (cont, roster_invisible)) && ids->issbl)
        SnacCliRosterbulkone (serv, cont, pak, roster_invisible);
    if ((ids = ContactIDHas (cont, roster_visible)) && ids->issbl)
        SnacCliRosterbulkone (serv, cont, pak, roster_visible);
    if ((ids = ContactIDHas (cont, roster_ignore)) && ids->issbl)
        SnacCliRosterbulkone (serv, cont, pak, roster_ignore);

    SnacSend (serv, pak);
    SnacCliAddend (serv);
}

/*
 * CLI_ROSTERDELETE - SNAC(13,a)
 */
void SnacCliRosterentrydelete (Connection *serv, RosterEntry *re)
{
    Packet *pak;
    int i;

    SnacCliAddstart (serv);
    pak = SnacC (serv, 19, 10, 0, 0);
    PacketWriteStrB     (pak, re->name);
    PacketWriteB2       (pak, re->tag);
    PacketWriteB2       (pak, re->id);
    PacketWriteB2       (pak, re->type);
    PacketWriteBLen     (pak);
    for (i = 0; i < 16 || re->tlv[i].tag; i++)
        if (re->tlv[i].tag)
            PacketWriteTLVData (pak, re->tlv[i].tag, re->tlv[i].str.txt, re->tlv[i].str.len);
    PacketWriteBLenDone (pak);
    SnacSend (serv, pak);
    SnacCliAddend (serv);
}

/*
 * SRV_UPDATEACK - SNAC(13,e)
 */
JUMP_SNAC_F(SnacSrvUpdateack)
{
    Connection *serv = event->conn;
    Contact *cont = NULL;
    Event *event2;
    UWORD err;
    
    if (PacketReadLeft (event->pak))
    {
        err = PacketReadB2 (event->pak);
        SnacSrvUpdateack (event);

        event2 = QueueDequeue (serv, QUEUE_CHANGE_ROSTER, event->pak->ref);
        cont = event2 ? event2->cont : NULL;
        
        switch (err)
        {
            case 0xe:
                if (cont)
                {
                    cont->oldflags |= CONT_REQAUTH;
                    OptSetVal (&cont->copts, CO_ISSBL, 0);
                    SnacCliRosteradd (serv, serv->contacts, cont);
                }
                break;
            case 10:
                if (cont)
                {
                    cont->oldflags |= CONT_REQAUTH;
                    OptSetVal (&cont->copts, CO_ISSBL, 0);
                    rl_printf (i18n (2565, "Contact upload for %s%s%s failed, authorization required.\n"),
                              COLCONTACT, s_quote (cont->nick), COLNONE);
                }
                else
                    rl_printf (i18n (2537, "Contact upload failed, authorization required.\n"));
                break;
            case 3:
                if (cont)
                {
                    rl_printf (i18n (2566, "Contact upload for %s%s%s failed, already on server.\n"),
                               COLCONTACT, s_quote (cont->nick), COLNONE);
                }
                else
                    rl_printf (i18n (2538, "Contact upload failed, already on server.\n"));
                break;
            case 0:
                if (cont)
                {
                    OptSetVal (&cont->copts, CO_ISSBL, 1);
                    rl_printf (i18n (2567, "Contact upload for %s%s%s succeeded.\n"),
                               COLCONTACT, s_quote (cont->nick), COLNONE);
                }
                else
                    rl_printf (i18n (2539, "Contact upload succeeded.\n"));
                break;
            default:
                rl_printf (i18n (2325, "Warning: server based contact list change failed with error code %d.\n"), err);
        }
        if (event2)
            EventD (event2);
    }
}

/*
 * SRV_ROSTEROK - SNAC(13,f)
 */
JUMP_SNAC_F(SnacSrvRosterok)
{
    /* ignore */
    Connection *serv;
    Packet *pak;
    Event *event2;
    Roster *roster;
    UWORD count;
    time_t stmp;

    if (!event)
        return;

    serv = event->conn;
    if (!serv)
        return;

    pak = event->pak;
    if (!pak)
        return;
    
    event2 = QueueDequeue2 (serv, QUEUE_REQUEST_ROSTER, 0, 0);
    if (!event2 || !event2->callback)
    {
        DebugH (DEB_PROTOCOL, "Unrequested roster packet received.\n");
        return;
    }

    roster = event2->data;
    if (!roster)
    {
        roster = OscarRosterC ();
        event2->data = roster;
    }
    
    PacketRead1 (pak);
    count = PacketReadB2 (pak);          /* COUNT */
    stmp = PacketReadB4 (pak);

    if (!roster->import)
    {
        time_t now = time (NULL);
        UDWORD da = now;
        SnacCliRosterentryadd (serv, "ImportTime", 0, 1, roster_importt, TLV_IMPORT, &da, 4);
    }
    if (!roster->ICQTIC)
        SnacCliRosterentryadd (serv, "ICQTIC", 0, 2, roster_icqtic, TLV_ICQTIC, "3608,0,0,0,60,null", 18);
    event2->callback (event2);
    if (ContactGroupPrefVal (serv->contacts, CO_OBEYSBL))
    {
        rl_printf ("#\n# Server side contact list activated, authorization restrictions apply.\n#\n");
        SnacCliRosterack (serv);
    }
}

/*
 * CLI_ADDSTART - SNAC(13,11)
 */
void SnacCliAddstart (Connection *serv)
{
    Packet *pak;
    
    pak = SnacC (serv, 19, 17, 0, 0);
    SnacSend (serv, pak);
}

/*
 * CLI_ADDEND - SNAC(13,12)
 */
void SnacCliAddend (Connection *serv)
{
    Packet *pak;

    pak = SnacC (serv, 19, 18, 0, 0);
    SnacSend (serv, pak);
}

/*
 * CLI_GRANTAUTH - SNAC(13,14)
 */
void SnacCliGrantauth (Connection *serv, Contact *cont)
{
    Packet *pak;
    
    if (!cont)
        return;
    pak = SnacC (serv, 19, 20, 0, 0);
    PacketWriteCont (pak, cont);
    PacketWrite4    (pak, 0);
    SnacSend (serv, pak);
}

/*
 * CLI_REQAUTH - SNAC(13,18)
 */
void SnacCliReqauth (Connection *serv, Contact *cont, const char *msg)
{
    Packet *pak;
    
    if (!cont)
        return;
    pak = SnacC (serv, 19, 24, 0, 0);
    PacketWriteCont (pak, cont);
    PacketWriteStrB (pak, c_out_to (msg, cont));
    PacketWrite2    (pak, 0);
    SnacSend (serv, pak);
}

/*
 * SRV_AUTHREQ - SNAC(13,19)
 */
JUMP_SNAC_F(SnacSrvAuthreq)
{
    Connection *serv = event->conn;
    Opt *opt;
    Packet *pak;
    Contact *cont;
    strc_t ctext;
    char *text;

    pak   = event->pak;
    cont  = PacketReadCont (pak, serv);
    ctext = PacketReadB2Str (pak, NULL);
    
    if (!cont)
        return;
    
    text = strdup (c_in_to_split (ctext, cont));
    
    opt = OptSetVals (NULL, CO_ORIGIN, CV_ORIGIN_v8,
                                 CO_MSGTYPE, MSG_AUTH_REQ, CO_MSGTEXT, text, 0);
    IMSrvMsg (cont, NOW, opt);
    free (text);
}

/*
 * CLI_AUTHORIZE - SNAC(13,1a)
 */
void SnacCliAuthorize (Connection *serv, Contact *cont, BOOL accept, const char *msg)
{
    Packet *pak;
    
    if (!cont)
        return;
    pak = SnacC (serv, 19, 26, 0, 0);
    PacketWriteCont (pak, cont);
    PacketWrite1    (pak, accept ? 1 : 0);
    PacketWriteStrB (pak, accept ? "" : c_out_to (msg, cont));
    SnacSend (serv, pak);
}

/*
 * SRV_AUTHREPLY - SNAC(13,1b)
 */
JUMP_SNAC_F(SnacSrvAuthreply)
{
    Connection *serv = event->conn;
    Opt *opt;
    Packet *pak;
    Contact *cont;
    strc_t ctext;
    char *text;
    UBYTE acc;

    pak = event->pak;
    cont  = PacketReadCont (pak, serv);
    acc   = PacketRead1    (pak);
    ctext = PacketReadB2Str (pak, NULL);
    
    if (!cont)
        return;
    
    text = strdup (c_in_to_split (ctext, cont));

    opt = OptSetVals (NULL, CO_ORIGIN, CV_ORIGIN_v8,
              CO_MSGTYPE, acc ? MSG_AUTH_GRANT : MSG_AUTH_DENY, CO_MSGTEXT, text, 0);
    IMSrvMsg (cont, NOW, opt);
    free (text);
}

/*
 * SRV_ADDEDYOU - SNAC(13,1c)
 */
JUMP_SNAC_F(SnacSrvAddedyou)
{
    Connection *serv = event->conn;
    Opt *opt;
    Contact *cont;
    Packet *pak;

    pak = event->pak;
    cont = PacketReadCont (pak, serv);

    opt = OptSetVals (NULL, CO_ORIGIN, CV_ORIGIN_v8, CO_MSGTYPE, MSG_AUTH_ADDED, 0);
    IMSrvMsg (cont, NOW, opt);
}
