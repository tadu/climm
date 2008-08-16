/*
 * Handles incoming and creates outgoing SNAC packets
 * for the family 19 (roster) commands.
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
 * As a special exemption, you may link your binary against OpenSSL,
 * provided your operating system distribution doesn't ship libgnutls.
 *
 * $Id$
 */

#include "climm.h"
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
#include "im_response.h"

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
    s_free (roster->ICQTIC);
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
    Server *serv = event->conn->serv;

    if (serv->flags & CONN_WIZARD)
    {
        IMRoster (serv, IMROSTER_IMPORT);
        if (serv->flags & CONN_INITWP)
        {
            Contact *cont = ContactScreen (serv, serv->screen);
            CONTACT_GENERAL (cont);
            CONTACT_MORE (cont);
            SnacCliMetasetabout (serv, "climm");
            SnacCliMetasetgeneral (serv, cont);
            SnacCliMetasetmore (serv, cont);
        }
    }
    else
        IMRoster (serv, IMROSTER_DIFF);
}

/*
 * CLI_REQROSTER - SNAC(13,4)
 */

/* implemented as macro */

/*
 * CLI_CHECKROSTER - SNAC(13,5)
 */
UDWORD SnacCliCheckroster (Server *serv)
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
    Server *serv;
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

    serv = event->conn->serv;
    if (!serv)
        return;

    pak = event->pak;
    
    if (!pak)
        return;
    
    event2 = QueueDequeue2 (serv->conn, QUEUE_REQUEST_ROSTER, 0, 0);
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
                if (TLVGet (re->tlv, TLV_REQAUTH) != (UWORD)-1)
                    re->reqauth = 1;
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
                serv->oscar_privacy_tag = re->id;
                j = TLVGet (re->tlv, TLV_PRIVACY);
                if (j != (UWORD)-1)
                {
                    serv->oscar_privacy_value = *re->tlv[j].str.txt;
                    if (*re->tlv[j].str.txt != 3 && *re->tlv[j].str.txt != 4)
                    {
                        rl_printf ("# privacy mode: (%d)\n", *re->tlv[j].str.txt);
                        if (*re->tlv[j].str.txt == 1)
                            rl_printf ("#     always visible to all users\n");
                        else if (*re->tlv[j].str.txt == 2)
                            rl_printf ("#     never visible to anyone\n");
                        else if (*re->tlv[j].str.txt == 3)
                            rl_printf ("#     only visible to always visible contacts\n");
                        else if (*re->tlv[j].str.txt == 4)
                            rl_printf ("#     always visible except to invisible contacts\n");
                        else if (*re->tlv[j].str.txt == 5)
                            rl_printf ("#     always visible to all contacts\n");
                        else
                            rl_printf ("#     unknown/invalid privacy mode\n");
                        rl_printf ("# change with 'contact security <mode>' (to 3 or 4).\n");
                    }
                }
                break;
            
            case roster_presence:
            case roster_noncont:
                /* TLV_UNKNIDLE   */
                /* TLV_PRIVACY    */
                /* TLV_VISIBILITY */
                /* TLV_ALLOWIDLE  */
            case roster_wierd25:
            case roster_wierd29:
            case roster_wierd37:
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
#if 0
        if (!roster->import)
        {
            time_t now = time (NULL);
            UDWORD da = now;
            SnacCliRosterentryadd (serv, "ImportTime", 0, 1, roster_importt, TLV_IMPORT, &da, 4);
        }
        if (!roster->ICQTIC)
            SnacCliRosterentryadd (serv, "ICQTIC", 0, 2, roster_icqtic, TLV_ICQTIC, "3608,0,0,0,60,null", 18);
        if (!serv->oscar_privacy_tag)
        {
            serv->oscar_privacy_tag = rand () % 0x8000;
            SnacCliRosterentryadd (serv, "", 0, serv->oscar_privacy_tag, roster_visibility, TLV_PRIVACY, "\x03", 1);
            serv->oscar_privacy_value = 3;
        }
#endif
        if (~serv->conn->connect & CONNECT_OK)
            CliFinishLogin (serv);

        if (ContactGroupPrefVal (serv->contacts, CO_OBEYSBL))
            SnacCliRosterack (serv);
        event2->callback (event2);
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

static void SnacCliRosterbulkdata (Packet *pak, const char *name, UWORD tag, UWORD id, UWORD type)
{
    PacketWriteStrB     (pak, name);
    PacketWriteB2       (pak, tag);
    PacketWriteB2       (pak, id);
    PacketWriteB2       (pak, type);
    PacketWriteBLen     (pak);
}

static void SnacCliRosterbulkone (Server *serv, ContactGroup *cg, Contact *cont, Packet *pak, UWORD type, UWORD subtype)
{
    UWORD tag = 0;
    ContactIDs *ids;
    
    if (type == roster_normal && (ids = ContactIDHas (cont, roster_normal)) && ids->issbl)
        tag = ids->tag;
    else if (type == roster_group || type == roster_normal)
        tag = ContactGroupID (cg);
    
    SnacCliRosterbulkdata (pak, type == roster_group ? cg->name : cont->screen, tag,
                                type == roster_group ? 0 : ContactIDGet (cont, type), type);
    if (type == roster_normal && subtype != 10)
    {
        PacketWriteTLVStr   (pak, TLV_NICK, cont->nick);
        if (cont->oldflags & CONT_REQAUTH)
        {
            PacketWriteTLV     (pak, TLV_REQAUTH);
            PacketWriteTLVDone (pak);
        }
    }
    else if (type == roster_group && subtype == 9)
    {
        Contact *cont;
        int i;
        PacketWriteTLV      (pak, TLV_GROUPITEMS);
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
            if (cont->group == cg)
                PacketWriteB2   (pak, ContactIDGet (cont, roster_normal));
        PacketWriteTLVDone  (pak);
    }
    PacketWriteBLenDone (pak);
    QueueEnqueueData (serv->conn, QUEUE_CHANGE_ROSTER, pak->ref, 0x7fffffffL, NULL, cont, NULL, NULL);
}

void SnacCliRosterbulkadd (Server *serv, ContactGroup *cs)
{
    Packet *pak;
    Contact *cont;
    ContactGroup *cg;
    int i;
    
    for (i = 0; (cont = ContactIndex (cs, i)); i++)
        if (cont->group && cont->group->serv && !ContactGroupPrefVal (cont->group, CO_ISSBL))
            SnacCliRosteraddgroup (serv, cont->group, 3);
    
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
        
        SnacCliRosterbulkone (serv, cg, cont, pak, roster_normal, 8);

        if (ContactPrefVal (cont, CO_HIDEFROM))
        {
            i++;
            SnacCliRosterbulkone (serv, NULL, cont, pak, roster_invisible, 8);
        }
        if (ContactPrefVal (cont, CO_INTIMATE))
        {
            i++;
            SnacCliRosterbulkone (serv, NULL, cont, pak, roster_visible, 8);
        }
        if (ContactPrefVal (cont, CO_IGNORE))
        {
            i++;
            SnacCliRosterbulkone (serv, NULL, cont, pak, roster_ignore, 8);
        }
    }
    SnacSend (serv, pak);
    SnacCliAddend (serv);
}

/*
 * CLI_ADDBUDDY - SNAC(13,8)
 */
void SnacCliRosterentryadd (Server *serv, const char *name, UWORD tag, UWORD id, UWORD type, UWORD tlv, void *data, UWORD len)
{
    Packet *pak;

    SnacCliAddstart (serv);
    pak = SnacC (serv, 19, 8, 0, 0);
    SnacCliRosterbulkdata (pak, name, tag, id, type);
    if (tlv)
        PacketWriteTLVData (pak, tlv, data, len);
    PacketWriteBLenDone (pak);
    SnacSend (serv, pak);
    SnacCliAddend (serv);
}

void SnacCliRosteraddgroup (Server *serv, ContactGroup *cg, int mode)
{
    Packet *pak;
    assert (cg);

    if (mode & 1)
        SnacCliAddstart (serv);
    pak = SnacC (serv, 19, 8, 0, 0);
    SnacCliRosterbulkone (serv, cg, NULL, pak, roster_group, 8);
    SnacSend (serv, pak);
    OptSetVal (&cg->copts, CO_ISSBL, 1);
    if (mode & 2)
        SnacCliAddend (serv);
}

void SnacCliRosteraddcontact (Server *serv, Contact *cont, int mode)
{
    Packet *pak;
    assert (cont);

    if (mode & 1)
        SnacCliAddstart (serv);
    if (!ContactGroupPrefVal (cont->group, CO_ISSBL))
        SnacCliRosteraddgroup (serv, cont->group, 0);
    pak = SnacC (serv, 19, 8, 0, 0);
    SnacCliRosterbulkone (serv, cont->group, cont, pak, roster_normal, 8);
    if (ContactPrefVal (cont, CO_HIDEFROM))
        SnacCliRosterbulkone (serv, NULL, cont, pak, roster_invisible, 8);
    if (ContactPrefVal (cont, CO_INTIMATE))
        SnacCliRosterbulkone (serv, NULL, cont, pak, roster_visible, 8);
    if (ContactPrefVal (cont, CO_IGNORE))
        SnacCliRosterbulkone (serv, NULL, cont, pak, roster_ignore, 8);
    SnacSend (serv, pak);
    if (mode & 2)
        SnacCliAddend (serv);
}

/*
 * CLI_ROSTERUPDATE - SNAC(13,9)
 */
void SnacCliRosterupdategroup (Server *serv, ContactGroup *cg, int mode)
{
    Packet *pak;
    assert (cg);

    if (mode & 1)
        SnacCliAddstart (serv);
    pak = SnacC (serv, 19, 9, 0, 0);
    SnacCliRosterbulkone (serv, cg, NULL, pak, roster_group, 9);
    SnacSend (serv, pak);
    if (mode & 2)
        SnacCliAddend (serv);
}

void SnacCliRosterupdatecontact (Server *serv, Contact *cont, int mode)
{
    Packet *pak;
    assert (cont);

    if (mode & 1)
        SnacCliAddstart (serv);
    if (!ContactGroupPrefVal (cont->group, CO_ISSBL))
        SnacCliRosteraddgroup (serv, cont->group, 0);
    pak = SnacC (serv, 19, 9, 0, 0);
    SnacCliRosterbulkone (serv, cont->group, cont, pak, roster_normal, 9);
    if (ContactPrefVal (cont, CO_HIDEFROM))
        SnacCliRosterbulkone (serv, NULL, cont, pak, roster_invisible, 9);
    if (ContactPrefVal (cont, CO_INTIMATE))
        SnacCliRosterbulkone (serv, NULL, cont, pak, roster_visible, 9);
    if (ContactPrefVal (cont, CO_IGNORE))
        SnacCliRosterbulkone (serv, NULL, cont, pak, roster_ignore, 9);
    SnacSend (serv, pak);
    if (mode & 2)
        SnacCliAddend (serv);
}

void SnacCliRostermovecontact (Server *serv, Contact *cont, ContactGroup *cg, int mode)
{
    Packet *pakd, *paka;
    ContactIDs *ids;
    char is, want;
    is = ContactPrefVal (cont, CO_ISSBL);
    want = ContactPrefVal (cont, CO_WANTSBL);
    
    if (!is && !want && mode == 3)
        return;
    
    if (mode & 1)
        SnacCliAddstart (serv);

    if (!ContactGroupPrefVal (cg, CO_ISSBL))
        SnacCliRosteraddgroup (serv, cg, 0);

    pakd = SnacC (serv, 19, 10, 0, 0);
    paka = SnacC (serv, 19, 8, 0, 0);
    
    if (cont->ids && (ids = ContactIDHas (cont, roster_normal)) && ids->issbl)
        SnacCliRosterbulkone (serv, cont->group, cont, pakd, roster_normal, 10);
    cont->group = cg;
    if (want)
        SnacCliRosterbulkone (serv, cont->group, cont, paka, roster_normal, 8);

    SnacSend (serv, pakd);
    SnacSend (serv, paka);

    if (mode & 2)
        SnacCliAddend (serv);
}

void SnacCliRosterbulkmove (Server *serv, ContactGroup *cg, int mode)
{
    Packet *pakd, *paka;
    Contact *cont;
    ContactIDs *ids;
    int i;

    if (!ContactIndex (cg, 0) && mode == 3)
        return;

    if (mode & 1)
        SnacCliAddstart (serv);

    for (i = 0; (cont = ContactIndex (cg, i)); i++)
        if (!ContactGroupPrefVal (cont->group, CO_ISSBL))
            SnacCliRosteraddgroup (serv, cont->group, 0);

    pakd = SnacC (serv, 19, 10, 0, 0);
    paka = SnacC (serv, 19, 8, 0, 0);
    
    for (i = 0; (cont = ContactIndex (cg, i)); i++)
    {
        if (cont->ids && (ids = ContactIDHas (cont, roster_normal)) && ids->issbl)
            SnacCliRosterbulkone (serv, cont->group, cont, pakd, roster_normal, 10);
        if (ContactPrefVal (cont, CO_WANTSBL))
            SnacCliRosterbulkone (serv, cont->group, cont, paka, roster_normal, 8);
    }
    SnacSend (serv, pakd);
    SnacSend (serv, paka);

    if (mode & 2)
        SnacCliAddend (serv);
}

/*
 * SRV_ROSTERADD
 */
JUMP_SNAC_F(SnacSrvRosteradd)
{
}

/*
 * SRV_ROSTERUPDATE
 */
JUMP_SNAC_F(SnacSrvRosterupdate)
{
    Server *serv = event->conn->serv;
    Packet *pak = event->pak;
    Contact *cont;
    UWORD tag, id, type, TLVlen;
    TLV *tlv;
    strc_t cname;
    UWORD j;

    cname  = PacketReadB2Str (pak, NULL);
    tag    = PacketReadB2 (pak);     /* TAG  */
    id     = PacketReadB2 (pak);     /* ID   */
    type   = PacketReadB2 (pak);     /* TYPE */
    TLVlen = PacketReadB2 (pak);     /* TLV length */
    tlv    = TLVRead (pak, TLVlen, -1);
    
    if (type == roster_normal)
    {
        cont = ContactScreen (serv, cname->txt);
        j = TLVGet (tlv, TLV_NICK);
        assert (j < 200 || j == (UWORD)-1);
        if (j != (UWORD)-1 && tlv[j].str.len)
            ContactAddAlias (cont, ConvFromServ (&tlv[j].str));
    }
    
    TLVD (tlv);

    if (pak->rpos < pak->len)
        SnacSrvUnknown (event);
}

/*
 * SRV_ROSTERDELETE
 */
JUMP_SNAC_F(SnacSrvRosterdelete)
{
}

static JUMP_SNAC_F(cb_LoginVisibilitySet)
{
    UWORD err;

    if (!event->pak)
    {
        EventD (event);
        return;
    }
    if (event->pak->ref != 19) /* family */
        return;
    if (event->pak->cmd != 14) /* command */
        return;

    err = PacketReadB2 (event->pak);
    PacketD (event->pak);
    event->pak = NULL;
    
    if (err)
        rl_printf (i18n (2325, "Warning: server based contact list change failed with error code %d.\n"), err);
}

/*
 * CLI_ROSTERADD/UPDATE - SNAC(13,9)
 */
void SnacCliSetvisibility (Server *serv, char value, char islogin)
{
    Packet *pak;
    
    if (serv->oscar_privacy_tag)
    {
        pak = SnacC (serv, 19, 9, 0, 0);
        SnacCliRosterbulkdata (pak, "", 0, serv->oscar_privacy_tag, roster_visibility);
        PacketWriteTLV      (pak, TLV_PRIVACY);
        PacketWrite1        (pak, value);
        PacketWriteTLVDone  (pak);
        PacketWriteBLenDone (pak);
        SnacSendR (serv, pak, cb_LoginVisibilitySet, NULL);
        serv->oscar_privacy_value = value;
    }
    else
    {
        serv->oscar_privacy_tag = rand () % 0x8000;
        SnacCliRosterentryadd (serv, "", 0, serv->oscar_privacy_tag, roster_visibility, TLV_PRIVACY, &value, 1);
        serv->oscar_privacy_value = value;
    }
}

/*
 * CLI_ROSTERUPDATE - SNAC(13,9)
 */
void SnacCliSetlastupdate (Server *serv)
{
    Packet *pak;
    
    pak = SnacC (serv, 19, 9, 0, 0);
    SnacCliRosterbulkdata (pak, "LastUpdateDate", 0, 0x4141, roster_lastupd);
    PacketWriteTLV      (pak, TLV_LASTUPD);
    PacketWrite4        (pak, time (NULL));
    PacketWriteTLVDone  (pak);
    PacketWriteBLenDone (pak);
    SnacSend (serv, pak);
}

/*
 * CLI_ROSTERDELETE - SNAC(13,a)
 */
void SnacCliRosterdeletegroup (Server *serv, ContactGroup *cg, int mode)
{
    Packet *pak;

    if (mode & 1)
        SnacCliAddstart (serv);
    if (cg->id)
    {
        pak = SnacC (serv, 19, 10, 0, 0);
        SnacCliRosterbulkone (serv, cg, NULL, pak, roster_group, 10);
        SnacSend (serv, pak);
    }
    if (mode & 2)
        SnacCliAddend (serv);
}

/*
 * CLI_ROSTERDELETE - SNAC(13,a)
 */
void SnacCliRosterdeletecontact (Server *serv, Contact *cont, int mode)
{
    Packet *pak;
    ContactIDs *ids;
    
    if (mode & 1)
        SnacCliAddstart (serv);
    if (cont->ids)
    {
        pak = SnacC (serv, 19, 10, 0, 0);
        if ((ids = ContactIDHas (cont, roster_normal)) && ids->issbl)
            SnacCliRosterbulkone (serv, cont->group, cont, pak, roster_normal, 10);
        if ((ids = ContactIDHas (cont, roster_invisible)) && ids->issbl)
            SnacCliRosterbulkone (serv, NULL, cont, pak, roster_invisible, 10);
        if ((ids = ContactIDHas (cont, roster_visible)) && ids->issbl)
            SnacCliRosterbulkone (serv, NULL, cont, pak, roster_visible, 10);
        if ((ids = ContactIDHas (cont, roster_ignore)) && ids->issbl)
            SnacCliRosterbulkone (serv, NULL, cont, pak, roster_ignore, 10);
        SnacSend (serv, pak);
    }
    if (mode & 2)
        SnacCliAddend (serv);
}

/*
 * CLI_ROSTERDELETE - SNAC(13,a)
 */
void SnacCliRosterentrydelete (Server *serv, RosterEntry *re)
{
    Packet *pak;
    int i;

    SnacCliAddstart (serv);
    pak = SnacC (serv, 19, 10, 0, 0);
    SnacCliRosterbulkdata (pak, re->name, re->tag, re->id, re->type);
    for (i = 0; i < 16 || re->tlv[i].tag; i++)
        if (re->tlv[i].tag)
            PacketWriteTLVData (pak, re->tlv[i].tag, re->tlv[i].str.txt, re->tlv[i].str.len);
    PacketWriteBLenDone (pak);
    SnacSend (serv, pak);
    SnacCliAddend (serv);
}

void SnacCliRosterdelete (Server *serv, const char *name, UWORD tag, UWORD id, roster_t type)
{
    Packet *pak;

    SnacCliAddstart (serv);
    pak = SnacC (serv, 19, 10, 0, 0);
    SnacCliRosterbulkdata (pak, name, tag, id, type);
    PacketWriteBLenDone (pak);
    SnacSend (serv, pak);
    SnacCliAddend (serv);
}

/*
 * SRV_UPDATEACK - SNAC(13,e)
 */
JUMP_SNAC_F(SnacSrvUpdateack)
{
    Server *serv = event->conn->serv;
    Contact *cont = NULL;
    Event *event2;
    UWORD err;
    
    if (!PacketReadLeft (event->pak))
        return;

    err = PacketReadB2 (event->pak);
    SnacSrvUpdateack (event);

    event2 = QueueDequeue (serv->conn, QUEUE_CHANGE_ROSTER, event->pak->ref);
    cont = event2 ? event2->cont : NULL;
    
    switch (err)
    {
        case 0xe:
            if (cont)
            {
                cont->oldflags |= CONT_REQAUTH;
                OptSetVal (&cont->copts, CO_ISSBL, 0);
                SnacCliRosteraddcontact (serv, cont, 3);
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

/*
 * SRV_ROSTEROK - SNAC(13,f)
 */
JUMP_SNAC_F(SnacSrvRosterok)
{
    /* ignore */
    Server *serv;
    Packet *pak;
    Event *event2;
    Roster *roster;
    UWORD count;
    time_t stmp;

    if (!event)
        return;

    serv = event->conn->serv;
    if (!serv)
        return;

    pak = event->pak;
    if (!pak)
        return;
    
    event2 = QueueDequeue2 (serv->conn, QUEUE_REQUEST_ROSTER, 0, 0);
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

#if 0
    if (!roster->import)
    {
        time_t now = time (NULL);
        UDWORD da = now;
        SnacCliRosterentryadd (serv, "ImportTime", 0, 1, roster_importt, TLV_IMPORT, &da, 4);
    }
    if (!roster->ICQTIC)
        SnacCliRosterentryadd (serv, "ICQTIC", 0, 2, roster_icqtic, TLV_ICQTIC, "3608,0,0,0,60,null", 18);
    if (!serv->oscar_privacy_tag)
    {
        serv->oscar_privacy_tag = rand () % 0x8000;
        SnacCliRosterentryadd (serv, "", 0, serv->oscar_privacy_tag, roster_visibility, TLV_PRIVACY, "\x03", 1);
        serv->oscar_privacy_value = 3;
    }
#endif
    if (~serv->conn->connect & CONNECT_OK)
        CliFinishLogin (serv);

    if (ContactGroupPrefVal (serv->contacts, CO_OBEYSBL))
    {
        rl_printf ("#\n# Server side contact list activated, authorization restrictions apply.\n#\n");
        SnacCliRosterack (serv);
    }
    event2->callback (event2);
}

/*
 * SRV_ADDSTART
 */
JUMP_SNAC_F(SnacSrvAddstart)
{
    if (event->pak->rpos < event->pak->len)
        SnacSrvUnknown (event);
}

/*
 * SRV_ADDEND
 */
JUMP_SNAC_F(SnacSrvAddend)
{
    if (event->pak->rpos < event->pak->len)
        SnacSrvUnknown (event);
}


/*
 * CLI_ADDSTART - SNAC(13,11)
 */
void SnacCliAddstart (Server *serv)
{
    Packet *pak;
    
    pak = SnacC (serv, 19, 17, 0, 0);
    SnacSend (serv, pak);
}

/*
 * CLI_ADDEND - SNAC(13,12)
 */
void SnacCliAddend (Server *serv)
{
    Packet *pak;

    pak = SnacC (serv, 19, 18, 0, 0);
    SnacSend (serv, pak);
}

/*
 * CLI_GRANTAUTH - SNAC(13,14)
 */
void SnacCliGrantauth (Server *serv, Contact *cont)
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
void SnacCliReqauth (Server *serv, Contact *cont, const char *msg)
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
    Server *serv = event->conn->serv;
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
    IMSrvMsg (cont, NOW, CV_ORIGIN_v8, MSG_AUTH_REQ, text);
    free (text);
}

/*
 * CLI_AUTHORIZE - SNAC(13,1a)
 */
void SnacCliAuthorize (Server *serv, Contact *cont, BOOL accept, const char *msg)
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
    Server *serv = event->conn->serv;
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
    IMSrvMsg (cont, NOW, CV_ORIGIN_v8, acc ? MSG_AUTH_GRANT : MSG_AUTH_DENY, text);
    free (text);
}

/*
 * SRV_ADDEDYOU - SNAC(13,1c)
 */
JUMP_SNAC_F(SnacSrvAddedyou)
{
    Contact *cont;

    cont = PacketReadCont (event->pak, event->conn->serv);
    IMSrvMsg (cont, NOW, CV_ORIGIN_v8, MSG_AUTH_ADDED, "");
}
