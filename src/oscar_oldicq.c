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
#include "oscar_base.h"
#include "oscar_tlv.h"
#include "oscar_snac.h"
#include "oscar_oldicq.h"
#include "oscar_roster.h"
#include "packet.h"
#include "contact.h"
#include "connection.h"
#include "preferences.h"
#include "conv.h"
#include "icq_response.h"
#include "util_ui.h"

static Packet *SnacMetaC (Connection *serv, UWORD sub, UWORD type, UWORD ref);
static void SnacMetaSend (Connection *serv, Packet *pak);

#define PacketWriteMetaLNTS(p,t,s) { PacketWrite2 (p, t); PacketWriteLen (p); PacketWriteLNTS (p, s); PacketWriteLenDone (p); }
#define PacketWriteMeta1(p,t,i)    { PacketWrite2 (p, t); PacketWrite2 (p, 1); PacketWrite1 (p, i); }
#define PacketWriteMeta2(p,t,i)    { PacketWrite2 (p, t); PacketWrite2 (p, 2); PacketWrite2 (p, i); }
#define PacketWriteMeta4(p,t,i)    { PacketWrite2 (p, t); PacketWrite2 (p, 4); PacketWrite4 (p, i); }

#define META_TAG_UIN         310 /* LE32 */
#define META_TAG_FIRST       320
#define META_TAG_LAST        330
#define META_TAG_NICK        340
#define META_TAG_EMAIL       350 /* LNTS B */
#define META_TAG_AGERANGE    360 /* LE16 LE16 */
#define META_TAG_AGE         370 /* LE16 */
#define META_TAG_GENDER      380 /* B */
#define META_TAG_LANG        390 /* LE16 */
#define META_TAG_HOME_CITY   400
#define META_TAG_HOME_STATE  410
#define META_TAG_HOME_CTRY   420 /* LE16 */
#define META_TAG_WRK_COMP    430
#define META_TAG_WRK_DEPT    440
#define META_TAG_WRK_POS     450
#define META_TAG_WRK_OCC     460 /* LE16 */
#define META_TAG_AFFIL       470 /* LE16 LNTS */

#define META_TAG_INTER       490 /* LE16 LNTS */

#define META_TAG_PASTINFO    510 /* LE16 LNTS */

#define META_TAG_HP_CAT      530 /* LE16 LNTS */
#define META_TAG_HP_URL      531 /* LE16 LNTS */

#define META_TAG_SRCH_STRING 550
#define META_TAG_SRCH_ONLINE 560 /* B */
#define META_TAG_BIRTHDAY    570 /* LE16 LE16 LE16 */

#define META_TAG_ABOUT       600
#define META_TAG_HOME_STREET 610
#define META_TAG_HOME_ZIP    620 /* LE32 */
#define META_TAG_HOME_PHONE  630
#define META_TAG_HOME_FAX    640
#define META_TAG_HOME_CELL   650
#define META_TAG_WRK_STREET  660
#define META_TAG_WRK_CITY    670
#define META_TAG_WRK_STATE   680
#define META_TAG_WRK_CTRY    690 /* LE16 */
#define META_TAG_WRK_ZIP     700 /* LE32 */
#define META_TAG_WRK_PHONE   710
#define META_TAG_WRK_FAX     720
#define META_TAG_WRK_URL     730

#define META_TAG_AUTHREQ     760 /* B (yes, 760, NOT 780) */
#define META_TAG_WEBAWARE    780 /* B */

#define META_TAG_TZ          790 /* SB */
#define META_TAG_ORIG_CITY   800
#define META_TAG_ORIG_STATE  810
#define META_TAG_ORIG_CTRY   820 /* LE16 */

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
        rl_print (i18n (2206, "The server doesn't want to give us offline messages.\n"));
    }
    else
    {
        UWORD err = PacketReadB2 (pak);
        Event *oevent;

        if ((oevent = QueueDequeue (event->conn, QUEUE_REQUEST_META, pak->ref)))
        {
            rl_printf (i18n (2207, "Protocol error in command to old ICQ server: %d.\n"), err);
            if (err == 2)
                rl_printf (i18n (2515, "You queried already too many users today - come back tomorrow.\n"));
            else if (err == 5)
                rl_printf (i18n (2516, "The query got stuck. Or somesuch. Try again later.\n"));
            else
                rl_printf (i18n (2517, "I'm out of wisdom about the server's problem. It just didn't work out.\n"));
            EventD (oevent);
        }
        else if ((pak->ref & 0xffff) != 0x4242)
        {
            rl_printf (i18n (2207, "Protocol error in command to old ICQ server: %d.\n"), err);
            rl_print (s_dump (pak->data + pak->rpos, pak->len - pak->rpos));
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

#if 0
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
#else
    pak = SnacMetaC (serv, 2000, META_SAVE_INFO, 0);
    if (cont->meta_general)
    {
        ContactMeta *ml;

        PacketWriteMetaLNTS (pak, META_TAG_NICK, c_out (cont->meta_general->nick));
        PacketWriteMetaLNTS (pak, META_TAG_FIRST, c_out (cont->meta_general->first));
        PacketWriteMetaLNTS (pak, META_TAG_LAST, c_out (cont->meta_general->last));

        PacketWrite2 (pak, META_TAG_EMAIL);
        PacketWriteLen (pak);
        PacketWriteLNTS (pak, c_out (cont->meta_general->email));
        PacketWrite1 (pak, 0);
        PacketWriteLenDone  (pak);

        for (ml = cont->meta_email ; ml; ml = ml->next)
        {
            if (!ml->text || !*ml->text)
                continue;
            PacketWrite2 (pak, META_TAG_EMAIL);
            PacketWriteLen (pak);
            PacketWriteLNTS (pak, c_out (ml->text));
            PacketWrite1 (pak, ml->data);
            PacketWriteLenDone  (pak);
        }
        
        PacketWriteMetaLNTS (pak, META_TAG_HOME_CITY, c_out (cont->meta_general->city));
        PacketWriteMetaLNTS (pak, META_TAG_HOME_STATE, c_out (cont->meta_general->state));
        PacketWriteMetaLNTS (pak, META_TAG_HOME_PHONE, c_out (cont->meta_general->phone));
        PacketWriteMetaLNTS (pak, META_TAG_HOME_FAX, c_out (cont->meta_general->fax));
        PacketWriteMetaLNTS (pak, META_TAG_HOME_STREET, c_out (cont->meta_general->street));
        PacketWriteMetaLNTS (pak, META_TAG_HOME_CELL, c_out (cont->meta_general->cellular));
        PacketWriteMeta4    (pak, META_TAG_HOME_ZIP, atoi (cont->meta_general->zip));

        PacketWriteMeta2    (pak, META_TAG_HOME_CTRY, cont->meta_general->country);
        PacketWriteMeta2    (pak, 835, 0);
        PacketWriteMeta4    (pak, 621, 0);
        PacketWriteMeta1    (pak, META_TAG_TZ, -cont->meta_general->tz/2);

        PacketWriteMeta1    (pak, META_TAG_WEBAWARE, cont->meta_general->webaware);
        PacketWriteMeta1    (pak, META_TAG_AUTHREQ, !cont->meta_general->auth);
    }
    else
    {
        PacketWriteMetaLNTS (pak, META_TAG_NICK, c_out (cont->nick));
        PacketWriteMetaLNTS (pak, META_TAG_FIRST, "<unset>");
        PacketWriteMetaLNTS (pak, META_TAG_LAST, "<unset>");

        PacketWrite2 (pak, META_TAG_EMAIL);
        PacketWriteLen (pak);
        PacketWriteLNTS (pak, "");
        PacketWrite1 (pak, 1);
        PacketWriteLenDone  (pak);

        PacketWriteMetaLNTS (pak, META_TAG_HOME_CITY, "");
        PacketWriteMetaLNTS (pak, META_TAG_HOME_STATE, "");
        PacketWriteMetaLNTS (pak, META_TAG_HOME_PHONE, "");
        PacketWriteMetaLNTS (pak, META_TAG_HOME_FAX, "");
        PacketWriteMetaLNTS (pak, META_TAG_HOME_STREET, "");
        PacketWriteMetaLNTS (pak, META_TAG_HOME_CELL, "");
        PacketWriteMetaLNTS (pak, META_TAG_HOME_ZIP, "");
        PacketWriteMeta2    (pak, META_TAG_HOME_CTRY, 0);
        PacketWriteMeta1    (pak, META_TAG_TZ, 0);
        PacketWriteMeta1    (pak, META_TAG_WEBAWARE, 0);
        PacketWriteMeta1    (pak, META_TAG_AUTHREQ, 0);
    }

#if 0    
    PacketWriteMeta1    (pak, 760, 0);
    PacketWriteMeta1    (pak, 780, 0);
    PacketWriteMeta4    (pak, 620, 0);
    PacketWriteMeta2    (pak, 420, 0);
    PacketWriteMeta1    (pak, 790, -2);
    PacketWriteMeta2    (pak, 820, 0);
    PacketWriteMeta2    (pak, 370, 99);
    PacketWriteMeta2    (pak, 380, 2);
    PacketWriteMeta2    (pak, 531, 0);

    PacketWrite2 (pak, 570);
    PacketWrite2 (pak, 6);
    PacketWrite2 (pak, 0);
    PacketWrite2 (pak, 0);
    PacketWrite2 (pak, 0);
    
    PacketWrite2 (pak, 350);
    PacketWriteLen (pak);
    PacketWriteLNTS (pak, "email@adresse");
    PacketWrite1 (pak, 1);
    PacketWriteLenDone  (pak);
    
    PacketWriteMeta4    (pak, 690, 0);
    PacketWriteMeta2    (pak, 460, 0);
    PacketWriteMeta4    (pak, 490, 0);
    PacketWriteMeta4    (pak, 490, 0);
    PacketWriteMeta4    (pak, 490, 0);
    PacketWriteMeta4    (pak, 470, 0);
    PacketWriteMeta4    (pak, 470, 0);
    PacketWriteMeta4    (pak, 470, 0);
    PacketWriteMeta4    (pak, 510, 0);
    PacketWriteMeta4    (pak, 510, 0);
    PacketWriteMeta4    (pak, 510, 0);
#endif

#endif
    SnacMetaSend    (serv, pak);
    SnacCliSetlastupdate (serv);
}

/*
 * CLI_METASETABOUT - SNAC(15,2) - 2000/1030
 */

void SnacCliMetasetabout (Connection *serv, const char *text)
{
    Packet *pak;
#if 0
    pak = SnacMetaC (serv, 2000, META_SET_ABOUT_INFO, 0);
    PacketWriteLNTS (pak, c_out (text));
#else
    pak = SnacMetaC (serv, 2000, META_SAVE_INFO, 0);
    PacketWriteMetaLNTS (pak, META_TAG_ABOUT, c_out (text));
#endif
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
    char buf[2000];
    time_t t = time (NULL);
    
    snprintf (buf, sizeof (buf), "<icq_sms_message><destination>%s</destination>"
             "<text>%s (%s www.mICQ.org)</text><codepage>1252</codepage><senders_UIN>%s</senders_UIN>"
             "<senders_name>%s</senders_name><delivery_receipt>Yes</delivery_receipt>"
             "<time>%s</time></icq_sms_message>",
             target, text, serv->screen, serv->screen, "mICQ",
             s_strftime (&t, "%a, %d %b %Y %H:%M:%S GMT", 1));

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
    tlv = TLVRead (pak, PacketReadLeft (pak), -1);
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
            rl_printf (i18n (1919, "UIN mismatch: %ld vs %ld.\n"), serv->uin, uin);
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
            rl_print (i18n (1743, "Size mismatch in packet lengths.\n"));
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
