/*
 * Handles incoming and creates outgoing SNAC packets
 * for the family 21 (old ICQ) commands.
 *
 * climm Copyright (C) © 2001-2010 Rüdiger Kuhlmann
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
#include <assert.h>
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
#include "im_response.h"
#include "im_request.h"
#include "util_ui.h"

static Packet *SnacMetaC (Server *serv, UWORD sub, UWORD type, UWORD ref);
static void SnacMetaSend (Server *serv, Packet *pak);

#define PacketWriteMetaLNTS(p,t,s)  { PacketWrite2 (p, t); PacketWriteLen (p); PacketWriteLNTS (p, s); PacketWriteLenDone (p); }
#define PacketWriteMeta1(p,t,i)     { PacketWrite2 (p, t); PacketWrite2 (p, 1); PacketWrite1 (p, i); }
#define PacketWriteMeta2(p,t,i)     { PacketWrite2 (p, t); PacketWrite2 (p, 2); PacketWrite2 (p, i); }
#define PacketWriteMeta4(p,t,i)     { PacketWrite2 (p, t); PacketWrite2 (p, 4); PacketWrite4 (p, i); }
#define PacketWriteMeta6(p,t,i,j,k) { PacketWrite2 (p, t); PacketWrite2 (p, 6); PacketWrite2 (p, i); PacketWrite2 (p, j); PacketWrite2 (p, k); }

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
static Packet *SnacMetaC (Server *serv, UWORD sub, UWORD type, UWORD ref)
{
    Packet *pak;

    serv->oscar_icq_seq = serv->oscar_icq_seq ? (serv->oscar_icq_seq + 1) % 0x7fff : 2;
    
    pak = SnacC (serv, 21, 2, 0, (ref ? ref : rand () % 0x7fff) + (serv->oscar_icq_seq << 16));
    PacketWriteTLV (pak, 1);
    PacketWriteLen (pak);
    PacketWrite4   (pak, serv->oscar_uin);
    PacketWrite2   (pak, sub);
    PacketWrite2   (pak, serv->oscar_icq_seq);
    if (type)
        PacketWrite2 (pak, type);

    return pak;
}

/*
 * Complete & send meta request package.
 */
static void SnacMetaSend (Server *serv, Packet *pak)
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
    Server *serv = event->conn->serv;
    Packet *pak = event->pak;
    if ((pak->ref & 0xffff) == 0x4231)
    {
        rl_print (i18n (2206, "The server doesn't want to give us offline messages.\n"));
    }
    else
    {
        UWORD err = PacketReadB2 (pak);
        Event *oevent;

        if ((oevent = QueueDequeue (serv->conn, QUEUE_REQUEST_META, pak->ref)))
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
void SnacCliReqofflinemsgs (Server *serv)
{
    Packet *pak;

    pak = SnacMetaC (serv, 60, 0, 0x4231);
    SnacMetaSend    (serv, pak);
}

/*
 * CLI_ACKOFFLINEMSGS - SNAC(15,2) - 62
 */
void SnacCliAckofflinemsgs (Server *serv)
{
    Packet *pak;

    pak = SnacMetaC (serv, 62, 0, 0);
    SnacMetaSend    (serv, pak);
}

/*
 * CLI_METASETGENERAL - SNAC(15,2) - 2000/1002
 */
void SnacCliMetasetgeneral (Server *serv, Contact *cont)
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

void SnacCliMetasetabout (Server *serv, const char *text)
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
    SnacCliSetlastupdate (serv);
}

/*
 * CLI_METASETMORE - SNAC(15,2) - 2000/1021
 */
void SnacCliMetasetmore (Server *serv, Contact *cont)
{
    Packet *pak;
    
#if 0
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
#else
    pak = SnacMetaC (serv, 2000, META_SAVE_INFO, 0);
    if (cont->meta_more)
    {
        PacketWriteMeta2    (pak, META_TAG_AGE, cont->meta_more->age);
        PacketWriteMeta1    (pak, META_TAG_GENDER, cont->meta_more->sex);
        PacketWriteMetaLNTS (pak, META_TAG_HP_URL, c_out (cont->meta_more->homepage));
        PacketWriteMeta6    (pak, META_TAG_BIRTHDAY, cont->meta_more->year, cont->meta_more->month, cont->meta_more->day);
        PacketWriteMeta1    (pak, META_TAG_LANG, cont->meta_more->lang1);
        PacketWriteMeta1    (pak, META_TAG_LANG, cont->meta_more->lang2);
        PacketWriteMeta1    (pak, META_TAG_LANG, cont->meta_more->lang3);
    }
    else
    {
        PacketWriteMeta2    (pak, META_TAG_AGE, 0);
        PacketWriteMeta1    (pak, META_TAG_GENDER, 0);
        PacketWriteMetaLNTS (pak, META_TAG_HP_URL, "");
        PacketWriteMeta6    (pak, META_TAG_BIRTHDAY, 0, 0, 0);
        PacketWriteMeta1    (pak, META_TAG_LANG, 0);
        PacketWriteMeta1    (pak, META_TAG_LANG, 0);
        PacketWriteMeta1    (pak, META_TAG_LANG, 0);
    }


#endif
    SnacMetaSend    (serv, pak);
    SnacCliSetlastupdate (serv);
}

/*
 * CLI_METASETPASS - SNAC(15,2) - 2000/1070
 */
void SnacCliMetasetpass (Server *serv, const char *newpass)
{
    Packet *pak;
    
    pak = SnacMetaC (serv, 2000, 1070, 0);
    PacketWriteLNTS (pak, c_out (newpass));
    SnacMetaSend    (serv, pak);
}

/*
 * CLI_METAREQINFO - SNAC(15,2) - 2000/1202
 */
UDWORD SnacCliMetareqmoreinfo (Server *serv, Contact *cont)
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
UDWORD SnacCliMetareqinfo (Server *serv, Contact *cont)
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
void SnacCliSearchbypersinf (Server *serv, const char *email, const char *nick, const char *name, const char *surname)
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
void SnacCliSearchbymail (Server *serv, const char *email)
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
UDWORD SnacCliSearchrandom (Server *serv, UWORD group)
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
void SnacCliSetrandom (Server *serv, UWORD group)
{
    Packet *pak;

    pak = SnacMetaC (serv, 2000, META_SET_RANDOM, serv->conn->connect & CONNECT_OK ? 0 : 0x4242);
    PacketWrite2    (pak, group);
    if (group)
    {
        PacketWriteB4 (pak, 0x00000220);
        PacketWriteB4 (pak, 0);
        PacketWriteB4 (pak, 0);
        PacketWriteB4 (pak, 0);
        PacketWrite1  (pak, serv->oscar_dc && serv->oscar_dc->connect & CONNECT_OK
                            ? ServerPrefVal (serv, CO_OSCAR_DC_MODE) & 15 : 0);
        PacketWrite2  (pak, serv->oscar_dc && serv->oscar_dc->connect & CONNECT_OK
                            ? serv->oscar_dc->version : 0);
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
void SnacCliSearchwp (Server *serv, const MetaWP *wp)
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
void SnacCliSendsms (Server *serv, const char *target, const char *text)
{
    Packet *pak;
    char buf[2000];
    time_t t = time (NULL);
    
    snprintf (buf, sizeof (buf), "<icq_sms_message><destination>%s</destination>"
             "<text>%s (%s www.climm.org)</text><codepage>utf8</codepage><senders_UIN>%s</senders_UIN>"
             "<senders_name>%s</senders_name><delivery_receipt>Yes</delivery_receipt>"
             "<time>%s</time></icq_sms_message>",
             target, text, serv->screen, serv->screen, "climm",
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
    Server *serv = event->conn->serv;
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
    if (uin != serv->oscar_uin)
    {
        if (prG->verbose & DEB_PROTOCOL)
        {
            rl_printf (i18n (1919, "UIN mismatch: %ld vs %ld.\n"), UD2UL (serv->oscar_uin), UD2UL (uin));
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

#define META_SRV_GEN_UPDATE     100
#define META_SRV_OTHER_UPDATE   120
#define META_SRV_ABOUT_UPDATE   130
#define META_SRV_SMS_OK         150
#define META_SRV_PASS_UPDATE    170
#define META_SRV_GEN            200
#define META_SRV_WORK           210
#define META_SRV_MORE           220
#define META_SRV_ABOUT          230
#define META_SRV_INTEREST       240
#define META_SRV_BACKGROUND     250
#define META_SRV_MOREEMAIL      235
#define META_SRV_INFO           260
#define META_SRV_UNKNOWN_270    270
#define META_SRV_WP_FOUND       420
#define META_SRV_WP_LAST_USER   430
#define META_SRV_RANDOM         870
#define META_SRV_RANDOM_UPDATE  880
#define META_SRV_METATLV_UPDATE 3135

void Auto_Reply (Server *serv, Contact *cont)
{
    const char *temp = NULL;

    if (!(prG->flags & FLAG_AUTOREPLY) || !cont)
        return;

    switch (ContactClearInv (cont->status))
    {
        case imr_dnd:  temp = ContactPrefStr (cont, CO_TAUTODND);  break;
        case imr_occ:  temp = ContactPrefStr (cont, CO_TAUTOOCC);  break;
        case imr_na:   temp = ContactPrefStr (cont, CO_TAUTONA);   break;
        case imr_away: temp = ContactPrefStr (cont, CO_TAUTOAWAY); break;
        case imr_ffc:  temp = ContactPrefStr (cont, CO_TAUTOFFC);  break;
        case imr_offline: return;
        case imr_online:
            if (ContactIsInv (cont->status))
                       temp = ContactPrefStr (cont, CO_TAUTOINV);
            else
                return;
    }

    if (!temp || !*temp)
    {
        switch (ContactClearInv (cont->status))
        {
            case imr_dnd:  temp = ContactPrefStr (cont, CO_AUTODND);  break;
            case imr_occ:  temp = ContactPrefStr (cont, CO_AUTOOCC);  break;
            case imr_na:   temp = ContactPrefStr (cont, CO_AUTONA);   break;
            case imr_away: temp = ContactPrefStr (cont, CO_AUTOAWAY); break;
            case imr_ffc:  temp = ContactPrefStr (cont, CO_AUTOFFC);  break;
            case imr_offline: assert (0);
            case imr_online:
                assert (ContactIsInv (cont->status));
                           temp = ContactPrefStr (cont, CO_AUTOINV);
        }
    }

    IMCliMsg (cont, MSG_AUTO, temp, NULL);
}

#define s_read(s) s_repl (&s, ConvFromCont (PacketReadL2Str (pak, NULL), cont))

static BOOL Meta_Read_List (Packet *pak, ContactMeta **list, Contact *cont)
{
    UBYTE i, j;
    UWORD data;
    const char *text;

    i = PacketRead1 (pak);
    ContactMetaD (*list);
    *list = NULL;
    for (j = 0; j < i; j++)
    {
        data = PacketRead2 (pak);
        text = ConvFromCont (PacketReadL2Str (pak, NULL), cont);
        if (data && text && *text)
            ContactMetaAdd (list, data, text);
    }
    return TRUE;
}

void Meta_User (Server *serv, Contact *cont, Packet *pak)
{
    UWORD subtype;
    UDWORD result;
    Event *event = NULL;

    if (!cont)
        return;
    subtype = PacketRead2 (pak);
    result  = PacketRead1 (pak);

    switch (subtype)
    {
        case META_SRV_PASS_UPDATE:
            rl_printf (i18n (2136, "Password change was %s%s%s.\n"),
                     COLCLIENT, result == META_SUCCESS ? i18n (1393, "successful")
                     : i18n (1394, "unsuccessful"), COLNONE);
            break;
        case META_SRV_ABOUT_UPDATE:
            rl_printf (i18n (2137, "About info change was %s%s%s.\n"),
                     COLCLIENT, result == META_SUCCESS ? i18n (1393, "successful")
                     : i18n (1394, "unsuccessful"), COLNONE);
            break;
        case META_SRV_GEN_UPDATE:
            rl_printf (i18n (2138, "Info change was %s%s%s.\n"),
                     COLCLIENT, result == META_SUCCESS ? i18n (1393, "successful")
                     : i18n (1394, "unsuccessful"), COLNONE);
            break;
        case META_SRV_OTHER_UPDATE:
            rl_printf (i18n (2139, "Other info change was %s%s%s.\n"),
                     COLCLIENT, result == META_SUCCESS ? i18n (1393, "successful")
                     : i18n (1394, "unsuccessful"), COLNONE);
            break;
        case META_SRV_RANDOM_UPDATE:
            if ((pak->ref & 0xffff) != 0x4242)
                rl_printf (i18n (2140, "Random chat group change was %s%s%s.\n"),
                         COLCLIENT, result == META_SUCCESS ? i18n (1393, "successful")
                         : i18n (1394, "unsuccessful"), COLNONE);
            break;
        case META_SRV_METATLV_UPDATE:
            rl_printf (i18n (2690, "Meta info change was %s%s%s.\n"),
                     COLCLIENT, result == META_SUCCESS ? i18n (1393, "successful")
                     : i18n (1394, "unsuccessful"), COLNONE);
            break;
    }

    switch (result) /* default error handling */
    {
        case 0x32:
        case 0x14:
            if ((event = QueueDequeue (serv->conn, QUEUE_REQUEST_META, pak->ref)))
                EventD (event);
            rl_printf ("%s ", s_now);
            rl_printf (i18n (2141, "Search %sfailed%s.\n"), COLCLIENT, COLNONE);
            return;
        case META_READONLY:
            rl_printf ("%s %s\n", s_now, i18n (1900, "It's readonly."));
            return;
        case META_SUCCESS:
            break;
        case 0x46:
            rl_printf ("%s\n", pak->data + pak->rpos);
            return;
        default:
            rl_printf (i18n (1940, "Unknown Meta User result %lx.\n"), UD2UL (result));
            return;
    }

    switch (subtype)
    {
        case META_SRV_ABOUT_UPDATE:
        case META_SRV_OTHER_UPDATE:
        case META_SRV_GEN_UPDATE:
        case META_SRV_PASS_UPDATE:
        case META_SRV_RANDOM_UPDATE:
        case META_SRV_METATLV_UPDATE:
            return;
        case META_SRV_INFO:
        case META_SRV_GEN:
        case META_SRV_MORE:
        case META_SRV_MOREEMAIL:
        case META_SRV_WORK:
        case META_SRV_ABOUT:
        case META_SRV_INTEREST:
        case META_SRV_BACKGROUND:
        case META_SRV_UNKNOWN_270:
        case META_SRV_RANDOM:
            if (!(event = QueueDequeue (serv->conn, QUEUE_REQUEST_META, pak->ref)) || !event->callback)
            {
                if (prG->verbose)
                    rl_printf ("FIXME: meta reply ref %lx not found.\n", UD2UL (pak->ref));
                return;
            }

            if (event->cont)
                cont = event->cont;
    }

    switch (subtype)
    {
        strc_t data;
        UWORD wdata, i, j;
        UDWORD dwdata, uin;
        MetaGeneral *mg;
        MetaMore *mm;
        MetaWork *mw;
        MetaObsolete *mo;

        case META_SRV_SMS_OK:
                   PacketRead4 (pak);
                   PacketRead2 (pak);
                   PacketReadB2Str (pak, NULL);
            data = PacketReadB2Str (pak, NULL);
            rl_printf (i18n (2080, "Server SMS delivery response:\n%s\n"), ConvFromServ (data));
            break;
        case META_SRV_INFO:
            Display_Info_Reply (cont, pak, 0);
            /* 3 unknown bytes ignored */

            event->callback (event);
            break;
        case META_SRV_GEN:
            Display_Info_Reply (cont, pak, 0);

            if (!(mg = CONTACT_GENERAL (cont)))
                break;

            s_read (mg->city);
            s_read (mg->state);
            s_read (mg->phone);
            s_read (mg->fax);
            s_read (mg->street);
            s_read (mg->cellular);
            if (serv->type == TYPE_SERVER)
                s_read (mg->zip);
            else
            {
                dwdata = PacketRead4 (pak);
                s_repl (&mg->zip, dwdata ? s_sprintf ("%ld", UD2UL (dwdata)) : "");
            }
            mg->country = PacketRead2 (pak);
            mg->tz      = PacketRead1 (pak);
            mg->auth    = PacketRead1 (pak);
            /* one unknown word ignored according to v7 doc */
            /* probably more security settings? */

            cont->updated |= UPF_GENERAL_B;
            event->callback (event);
            break;
        case META_SRV_MORE:
            if (!(mm = CONTACT_MORE (cont)))
                break;

            mm->age = PacketRead2 (pak);
            mm->sex = PacketRead1 (pak);
            s_read (mm->homepage);
            mm->year  = PacketRead2 (pak);
            mm->month = PacketRead1 (pak);
            mm->day   = PacketRead1 (pak);
            mm->lang1 = PacketRead1 (pak);
            mm->lang2 = PacketRead1 (pak);
            mm->lang3 = PacketRead1 (pak);
            /* one unknown word ignored according to v7 doc */

            cont->updated |= UPF_MORE;
            event->callback (event);
            break;
        case META_SRV_MOREEMAIL:
            ContactMetaD (cont->meta_email);
            cont->meta_email = NULL;
            if ((i = PacketRead1 (pak)))
            {
                for (j = 0; j < i; j++)
                {
                    int auth = PacketRead1 (pak);
                    ContactMetaAdd (&cont->meta_email, auth, ConvFromCont (PacketReadL2Str (pak, NULL), cont));
                }
            }
            cont->updated |= UPF_EMAIL;
            event->callback (event);
            break;
        case META_SRV_WORK:
            if (!(mw = CONTACT_WORK (cont)))
                break;

            s_read (mw->wcity);
            s_read (mw->wstate);
            s_read (mw->wphone);
            s_read (mw->wfax);
            s_read (mw->waddress);
            if (serv->type == TYPE_SERVER)
                s_read (mw->wzip);
            else
            {
                dwdata = PacketRead4 (pak);
                s_repl (&mw->wzip, dwdata ? s_sprintf ("%ld", UD2UL (dwdata)) : "");
            }
            mw->wcountry = PacketRead2 (pak);
            s_read (mw->wcompany);
            s_read (mw->wdepart);
            s_read (mw->wposition);
            mw->woccupation = PacketRead2 (pak);
            s_read (mw->whomepage);

            cont->updated |= UPF_WORK;
            event->callback (event);
            break;
        case META_SRV_ABOUT:
            s_read (cont->meta_about);
            cont->updated |= UPF_ABOUT;
            event->callback (event);
            break;
        case META_SRV_INTEREST:
            if (!Meta_Read_List (pak, &cont->meta_interest, cont))
                break;

            cont->updated |= UPF_INTEREST;
            event->callback (event);
            break;
        case META_SRV_BACKGROUND:
            if (!Meta_Read_List (pak, &cont->meta_background, cont))
                break;
            if (!Meta_Read_List (pak, &cont->meta_affiliation, cont))
                break;

            cont->updated |= UPF_BACKGROUND | UPF_AFFILIATION;
            event->callback (event);
            break;
        case META_SRV_WP_FOUND:
        case META_SRV_WP_LAST_USER:
            if (PacketRead2 (pak) < 19)
            {
                rl_printf (i18n (2141, "Search %sfailed%s.\n"), COLCLIENT, COLNONE);
                break;
            }
            cont = ContactUIN (serv, PacketRead4 (pak));
            if (!cont || !(mg = CONTACT_GENERAL (cont)) || !(mm = CONTACT_MORE (cont)))
                break;

            Display_Info_Reply (cont, pak, IREP_HASAUTHFLAG);
            mg->webaware = PacketRead2 (pak);
            mm->sex = PacketRead1 (pak);
            mm->age = PacketRead2 (pak);

            cont->updated |= UPF_GENERAL_C;

            UtilUIDisplayMeta (cont);
            PacketRead2 (pak);
            if (subtype == META_SRV_WP_LAST_USER && (wdata = PacketRead2 (pak)))
                rl_printf ("%u %s\n", wdata, i18n (1621, "users not returned."));
            break;
        case META_SRV_RANDOM:
            uin = PacketRead4 (pak);
            event->cont = cont = ContactUIN (serv, uin);
            wdata = PacketRead2 (pak);
            rl_printf (i18n (2606, "Found random chat partner UIN %s in chat group %d.\n"),
                      cont->screen, wdata);
            if (!cont || !CONTACT_DC (cont))
                break;
            if (~cont->updated & UP_INFO)
            {
                if (serv->type == TYPE_SERVER)
                    event->seq = SnacCliMetareqinfo (serv, cont);
            }
            event->callback (event);
            cont->dc->ip_rem  = PacketReadB4 (pak);
            cont->dc->port    = PacketRead4  (pak);
            cont->dc->ip_loc  = PacketReadB4 (pak);
            cont->dc->type    = PacketRead1  (pak);
            cont->dc->version = PacketRead2  (pak);
            /* 14 unknown bytes ignored */
            break;
        case META_SRV_UNKNOWN_270:
            /* ignored - obsoleted as of ICQ 2002 */
            if (!(mo = CONTACT_OBSOLETE (cont)))
                break;

            if ((mo->given = PacketRead1 (pak)))
            {
                mo->unknown = PacketReadB2 (pak);
                s_read (mo->description);
            }
            mo->empty = PacketRead1 (pak);

            cont->updated |= UPF_OBSOLETE;
            event->callback (event);
            break;
        default:
            rl_printf ("%s: %s%04x%s\n",
                     i18n (1945, "Unknown Meta User response"), COLSERVER, subtype, COLNONE);
            rl_print  (s_dump (pak->data + pak->rpos, pak->len - pak->rpos));
            break;
    }
}

void Display_Info_Reply (Contact *cont, Packet *pak, UBYTE flags)
{
    MetaGeneral *mg;

    if (!(mg = CONTACT_GENERAL (cont)))
        return;

    s_read (mg->nick);
    s_read (mg->first);
    s_read (mg->last);
    s_read (mg->email);
    mg->auth = (flags & IREP_HASAUTHFLAG) ? PacketRead1 (pak) : 0;

    cont->updated |= UPF_GENERAL_A;
}

void Display_Ext_Info_Reply (Server *serv, Packet *pak)
{
    Contact *cont;
    MetaGeneral *mg;
    MetaMore *mm;

    if (!(cont = ContactUIN (serv, PacketRead4 (pak))))
        return;

    if (!(mg = CONTACT_GENERAL (cont)) || !(mm = CONTACT_MORE (cont)))
        return;

    s_read (mg->city);
    mg->country = PacketRead2 (pak);
    mg->tz      = PacketRead1 (pak);
    s_read (mg->state);
    mm->age     = PacketRead2 (pak);
    mm->sex     = PacketRead1 (pak);
    s_read (mg->phone);
    s_read (mg->fax);
    s_read (mm->homepage);
    s_read (cont->meta_about);

    cont->updated |= UPF_GENERAL_E;

    UtilUIDisplayMeta (cont);
}

void Recv_Message (Server *serv, Packet *pak)
{
    struct tm stamp;
    Contact *cont;
    strc_t ctext;
    const char *text;
    UDWORD uin;
    UWORD type, len;

    uin            = PacketRead4 (pak);
    stamp.tm_sec   = 0;
    stamp.tm_year  = PacketRead2 (pak) - 1900;
    stamp.tm_mon   = PacketRead1 (pak) - 1;
    stamp.tm_mday  = PacketRead1 (pak);
    stamp.tm_hour  = PacketRead1 (pak);
    stamp.tm_min   = PacketRead1 (pak);
    stamp.tm_isdst = -1;
    type           = PacketRead2 (pak);
    len            = PacketReadAt2 (pak, pak->rpos);
    ctext          = PacketReadL2Str (pak, NULL);

    cont = ContactUIN (serv, uin);
    if (!cont)
        return;

    if (len == ctext->len + 1 && ConvIsUTF8 (ctext->txt))
        text = ConvFrom (ctext, ENC_UTF8)->txt;
    else if (len == ctext->len + 10 || len == strlen (ctext->txt) + 2)
        /* work around bug in Miranda < 0.3.1 */
        text = type == MSG_NORM ? ConvFromCont (ctext, cont) : c_in_to_split (ctext, cont);
    else if (len != ctext->len + 1 && type == MSG_NORM && len & 1)
        text = ConvFrom (ctext, ENC_UCS2BE)->txt;
    else
        text = type == MSG_NORM ? ConvFromCont (ctext, cont) : c_in_to_split (ctext, cont);

    uiG.last_rcvd = cont;
    IMSrvMsg (cont, timegm (&stamp), CV_ORIGIN_v5, type, text);
}
