/*
 * Handles incoming and creates outgoing SNAC packets
 * for the family 21 (old ICQ) commands.
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
#include "oscar_base.h"
#include "oscar_tlv.h"
#include "oscar_snac.h"
#include "oscar_oldicq.h"
#include "packet.h"
#include "contact.h"
#include "connection.h"
#include "preferences.h"
#include "conv.h"
#include "icq_response.h"
#include "util_ui.h"

static Packet *SnacMetaC (Connection *serv, UWORD sub, UWORD type, UWORD ref);
static void SnacMetaSend (Connection *serv, Packet *pak);

/*
 * Create meta request package.
 */
static Packet *SnacMetaC (Connection *serv, UWORD sub, UWORD type, UWORD ref)
{
    Packet *pak;

    serv->our_seq3 = serv->our_seq3 ? (serv->our_seq3 + 1) % 0x7fff : 2;
    
    pak = SnacC (serv, 21, 2, 0, (ref ? ref : rand () % 0x7fff) + (serv->our_seq3 << 16));
    PacketWriteTLV (pak, 1);
    PacketWriteLen (pak);
    PacketWrite4   (pak, serv->uin);
    PacketWrite2   (pak, sub);
    PacketWrite2   (pak, serv->our_seq3);
    if (type)
        PacketWrite2 (pak, type);

    return pak;
}

/*
 * Complete & send meta request package.
 */
static void SnacMetaSend (Connection *serv, Packet *pak)
{
    PacketWriteLenDone (pak);
    PacketWriteTLVDone (pak);
    SnacSend (serv, pak);
}

/*
 * SRV_TOICQERR - SNAC(15,1)
 */
JUMP_SNAC_F(SnacSrvToicqerr)
{
    Packet *pak = event->pak;
    if ((pak->ref & 0xffff) == 0x4231)
    {
        M_print (i18n (2206, "The server doesn't want to give us offline messages.\n"));
    }
    else
    {
        UWORD err = PacketReadB2 (pak);
        Event *oevent;

        if ((oevent = QueueDequeue (event->conn, QUEUE_REQUEST_META, pak->ref)))
        {
            M_printf (i18n (2207, "Protocol error in command to old ICQ server: %d.\n"), err);
            if (err == 2)
                M_printf (i18n (9999, "You queried already too many users today - come back tomorrow.\n"));
            else if (err == 5)
                M_printf (i18n (9999, "The query got stuck. Or somesuch. Try again later.\n"));
            else
                M_printf (i18n (9999, "I'm out of wisdom about the server's problem. It just didn't work out.\n"));
            EventD (oevent);
        }
        else
        {
            M_printf (i18n (2207, "Protocol error in command to old ICQ server: %d.\n"), err);
            M_print (s_dump (pak->data + pak->rpos, pak->len - pak->rpos));
        }
    }
}

/*
 * CLI_REQOFFLINEMSGS - SNAC(15,2) - 60
 */
void SnacCliReqofflinemsgs (Connection *serv)
{
    Packet *pak;

    pak = SnacMetaC (serv, 60, 0, 0x4231);
    SnacMetaSend    (serv, pak);
}

/*
 * CLI_ACKOFFLINEMSGS - SNAC(15,2) - 62
 */
void SnacCliAckofflinemsgs (Connection *serv)
{
    Packet *pak;

    pak = SnacMetaC (serv, 62, 0, 0);
    SnacMetaSend    (serv, pak);
}

/*
 * CLI_METASETGENERAL - SNAC(15,2) - 2000/1002
 */
void SnacCliMetasetgeneral (Connection *serv, Contact *cont)
{
    Packet *pak;

    pak = SnacMetaC (serv, 2000, META_SET_GENERAL_INFO, 0);
    if (cont->meta_general)
    {
        PacketWriteLNTS (pak, c_out (cont->meta_general->nick));
        PacketWriteLNTS (pak, c_out (cont->meta_general->first));
        PacketWriteLNTS (pak, c_out (cont->meta_general->last));
        PacketWriteLNTS (pak, c_out (cont->meta_general->email));
        PacketWriteLNTS (pak, c_out (cont->meta_general->city));
        PacketWriteLNTS (pak, c_out (cont->meta_general->state));
        PacketWriteLNTS (pak, c_out (cont->meta_general->phone));
        PacketWriteLNTS (pak, c_out (cont->meta_general->fax));
        PacketWriteLNTS (pak, c_out (cont->meta_general->street));
        PacketWriteLNTS (pak, c_out (cont->meta_general->cellular));
        PacketWriteLNTS (pak, c_out (cont->meta_general->zip));
        PacketWrite2    (pak, cont->meta_general->country);
        PacketWrite1    (pak, cont->meta_general->tz);
        PacketWrite1    (pak, cont->meta_general->webaware);
    }
    else
    {
        PacketWriteLNTS (pak, c_out (cont->nick));
        PacketWriteLNTS (pak, "<unset>");
        PacketWriteLNTS (pak, "<unset>");
        PacketWriteLNTS (pak, "");
        PacketWriteLNTS (pak, "");
        PacketWriteLNTS (pak, "");
        PacketWriteLNTS (pak, "");
        PacketWriteLNTS (pak, "");
        PacketWriteLNTS (pak, "");
        PacketWriteLNTS (pak, "");
        PacketWriteLNTS (pak, "");
        PacketWrite2    (pak, 0);
        PacketWrite1    (pak, 0);
        PacketWrite1    (pak, 0);
    }
    SnacMetaSend    (serv, pak);
}

/*
 * CLI_METASETABOUT - SNAC(15,2) - 2000/1030
 */
void SnacCliMetasetabout (Connection *serv, const char *text)
{
    Packet *pak;

    pak = SnacMetaC (serv, 2000, META_SET_ABOUT_INFO, 0);
    PacketWriteLNTS (pak, c_out (text));
    SnacMetaSend    (serv, pak);
}

/*
 * CLI_METASETMORE - SNAC(15,2) - 2000/1021
 */
void SnacCliMetasetmore (Connection *serv, Contact *cont)
{
    Packet *pak;
    
    pak = SnacMetaC (serv, 2000, META_SET_MORE_INFO, 0);
    if (cont->meta_more)
    {
        PacketWrite2    (pak, cont->meta_more->age);
        PacketWrite1    (pak, cont->meta_more->sex);
        PacketWriteLNTS (pak, c_out (cont->meta_more->homepage));
        PacketWrite2    (pak, cont->meta_more->year);
        PacketWrite1    (pak, cont->meta_more->month);
        PacketWrite1    (pak, cont->meta_more->day);
        PacketWrite1    (pak, cont->meta_more->lang1);
        PacketWrite1    (pak, cont->meta_more->lang2);
        PacketWrite1    (pak, cont->meta_more->lang3);
    }
    else
    {
        PacketWrite2    (pak, 0);
        PacketWrite1    (pak, 0);
        PacketWriteLNTS (pak, "");
        PacketWrite2    (pak, 0);
        PacketWrite1    (pak, 0);
        PacketWrite1    (pak, 0);
        PacketWrite1    (pak, 0);
        PacketWrite1    (pak, 0);
        PacketWrite1    (pak, 0);
    }
    SnacMetaSend    (serv, pak);
}

/*
 * CLI_METASETPASS - SNAC(15,2) - 2000/1070
 */
void SnacCliMetasetpass (Connection *serv, const char *newpass)
{
    Packet *pak;
    
    pak = SnacMetaC (serv, 2000, 1070, 0);
    PacketWriteLNTS (pak, c_out (newpass));
    SnacMetaSend    (serv, pak);
}

/*
 * CLI_METAREQINFO - SNAC(15,2) - 2000/1202
 */
UDWORD SnacCliMetareqmoreinfo (Connection *serv, Contact *cont)
{
    Packet *pak;
    UDWORD ref;

    pak = SnacMetaC (serv, 2000, 1202, 0);
    ref = pak->ref;
    PacketWrite4    (pak, cont->uin);
    SnacMetaSend    (serv, pak);
    return ref;
}

/*
 * CLI_METAREQINFO - SNAC(15,2) - 2000/1232
 */
UDWORD SnacCliMetareqinfo (Connection *serv, Contact *cont)
{
    Packet *pak;
    UDWORD ref;

    pak = SnacMetaC (serv, 2000, META_REQ_INFO, 0);
    ref = pak->ref;
    PacketWrite4    (pak, cont->uin);
    SnacMetaSend    (serv, pak);
    return ref;
}

/*
 * CLI_SEARCHBYPERSINF - SNAC(15,2) - 2000/1375
 */
void SnacCliSearchbypersinf (Connection *serv, const char *email, const char *nick, const char *name, const char *surname)
{
    Packet *pak;

    pak = SnacMetaC  (serv, 2000, META_SEARCH_PERSINFO, 0);
    PacketWrite2     (pak, 320); /* key: first name */
    PacketWriteLLNTS (pak, c_out (name));
    PacketWrite2     (pak, 330); /* key: last name */
    PacketWriteLLNTS (pak, c_out (surname));
    PacketWrite2     (pak, 340); /* key: nick */
    PacketWriteLLNTS (pak, c_out (nick));
    PacketWrite2     (pak, 350); /* key: email address */
    PacketWriteLLNTS (pak, c_out (email));
    SnacMetaSend     (serv, pak);
}

/*
 * CLI_SEARCHBYMAIL - SNAC(15,2) - 2000/{1395,1321}
 */
void SnacCliSearchbymail (Connection *serv, const char *email)
{
    Packet *pak;

    pak = SnacMetaC  (serv, 2000, META_SEARCH_EMAIL, 0);
    PacketWrite2     (pak, 350); /* key: email address */
    PacketWriteLLNTS (pak, c_out (email));
    SnacMetaSend     (serv, pak);
}

/*
 * CLI_SEARCHRANDOM - SNAC(15,2) - 2000/1870
 */
UDWORD SnacCliSearchrandom (Connection *serv, UWORD group)
{
    Packet *pak;
    UDWORD ref;

    pak = SnacMetaC (serv, 2000, META_SEARCH_RANDOM, 0);
    ref = pak->ref;
    PacketWrite2    (pak, group);
    SnacMetaSend    (serv, pak);
    return ref;
}

/*
 * CLI_SETRANDOM - SNAC(15,2) - 2000/1880
 */
void SnacCliSetrandom (Connection *serv, UWORD group)
{
    Packet *pak;

    pak = SnacMetaC (serv, 2000, META_SET_RANDOM, serv->connect & CONNECT_OK ? 0 : 0x4242);
    PacketWrite2    (pak, group);
    if (group)
    {
        PacketWriteB4 (pak, 0x00000220);
        PacketWriteB4 (pak, 0);
        PacketWriteB4 (pak, 0);
        PacketWriteB4 (pak, 0);
        PacketWrite1  (pak, serv->assoc && serv->assoc->connect & CONNECT_OK
                            ? serv->assoc->status : 0);
        PacketWrite2  (pak, serv->assoc && serv->assoc->connect & CONNECT_OK
                            ? serv->assoc->version : 0);
        PacketWriteB4 (pak, 0);
        PacketWriteB4 (pak, 0x00005000);
        PacketWriteB4 (pak, 0x00000300);
        PacketWrite2  (pak, 0);
    }
    SnacMetaSend (serv, pak);
}

/*
 * CLI_SEARCHWP - SNAC(15,2) - 2000/1331
 */
void SnacCliSearchwp (Connection *serv, const MetaWP *wp)
{
    Packet *pak;

    pak = SnacMetaC (serv, 2000, META_SEARCH_WP, 0);
    PacketWriteLNTS    (pak, c_out (wp->first));
    PacketWriteLNTS    (pak, c_out (wp->last));
    PacketWriteLNTS    (pak, c_out (wp->nick));
    PacketWriteLNTS    (pak, c_out (wp->email));
    PacketWrite2       (pak, wp->minage);
    PacketWrite2       (pak, wp->maxage);
    PacketWrite1       (pak, wp->sex);
    PacketWrite1       (pak, wp->language);
    PacketWriteLNTS    (pak, c_out (wp->city));
    PacketWriteLNTS    (pak, c_out (wp->state));
    PacketWriteB2      (pak, wp->country);
    PacketWriteLNTS    (pak, c_out (wp->company));
    PacketWriteLNTS    (pak, c_out (wp->department));
    PacketWriteLNTS    (pak, c_out (wp->position));
    PacketWrite1       (pak, 0);    /* occupation); */
    PacketWriteB2      (pak, 0);    /* past information); */
    PacketWriteLNTS    (pak, NULL); /* description); */
    PacketWriteB2      (pak, 0);    /* interests-category); */
    PacketWriteLNTS    (pak, NULL); /* interests-specification); */
    PacketWriteB2      (pak, 0);    /* affiliation/organization); */
    PacketWriteLNTS    (pak, NULL); /* description); */
    PacketWriteB2      (pak, 0);    /* homepage category); */
    PacketWriteLNTS    (pak, NULL); /* description); */
    PacketWrite1       (pak, wp->online);
    SnacMetaSend (serv, pak);
}

/*
 * CLI_SENDSMS - SNAC(15,2) - 2000/5250
 */
void SnacCliSendsms (Connection *serv, const char *target, const char *text)
{
    Packet *pak;
    char buf[2000], tbuf[100];
    time_t t = time (NULL);

    strftime (tbuf, sizeof (tbuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime (&t));
    snprintf (buf, sizeof (buf), "<icq_sms_message><destination>%s</destination>"
             "<text>%s (%ld www.mICQ.org)</text><codepage>1252</codepage><senders_UIN>%ld</senders_UIN>"
             "<senders_name>%s</senders_name><delivery_receipt>Yes</delivery_receipt>"
             "<time>%s</time></icq_sms_message>",
             target, text, serv->uin, serv->uin, "mICQ", tbuf);

    pak = SnacMetaC (serv, 2000, META_SEND_SMS, 0);
    PacketWriteB2      (pak, 1);
    PacketWriteB2      (pak, 22);
    PacketWriteB4      (pak, 0);
    PacketWriteB4      (pak, 0);
    PacketWriteB4      (pak, 0);
    PacketWriteB4      (pak, 0);
    PacketWriteTLVStr  (pak, 0, buf);
    SnacMetaSend (serv, pak);
}

/*
 * SRV_FROMOLDICQ - SNAC(15,3)
 */
JUMP_SNAC_F(SnacSrvFromicqsrv)
{
    Connection *serv = event->conn;
    TLV *tlv;
    Packet *p, *pak;
    UDWORD len, uin, type /*, id*/;
    
    pak = event->pak;
    tlv = TLVRead (pak, PacketReadLeft (pak));
    if (tlv[1].str.len < 10)
    {
        SnacSrvUnknown (event);
        TLVD (tlv);
        return;
    }
    p = PacketCreate (&tlv[1].str);
    p->ref = pak->ref; /* copy reference */
    len = PacketRead2 (p);
    uin = PacketRead4 (p);
    type= PacketRead2 (p);
/*  id=*/ PacketRead2 (p);
    if (uin != serv->uin)
    {
        if (prG->verbose & DEB_PROTOCOL)
        {
            M_printf (i18n (1919, "UIN mismatch: %ld vs %ld.\n"), serv->uin, uin);
            SnacSrvUnknown (event);
        }
        TLVD (tlv);
        PacketD (p);
        return;
    }
    else if (len != tlv[1].str.len - 2)
    {
        if (prG->verbose & DEB_PROTOCOL)
        {
            M_print (i18n (1743, "Size mismatch in packet lengths.\n"));
            SnacSrvUnknown (event);
        }
        TLVD (tlv);
        PacketD (p);
        return;
    }

    TLVD (tlv);
    switch (type)
    {
        case 65:
            if (len >= 14)
                Recv_Message (serv, p);
            break;

        case 66:
            SnacCliAckofflinemsgs (serv);
            break;

        case 2010:
            Meta_User (serv, ContactUIN (serv, uin), p);
            break;

        default:
            SnacSrvUnknown (event);
            break;
    }
    PacketD (p);
}
