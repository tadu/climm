/*
 * Handles incoming and creates outgoing SNAC packets
 * for the family 19 (roster) commands.
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
#include "icq_response.h"

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
    SnacCliAddcontact (serv, 0);
    SnacCliReqofflinemsgs (serv);
    if (serv->flags & CONN_WIZARD)
    {
        SnacCliReqroster  (serv);
        QueueEnqueueData (serv, QUEUE_REQUEST_ROSTER, IMROSTER_IMPORT, 0x7fffffffL,
                          NULL, NULL, NULL, NULL);
    }
    else
    {
        SnacCliCheckroster  (serv);
        QueueEnqueueData (serv, QUEUE_REQUEST_ROSTER, IMROSTER_DIFF, 0x7fffffffL,
                          NULL, NULL, NULL, NULL);
    }
}

/*
 * CLI_REQROSTER - SNAC(13,4)
 */

/* implemented as macro */

/*
 * CLI_CHECKROSTER - SNAC(13,5)
 */
void SnacCliCheckroster (Connection *serv)
{
    ContactGroup *cg;
    Packet *pak;
    int i;
    
    cg = serv->contacts;
    pak = SnacC (serv, 19, 5, 0, 0);
    for (i = 0; (/*cont =*/ ContactIndex (cg, i)); )
        i++;

    PacketWriteB4 (pak, 0);  /* last modification of server side contact list */
    PacketWriteB2 (pak, 0);  /* # of entries */
    SnacSend (serv, pak);
}

/*
 * SRV_REPLYROSTER - SNAC(13,6)
 */
JUMP_SNAC_F(SnacSrvReplyroster)
{
    Connection *serv = event->conn;
    Packet *pak, *p;
    TLV *tlv;
    Event *event2;
    ContactGroup *cg = NULL;
    Contact *cont;
    int i, k, l;
    int cnt_sbl_add, cnt_sbl_chn, cnt_sbl_del;
    int cnt_loc_add, cnt_loc_chn, cnt_loc_del;
    strc_t cname;
    char *name, *nick;
    UWORD count, type, tag, id, TLVlen, j, data;
    time_t stmp;

    pak = event->pak;
    
    event2 = QueueDequeue2 (serv, QUEUE_REQUEST_ROSTER, 0, 0);
    data = event2 ? event2->seq : 1;

    PacketRead1 (pak);

    count = PacketReadB2 (pak);          /* COUNT */
    cnt_sbl_add = cnt_sbl_chn = cnt_sbl_del = 0;
    cnt_loc_add = cnt_loc_chn = cnt_loc_del = 0;
    for (i = k = l = 0; i < count; i++)
    {
        cname  = PacketReadB2Str (pak, NULL);   /* GROUP NAME */
        tag    = PacketReadB2 (pak);     /* TAG  */
        id     = PacketReadB2 (pak);     /* ID   */
        type   = PacketReadB2 (pak);     /* TYPE */
        TLVlen = PacketReadB2 (pak);     /* TLV length */
        tlv    = TLVRead (pak, TLVlen);
        
        name = strdup (ConvFromServ (cname));

        switch (type)
        {
            case 1:
                if (!tag && !id) /* group 0, ID 0: list IDs of all groups */
                {
                    j = TLVGet (tlv, 200);
                    if (j == (UWORD)-1)
                        break;
                    p = PacketCreate (&tlv[j].str);
                    while ((id = PacketReadB2 (p)))
                        if (!ContactGroupFind (serv, id, NULL))
                            if (IMROSTER_ISDOWN (data))
                                ContactGroupC (serv, id, s_sprintf ("<group #%d>", tag));
                    PacketD (p);
                }
                else
                {
                    if (!(cg = ContactGroupFind (serv, tag, name)))
                        if (!(cg = ContactGroupFind (serv, tag, NULL)))
                            if (!(cg = ContactGroupFind (serv, 0, name)))
                                if (!IMROSTER_ISDOWN (data) || !(cg = ContactGroupC (serv, tag, name)))
                                    break;
                    if (IMROSTER_ISDOWN (data))
                    {
                        s_repl (&cg->name, name);
                        cg->id = tag;
                    }
                    rl_printf ("FIXME: Group #%08d '%s'\n", tag, name);
                }
                break;
            case 2:
            case 3:
            case 14:
            case 0:
                cg = ContactGroupFind (serv, tag, NULL);
                if (!tag || (!cg && data == IMROSTER_IMPORT))
                    break;
                if (!(cg = ContactGroupFind (serv, tag, NULL)))
                    if (!(cg = ContactGroupC (serv, tag, s_sprintf ("<group #%d>", tag))))
                        break;
                j = TLVGet (tlv, 305);
                assert (j < 200 || j == (UWORD)-1);
                nick = strdup (j != (UWORD)-1 && tlv[j].str.len ? ConvFromServ (&tlv[j].str) : name);

                switch (data)
                {
                    case 3:
                        if (j != (UWORD)-1 || !(cont = ContactFind (serv->contacts, 0, atoi (name), NULL)))
                        {
                            cont = ContactFindCreate (serv->contacts, id, atoi (name), nick);
                            SnacCliAddcontact (serv, cont);
                            k++;
                        }
                        if (!cont)
                            break;
                        if (!ContactFind (serv->contacts, 0, atoi (name), nick))
                            ContactFindCreate (serv->contacts, id, atoi (name), nick);
                        cont->id = id;   /* FIXME: should be in ContactGroup? */
                        if (type == 2)
                        {
                            OptSetVal (&cont->copts, CO_INTIMATE, 1);
                            OptSetVal (&cont->copts, CO_HIDEFROM, 0);
                        }
                        else if (type == 3)
                        {
                            OptSetVal (&cont->copts, CO_HIDEFROM, 1);
                            OptSetVal (&cont->copts, CO_INTIMATE, 0);
                        }
                        else if (type == 14)
                            OptSetVal (&cont->copts, CO_IGNORE, 1);
                        if (!ContactFind (cg, 0, cont->uin, NULL))
                        {
                            ContactAdd (cg, cont);
                            l++;
                        }
                        rl_printf (" #%d %10d %s\n", id, atoi (name), nick);
                        break;
                    case 2:
                        if ((cont = ContactFind (serv->contacts, id, atoi (name), nick)))
                            break;
                    case 1:
                        rl_printf (" #%08d %10d %s\n", id, atoi (name), nick);
                }
                free (nick);
                break;
            case 4:
            case 9:
            case 17:
                /* unknown / ignored */
                break;
        }
        free (name);
        TLVD (tlv);
    }
    stmp = PacketReadB4 (pak);
    if (!stmp && event2)
        QueueEnqueue (event2);
    else if (event2)
        EventD (event2);

    if (k || l)
    {
        rl_printf (i18n (2242, "Imported %d new contacts, added %d times to a contact group.\n"), k, l);
        if (serv->flags & CONN_WIZARD)
        {
            if (Save_RC () == -1)
                rl_print (i18n (1679, "Sorry saving your personal reply messages went wrong!\n"));
        }
        else
            rl_print (i18n (1754, "Note: You need to 'save' to write new contact list to disc.\n"));
    }
//    SnacCliRosterack (serv);
}

/*
 * CLI_ROSTERACK - SNAC(13,7)
 */

/* implemented as macro */

/*
 * CLI_ADDBUDDY - SNAC(13,8)
 */
void SnacCliRosteradd (Connection *serv, ContactGroup *cg, Contact *cont)
{
    Packet *pak;
    
    if (cont)
    {
/*        SnacCliGrantauth (serv, cont);  */
        SnacCliAddstart (serv);
        
        pak = SnacC (serv, 19, 8, 0, 0);
        PacketWriteStrB     (pak, s_sprintf ("%ld", cont->uin));
        PacketWriteB2       (pak, ContactGroupID (cg));
        PacketWriteB2       (pak, ContactID (cont));
        PacketWriteB2       (pak, 0);
        PacketWriteBLen     (pak);
        PacketWriteTLVStr   (pak, 305, cont->nick);
        if (cont->oldflags & CONT_REQAUTH)
        {
            PacketWriteTLV     (pak, 102);
            PacketWriteTLVDone (pak);
        }
        PacketWriteBLenDone (pak);
        QueueEnqueueData (serv, QUEUE_CHANGE_ROSTER, pak->ref, 0x7fffffffL, NULL, cont, NULL, NULL);
    }
    else
    {
        pak = SnacC (serv, 19, 8, 0, 0);
        PacketWriteStrB     (pak, cg->name);
        PacketWriteB2       (pak, ContactGroupID (cg));
        PacketWriteB2       (pak, 0);
        PacketWriteB2       (pak, 1);
        PacketWriteBLen     (pak);
        PacketWriteBLenDone (pak);
    }
    SnacSend (serv, pak);
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
        PacketWriteStrB     (pak, s_sprintf ("%ld", cont->uin));
        PacketWriteB2       (pak, ContactGroupID (cg));
        PacketWriteB2       (pak, ContactID (cont));
        PacketWriteB2       (pak, 0);
        PacketWriteBLen     (pak);
        PacketWriteTLVStr   (pak, 305, cont->nick);
        PacketWriteBLenDone (pak);
    }
    else
    {
        PacketWriteStrB     (pak, cg->name);
        PacketWriteB2       (pak, ContactGroupID (cg));
        PacketWriteB2       (pak, 0);
        PacketWriteB2       (pak, 1);
        PacketWriteBLen     (pak);
        PacketWriteTLV      (pak, 200);
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
            PacketWriteB2   (pak, ContactID (cont));
        PacketWriteTLVDone  (pak);
        PacketWriteBLenDone (pak);
    }
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
    PacketWriteB2       (pak, 5);
    PacketWriteTLV      (pak, 202);
    PacketWrite1        (pak, 4);
    PacketWriteTLVDone  (pak);
    PacketWriteBLenDone (pak);
    SnacSend (serv, pak);
}

/*
 * CLI_ROSTERDELETE - SANC(13,a)
 */
void SnacCliRosterdelete (Connection *serv, ContactGroup *cg, Contact *cont)
{
    Packet *pak;

    pak = SnacC (serv, 19, 10, 0, 0);
    if (cont)
    {
        PacketWriteStrB     (pak, s_sprintf ("%ld", cont->uin));
        PacketWriteB2       (pak, ContactGroupID (cg));
        PacketWriteB2       (pak, ContactID (cont));
        PacketWriteB2       (pak, 0);
        PacketWriteBLen     (pak);
        PacketWriteBLenDone (pak);
    }
    else
    {
        PacketWriteStrB     (pak, cg->name);
        PacketWriteB2       (pak, ContactGroupID (cg));
        PacketWriteB2       (pak, 0);
        PacketWriteB2       (pak, 1);
        PacketWriteBLen     (pak);
        PacketWriteBLenDone (pak);
    }
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
    
    event2 = QueueDequeue (serv, QUEUE_CHANGE_ROSTER, event->pak->ref);
    if (event2)
        cont = event2->cont;
    err = PacketReadB2 (event->pak);
    
    if (cont)
    {
        switch (err)
        {
            case 0xe:
                cont->oldflags |= CONT_REQAUTH;
                cont->oldflags &= ~CONT_ISSBL;
                SnacCliRosteradd (serv, serv->contacts, cont);
                EventD (event2);
                return;
            case 0x3:
                cont->id = 0;
            case 0x0:
                cont->oldflags |= ~CONT_ISSBL;
                EventD (event2);
                return;
        }
    }
    
    rl_printf (i18n (2325, "Warning: server based contact list change failed with error code %d.\n"), err);
    if (event2)
        EventD (event2);
}

/*
 * SRV_ROSTEROK - SNAC(13,f)
 */
JUMP_SNAC_F(SnacSrvRosterok)
{
    /* ignore */
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
    IMSrvMsg (cont, serv, NOW, opt);
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
    IMSrvMsg (cont, serv, NOW, opt);
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
    IMSrvMsg (cont, serv, NOW, opt);
}
