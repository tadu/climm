/*
 * Handles incoming and creates outgoing SNAC packets
 * for the family 4 (icbm) commands.
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
#include <assert.h>
#include "oldicq_compat.h"
#include "oscar_base.h"
#include "oscar_tlv.h"
#include "oscar_snac.h"
#include "oscar_icbm.h"
#include "packet.h"
#include "contact.h"
#include "connection.h"
#include "conv.h"
#include "im_request.h"
#include "im_response.h"
#include "buildmark.h"
#include "tcp.h"
#include "peer_file.h"
#include "util_ui.h"
#include "util_ssl.h"
#include "preferences.h"

static void SnacCallbackType2Ack (Event *event);
static void SnacCallbackType2 (Event *event);
static void SnacCallbackType2Cancel (Event *event);
static void SnacCallbackIgnore (Event *event);

#define PEEK_REFID 0x3d1db11f

/*
 * SRV_ICBMERR - SNAC(4,1)
 */
JUMP_SNAC_F(SnacSrvIcbmerr)
{
    Connection *serv = event->conn;
    UWORD err = PacketReadB2 (event->pak);

    if ((event->pak->ref == PEEK_REFID) && (err == 0xe || err == 4 || err == 9))
    {
        if (err == 0xe || err == 9)
            rl_print (i18n (2017, "The user is online, but possibly invisible.\n"));
        else
            rl_print (i18n (2564, "The user couldn't be detected to be online.\n"));
        return;
    }

    event = QueueDequeue (serv, QUEUE_TYPE2_RESEND_ACK, event->pak->ref);
    if (event && event->callback)
        event->callback (event);
    else if (err == 4)
        rl_print (i18n (2022, "The user is offline.\n"));
    else if (err != 0xd)
        rl_printf (i18n (2191, "Instant message error: %d.\n"), err);
}

/*
 * CLI_SETICBM - SNAC(4,2)
 */
void SnacCliSeticbm (Connection *serv)
{
   Packet *pak;
   
   pak = SnacC (serv, 4, 2, 0, 0);
   PacketWriteB2 (pak, 0);
   PacketWriteB2 (pak, 0);
   PacketWriteB2 (pak, 3);
   PacketWriteB2 (pak, 8000);
   PacketWriteB2 (pak, 999);
   PacketWriteB2 (pak, 999);
   PacketWriteB2 (pak, 0);
   PacketWriteB2 (pak, 0);
   SnacSend (serv, pak);
}

/*
 * CLI_REQICBM - SNAC(4,4)
 */

/* implemented as macro */

/*
 * SRV_REPLYICBM - SNAC(4,5)
 */
JUMP_SNAC_F(SnacSrvReplyicbm)
{
    SnacCliSeticbm (event->conn);
}

JUMP_SNAC_F(SnacSrvAckmsg)
{
    Connection *serv = event->conn;
    /* UDWORD midtime, midrand; */
    UWORD msgtype, seq_dc;
    Contact *cont;
    Packet *pak;
    strc_t ctext;
    char *text;
    
    pak = event->pak;
    /*midtime*/PacketReadB4 (pak);
    /*midrand*/PacketReadB4 (pak);
              PacketReadB2 (pak);
    cont =    PacketReadCont (pak, serv);
              PacketReadB2 (pak);
              PacketReadData (pak, NULL, PacketRead2 (pak));
              PacketRead2 (pak);
    seq_dc  = PacketRead2 (pak);
              PacketRead4 (pak);
              PacketRead4 (pak);
              PacketRead4 (pak);
    msgtype = PacketRead2 (pak);
              PacketRead2 (pak);
              PacketRead2 (pak);
    ctext   = PacketReadL2Str (pak, NULL);
    
    if (!cont)
        return;
    
    text = strdup (c_in_to_split (ctext, cont));
    if (msgtype == MSG_NORM)
    {
        strc_t cctmp;
        PacketRead4 (pak);
        PacketRead4 (pak);
        cctmp = PacketReadL2Str (pak, NULL);
        if (!strcmp (cctmp->txt, CAP_GID_UTF8))
        {
            free (text);
            text = strdup (ctext->txt);
        }
    }

    event = QueueDequeue (serv, QUEUE_TYPE2_RESEND, seq_dc);

    if ((msgtype & 0x300) == 0x300)
        IMSrvMsg (cont, NOW, CV_ORIGIN_v8, msgtype, text);
    if (event)
    {
        Message *msg = event->data;
        assert (msg);
        assert (msg->cont == cont);
        if ((msg->plain_message || msg->send_message) && !msg->otrinjected && (msgtype & 0x300) != 0x300)
        {
            msg->type = INT_MSGACK_TYPE2;
            IMIntMsgMsg (msg, NOW, ims_offline);
            if ((~cont->oldflags & CONT_SEENAUTO) && strlen (text) && msg->send_message && strcmp (text, msg->send_message))
            {
                IMSrvMsg (cont, NOW, CV_ORIGIN_v8, MSG_AUTO, text);
                cont->oldflags |= CONT_SEENAUTO;
            }
        }
        MsgD (msg);
        event->data = NULL;
        EventD (event);
    }
    free (text);
}

void SnacCallbackIgnore (Event *event)
{
    EventD (event);
}

/*
 * CLI_SENDMSG - SNAC(4,6)
 */
static UBYTE SnacCliSendmsg1 (Connection *serv, Message *msg)
{
    Packet *pak;
    Event *event;
    Contact *cont;
    UDWORD mtime = rand() % 0xffff, mid = rand() % 0xffff;
    int remenc;
    strc_t str;
    int enc = ENC_LATIN1, icqenc = 0, icqcol;

    assert (serv);
    assert (msg);
    cont = msg->cont;
    assert (cont);
    
    remenc = ContactPrefVal (cont, CO_ENCODING);
    
    if (cont->status != ims_offline &&
        HAS_CAP (cont->caps, CAP_UTF8) &&
        !ConvFits (msg->send_message, ENC_ASCII) &&
        !(cont->dc && cont->dc->id1 == (time_t)0xffffff42 &&
          (cont->dc->id2 & 0x7fffffff) < (time_t)0x00040c00)) /* exclude old mICQ */
    {
        enc = ENC_UCS2BE;
        icqenc = 2;
    }
    else
    {
        /* too bad, there's nothing we can do */
        enc = remenc;
        icqenc = 3;
    }
    if (msg->type != 1)
    {
        icqenc = msg->type;
        enc = ENC_LATIN9;
    }

    icqcol = atoi (msg->send_message); /* FIXME FIXME WIXME */
    str = ConvTo (msg->send_message, enc);
    if (str->len > 700)
    {
        msg->maxsize = 700; /* FIXME: not 450? What is the real max? */
        msg->maxenc = enc;
        return RET_DEFER;
    }

    pak = SnacC (serv, 4, 6, 0, 0);
    PacketWriteB4 (pak, mtime);
    PacketWriteB4 (pak, mid);
    PacketWriteB2 (pak, 1 /* format */);
    PacketWriteCont (pak, cont);
    
    PacketWriteTLV     (pak, 2);
    PacketWriteTLV     (pak, 1281);
    if (icqenc == 2)
        PacketWriteB2  (pak, 0x0106);
    else
        PacketWrite1   (pak, 0x01);
    PacketWriteTLVDone (pak);
    PacketWriteTLV     (pak, 257);
    PacketWriteB2      (pak, icqenc);
    PacketWriteB2      (pak, icqcol);
    PacketWriteData    (pak, str->txt, str->len);
    PacketWriteTLVDone (pak);
    PacketWriteTLVDone (pak);
    PacketWriteB2 (pak, 3);
    PacketWriteB2 (pak, 0);
    PacketWriteB2 (pak, 6);
    PacketWriteB2 (pak, 0);

    event = QueueEnqueueData2 (serv, QUEUE_TYPE1_RESEND_ACK, pak->ref, 120, msg, &SnacCallbackIgnore, NULL);
    event->cont = cont;

    SnacSend (serv, pak);

    return RET_OK;
}

static UBYTE SnacCliSendmsg4 (Connection *serv, Message *msg)
{
    Packet *pak;
    Event *event;
    const char *text;
    Contact *cont;
    UDWORD mtime = rand() % 0xffff, mid = rand() % 0xffff;
    
    assert (serv);
    assert (msg);
    cont = msg->cont;
    assert (cont);
    
    text = c_out_to_split (msg->send_message, cont);
    if (strlen (text) > 3 * 1024)
    {
        msg->maxsize = 3 * 1024;
        msg->maxenc = ContactPrefVal (cont, CO_ENCODING);
        return RET_DEFER;
    }
    
    pak = SnacC (serv, 4, 6, 0, 0);
    PacketWriteB4 (pak, mtime);
    PacketWriteB4 (pak, mid);
    PacketWriteB2 (pak, 4 /* format */);
    PacketWriteCont (pak, cont);
    
    PacketWriteTLV     (pak, 5);
    PacketWrite4       (pak, serv->uin);
    PacketWrite1       (pak, msg->type % 256);
    PacketWrite1       (pak, msg->type / 256);
    PacketWriteLNTS    (pak, text);
    PacketWriteTLVDone (pak);
    PacketWriteB2 (pak, 6);
    PacketWriteB2 (pak, 0);

    event = QueueEnqueueData2 (serv, QUEUE_TYPE4_RESEND_ACK, pak->ref, 120, msg, &SnacCallbackIgnore, NULL);
    event->cont = cont;

    SnacSend (serv, pak);

    return RET_OK;
}

static void SnacCallbackType2Cancel2 (Event *event)
{
    assert (event);
    assert (event->conn);
    assert (!event->data);
    EventD (event);
}

static void SnacCallbackType2Ack (Event *event)
{
    Connection *serv = event->conn;
    Contact *cont = event->cont;
    Event *aevent;
    UDWORD opt_ref;
    Message *msg;
    
    if (!OptGetVal (event->opt, CO_REF, &opt_ref))
    {
        EventD (event);
        return;
    }
    aevent = QueueDequeue (serv, QUEUE_TYPE2_RESEND, opt_ref);
    assert (aevent);
    assert (cont);
    ASSERT_SERVER (serv);
    
    msg = aevent->data;
    assert (msg);
    assert (msg->cont == cont);
    aevent->data = NULL;
    EventD (aevent);
    EventD (event);
    IMCliReMsg (cont, msg);
}

static void SnacCallbackType2Cancel (Event *event)
{
    Message *msg = event->data;
    if (msg->send_message && !msg->otrinjected)
        rl_printf (i18n (2234, "Message %s discarded - lost session.\n"),
            msg->plain_message ? msg->plain_message : msg->send_message);
    MsgD (event->data);
    event->data = NULL;
    EventD (event);
}

static void SnacCallbackType2 (Event *event)
{
    Connection *serv = event->conn;
    Contact *cont = event->cont;
    Packet *pak = event->pak;
    Message *msg = event->data;
    Event *event2;

    ASSERT_SERVER (serv);
    assert (pak);
    assert (event->cont);
    assert (event->data);
    assert (!event->opt);

    if (event->attempts < MAX_RETRY_TYPE2_ATTEMPTS && serv->connect & CONNECT_MASK)
    {
        if (serv->connect & CONNECT_OK)
        {
            if (event->attempts > 1)
                IMIntMsg (cont, NOW, ims_offline, INT_MSGTRY_TYPE2, msg->plain_message ? msg->plain_message : msg->send_message);
            SnacSend (serv, PacketClone (pak));
            event->attempts++;
            /* allow more time for the peer's ack than the server's ack */
            event->due = time (NULL) + RETRY_DELAY_TYPE2 + 5;

            event2 = QueueEnqueueData2 (serv, QUEUE_TYPE2_RESEND_ACK, pak->ref, RETRY_DELAY_TYPE2, NULL, &SnacCallbackType2Ack, &SnacCallbackType2Cancel2);
            event2->cont = cont;
            event2->opt = OptSetVals (NULL, CO_REF, event->seq, 0);
        }
        else
            event->due = time (NULL) + 1;
        QueueEnqueue (event);
        return;
    }
    
    event->data = NULL;
    IMCliReMsg (cont, msg);
    EventD (event);
}

static UBYTE SnacCliSendmsg2 (Connection *serv, Message *msg)
{
    Packet *pak;
    UDWORD mtime = rand() % 0xffff, mid = rand() % 0xffff;
    BOOL peek = 0;
    Contact *cont;
    const char *text;
    
    assert (serv);
    assert (msg);
    cont = msg->cont;
    assert (cont);
    
    if (msg->type == MSG_GET_PEEK)
    {
        peek = 1;
        msg->type = MSG_GET_AWAY | MSGF_GETAUTO;
    }

    text = c_out_for (msg->send_message, cont, msg->type);
    if (strlen (text) > 3 * 1024)
    {
        msg->maxsize = 3 * 1024;
        msg->maxenc = CONT_UTF8 (cont, msg->type) ? ENC_UTF8 : ContactPrefVal (cont, CO_ENCODING);
        return RET_DEFER;
    }
    
    serv->our_seq_dc--;
    msg->trans &= ~CV_MSGTRANS_TYPE2;
    
    pak = SnacC (serv, 4, 6, 0, peek ? PEEK_REFID : 0);
    PacketWriteB4 (pak, mtime);
    PacketWriteB4 (pak, mid);
    PacketWriteB2 (pak, 2);
    PacketWriteCont (pak, cont);
    
    PacketWriteTLV     (pak, 5);
     PacketWrite2       (pak, 0);
     PacketWriteB4      (pak, mtime);
     PacketWriteB4      (pak, mid);

     PacketWriteCapID   (pak, peek ? CAP_NONE : CAP_SRVRELAY);
     PacketWriteTLV2    (pak, 10, 1);
     PacketWriteB4      (pak, 0x000f0000); /* empty TLV(15) */
     if (!peek) {
     PacketWriteTLV     (pak, 10001);
      PacketWriteLen     (pak);
       PacketWrite2       (pak, serv->assoc && serv->assoc->connect & CONNECT_OK
                              ? serv->assoc->version : 8);
       PacketWriteCapID   (pak, CAP_NONE);
       PacketWrite2       (pak, 0);
       PacketWrite4       (pak, 3);
       PacketWrite1       (pak, 0);
       PacketWrite2       (pak, peek ? 0 : serv->our_seq_dc);
      PacketWriteLenDone (pak);
      if (peek)
      {
          PacketWriteLen    (pak);
           PacketWrite2      (pak, serv->our_seq_dc);       /* sequence number */
           PacketWrite4      (pak, 0);
           PacketWrite4      (pak, 0);
           PacketWrite4      (pak, 0);
          PacketWriteLenDone (pak);
          PacketWrite2      (pak, MSG_GET_DND | MSGF_GETAUTO);    /* message type    */
          PacketWrite2      (pak, 0);     /* status          */
          PacketWrite2      (pak, 0);      /* flags           */
          PacketWrite2      (pak, 0x7fff);
          PacketWrite1      (pak, 0);
      }
      else
      {
          SrvMsgAdvanced     (pak, serv->our_seq_dc, msg->type, serv->status, cont->status, -1, text);
          PacketWrite4       (pak, TCP_COL_FG);
          PacketWrite4       (pak, TCP_COL_BG);
          if (CONT_UTF8 (cont, msg->type))
              PacketWriteDLStr     (pak, CAP_GID_UTF8);
      }
     PacketWriteTLVDone (pak);
     }
    PacketWriteTLVDone (pak);
    
    if (peek)
    {    
        PacketWriteB4  (pak, 0x00060000);
        SnacSend (serv, pak);
        MsgD (msg);
    }
    else
    {
        Event *event;

        PacketWriteB4      (pak, 0x00030000); /* empty TLV(3) */
        event = QueueEnqueueData2 (serv, QUEUE_TYPE2_RESEND, serv->our_seq_dc, 0, msg, &SnacCallbackType2, &SnacCallbackType2Cancel);
        event->cont = cont;
        event->pak = pak;
    }
    return RET_INPR;
}


/*
 * CLI_SENDMSG - SNAC(4,6) - all
 */
UBYTE SnacCliSendmsg (Connection *serv, Contact *cont, UBYTE format, Message *msg)
{
    assert (serv);
    assert (cont);
    assert (msg);
    assert (msg->cont == cont);
    
    if (format == 2 && !msg->force)
    {
        switch (msg->type & 0xff)
        {
            case MSG_AUTO:
            case MSG_AUTH_REQ:
            case MSG_AUTH_GRANT:
            case MSG_AUTH_DENY:
            case MSG_AUTH_ADDED:
                return RET_DEFER;
            case MSG_GET_PEEK:
            case MSG_GET_VER:
                break;
            default:
                if (!HAS_CAP (cont->caps, CAP_SRVRELAY) || !HAS_CAP (cont->caps, CAP_ISICQ))
                    return RET_DEFER;
        }
    }
    else if (format == 1 && !msg->force)
    {
        if ((msg->type & 0xff) != MSG_NORM)
            return RET_DEFER;
    }
    else if (format == 4 && !msg->force)
    {
        switch (msg->type & 0xff)
        {
            case MSG_AUTO:
            case MSG_URL:
            case MSG_AUTH_REQ:
            case MSG_AUTH_GRANT:
            case MSG_AUTH_DENY:
            case MSG_AUTH_ADDED:
                if (!cont->uin)
                    return RET_DEFER;
                break;
            case MSG_NORM:
            default:
            case MSG_GET_AWAY:
            case MSG_GET_DND:
            case MSG_GET_OCC:
            case MSG_GET_FFC:
            case MSG_GET_NA:
            case MSG_GET_VER:
            case MSG_GET_PEEK:
                return RET_DEFER;
        }
    }
    else if (!format || format == 0xff)
    {
        switch (msg->type & 0xff)
        {
            case MSG_AUTO:
            case MSG_URL:
            case MSG_AUTH_REQ:
            case MSG_AUTH_GRANT:
            case MSG_AUTH_DENY:
            case MSG_AUTH_ADDED:
                if (!cont->uin)
                    return RET_DEFER;
                format = 4;
                break;
            case MSG_NORM:
                format = 1;
                break;
            default:
            case MSG_GET_AWAY:
            case MSG_GET_DND:
            case MSG_GET_OCC:
            case MSG_GET_FFC:
            case MSG_GET_NA:
            case MSG_GET_VER:
            case MSG_GET_PEEK:
                format = 2;
        }
    }
    
    if (format == 1)
        return SnacCliSendmsg1 (serv, msg);
    else if (format == 4)
        return SnacCliSendmsg4 (serv, msg);
    else if (format == 2)
        return SnacCliSendmsg2 (serv, msg);
    return RET_DEFER;
}

static void SnacSrvCallbackSendack (Event *event)
{
    if (event && event->conn && event->pak && event->conn->type == TYPE_SERVER)
    {
        SnacSend (event->conn, event->pak);
        event->pak = NULL;
    }
    EventD (event);
}

/*
 * SRV_RECVMSG - SNAC(4,7)
 */
JUMP_SNAC_F(SnacSrvRecvmsg)
{
    Connection *serv = event->conn;
    Contact *cont;
    Event *newevent;
    Cap *cap1, *cap2;
    Packet *p = NULL, *pp = NULL, *pak;
    TLV *tlv, *tlvs;
    Opt *opt;
    UDWORD midtim, midrnd, midtime, midrand, unk, tmp, type1enc, tlvc;
    UWORD seq1, tcpver, len, i, msgtyp, type;
    const char *txt = NULL;
    strc_t ctext;
    str_s str = { NULL, 0, 0 };

    pak = event->pak;

    midtime = PacketReadB4 (pak);
    midrand = PacketReadB4 (pak);
    type    = PacketReadB2 (pak);
    cont    = PacketReadCont (pak, serv);
              PacketReadB2 (pak); /* WARNING */
    tlvc    = PacketReadB2 (pak); /* COUNT */
    
    if (!cont)
        return;

    tlv = TLVRead (pak, PacketReadLeft (pak), tlvc);

#ifdef WIP
    if (tlv[6].str.len && tlv[6].nr != cont->nativestatus)
        rl_printf ("FIXMEWIP: status for %s embedded in message 0x%08lx different from server status 0x%08lx.\n", cont->screen, UD2UL (tlv[6].nr), UD2UL (cont->nativestatus));
#endif

    if (tlv[6].str.len)
        event->opt = OptSetVals (event->opt, CO_STATUS, tlv[6].nr, 0);

    TLVD (tlv);
    tlv = TLVRead (pak, PacketReadLeft (pak), -1);

    if (type == 1 && tlv[2].str.len)
        tlvs = &tlv[2];
    else if (type == 2 && tlv[5].str.len)
        tlvs = &tlv[5];
    else if (type == 4 && tlv[5].str.len)
        tlvs = &tlv[5];
    else
    {
        SnacSrvUnknown (event);
        TLVD (tlv);
        return;
    }
    p = PacketCreate (&tlvs->str);
    TLVD (tlv);
    
    switch (type)
    {
        case 1:
            PacketReadB2 (p);
            PacketReadData (p, NULL, PacketReadB2 (p));
                       PacketReadB2 (p);
            len      = PacketReadB2 (p);
            
            type1enc = PacketReadB4 (p);
            if (len < 4)
            {
                SnacSrvUnknown (event);
                PacketD (p);
                return;
            }
            PacketReadData (p, &str, len - 4);
            PacketD (p);
            /* TLV 1, 2(!), 3, 4, f ignored */
            switch (type1enc & 0xf0000)
            {
                case 0x00020000:
                    txt = ConvFrom (&str, ENC_UCS2BE)->txt;
                    break;
                case 0x00030000:
                    txt = ConvFromCont (&str, cont);
                    break;
                case 0x00000000:
                    if (ConvIsUTF8 (str.txt) && len == strlen (str.txt) + 4)
                        txt = ConvFrom (&str, ENC_UTF8)->txt;
                    else
                        txt = ConvFromCont (&str, cont);
                    break;
                default:
                    SnacSrvUnknown (event);
                    txt = ConvFromCont (&str, cont);
                    break;
            }
            opt = OptSetVals (event->opt, CO_ORIGIN, CV_ORIGIN_v5, CO_MSGTYPE, MSG_NORM, CO_MSGTEXT, txt, 0);
            event->opt = NULL;
            IMSrvMsgFat (cont, NOW, opt);
            Auto_Reply (serv, cont);
            s_done (&str);
            break;
        case 2:
            type   = PacketReadB2 (p);
            midtim = PacketReadB4 (p);
            midrnd = PacketReadB4 (p);
            cap1   = PacketReadCap (p);
            
            ContactSetCap (cont, cap1);
            if (midtim != midtime || midrnd != midrand)
            {
                SnacSrvUnknown (event);
                PacketD (p);
                return;
            }

            tlv = TLVRead (p, PacketReadLeft (p), -1);
            PacketD (p);
            
            if ((i = TLVGet (tlv, 0x2711)) == (UWORD)-1)
            {
                if (TLVGet (tlv, 11) == (UWORD)-1)
                    SnacSrvUnknown (event);
#ifdef WIP
                else
                {
                    rl_log_for (cont->nick, COLCONTACT);
                    rl_printf ("FIXMEWIP: tlv(b)-only packet.\n");
                }
#endif
                TLVD (tlv);
                return;
            }
            
            switch (cap1->id)
            {
                case CAP_ISICQ:
                    if (tlv[i].str.len != 0x1b)
                    {
                        SnacSrvUnknown (event);
                        TLVD (tlv);
                        return;
                    }
                    pp = PacketCreate (&tlv[i].str);
                    {
                        UDWORD suin = PacketRead4  (pp);
                        UDWORD sip  = PacketReadB4 (pp);
                        UDWORD sp1  = PacketRead4  (pp);
                        UBYTE  scon = PacketRead1  (pp);
#ifdef WIP
                        UDWORD sop  = PacketRead4  (pp);
                        UDWORD sp2  = PacketRead4  (pp);
                        UWORD  sver = PacketRead2  (pp);
                        UDWORD sunk = PacketRead4  (pp);
#else
                        UWORD sver;
                               PacketRead4  (pp);
                               PacketRead4  (pp);
                        sver = PacketRead2  (pp);
                               PacketRead4  (pp);
#endif
                        if (suin != cont->uin)
                        {
                            SnacSrvUnknown (event);
                            TLVD (tlv);
                            return;
                        }
                        
#ifdef WIP
                        rl_log_for (cont->nick, COLCONTACT);
                        rl_printf ("FIXMEWIP: updates dc to %s:%ld|%ld|%ld v%d %d seq %ld\n",
                                  s_ip (sip), UD2UL (sp1), UD2UL (sp2), UD2UL (sop), sver, scon, UD2UL (sunk));
#endif
                        CONTACT_DC (cont)->ip_rem = sip;
                        cont->dc->port = sp1;
                        cont->dc->type = scon;
                        cont->dc->version = sver;
                    }
                    PacketD (pp);
                    TLVD (tlv);
                    return;

                case CAP_SRVRELAY:
                    if (tlv[i].str.txt[0] != 0x1b)
                    {
                        SnacSrvUnknown (event);
                        TLVD (tlv);
                        return;
                    }
                    pp = PacketCreate (&tlv[i].str);

                    p = SnacC (serv, 4, 11, 0, 0);
                    PacketWriteB4 (p, midtim);
                    PacketWriteB4 (p, midrnd);
                    PacketWriteB2 (p, 2);
                    PacketWriteCont (p, cont);
                    PacketWriteB2 (p, 3);

                    len    = PacketRead2 (pp);      PacketWrite2 (p, len);
                    tcpver = PacketRead2 (pp);      PacketWrite2 (p, tcpver);
                    cap2   = PacketReadCap (pp);    PacketWriteCap (p, cap2);
                    tmp    = PacketRead2 (pp);      PacketWrite2 (p, tmp);
                    tmp    = PacketRead4 (pp);      PacketWrite4 (p, tmp);
                    tmp    = PacketRead1 (pp);      PacketWrite1 (p, tmp);
                    seq1   = PacketRead2 (pp);      PacketWrite2 (p, seq1);

                    ContactSetCap (cont, cap2);
                    ContactSetVersion (cont);
                    
                    if (cap2->id != CAP_STR_2001 && cap2->id != CAP_STR_2002)
                    {
                        event->opt = OptSetVals (event->opt, CO_ORIGIN, CV_ORIGIN_v8, 0);
                        event->cont = cont;
                        newevent = QueueEnqueueData (serv, QUEUE_ACKNOWLEDGE, seq1,
                                     (time_t)-1, p, cont, NULL, &SnacSrvCallbackSendack);
                        SrvReceiveAdvanced (serv, event, pp, newevent);
                    }
                    else
                        PacketD (p);
                    PacketD (pp);
                    TLVD (tlv);
                    return;

                default:
                    SnacSrvUnknown (event);
                    TLVD (tlv);
                    return;
            }
            /* TLV 1, 2(!), 3, 4, f ignored */
            break;
        case 4:
            unk    = PacketRead4 (p);
            msgtyp = PacketRead2 (p);
            if (unk != cont->uin)
            {
                PacketD (p);
                SnacSrvUnknown (event);
                return;
            }
            ctext = PacketReadL2Str (p, NULL);
            PacketD (p);
            /* FOREGROUND / BACKGROUND ignored */
            /* TLV 1, 2(!), 3, 4, f ignored */

            opt = OptSetVals (event->opt, CO_ORIGIN, CV_ORIGIN_v5, CO_MSGTYPE, msgtyp,
                      CO_MSGTEXT, msgtyp == MSG_NORM ? ConvFromCont (ctext, cont) : c_in_to_split (ctext, cont), 0);
            event->opt = NULL;
            IMSrvMsgFat (cont, NOW, opt);
            Auto_Reply (serv, cont);
            break;
    }
}

/*
 * SRV_ACKMSG - SNAC(4,C)
 */
JUMP_SNAC_F(SnacSrvSrvackmsg)
{
    Connection *serv = event->conn;
    Event *event2 = NULL;
    Packet *pak;
    Message *msg;
    Contact *cont;
    /* UDWORD mid1, mid2; */
    UWORD format;

    pak = event->pak;

    /*mid1=*/PacketReadB4 (pak);
    /*mid2=*/PacketReadB4 (pak);
    format = PacketReadB2 (pak);

    cont = PacketReadCont (pak, serv);
    
    if (!cont)
        return;
    
    
    switch (format)
    {
        case 1:
            event2 = QueueDequeue (serv, QUEUE_TYPE1_RESEND_ACK, pak->ref);
            if (!event2 || !(msg = event2->data))
                break;
            msg->type = INT_MSGACK_V8;
            if ((msg->send_message || msg->plain_message) && !msg->otrinjected)
                IMIntMsgMsg (msg, NOW, ims_offline);
            break;
        case 4:
            event2 = QueueDequeue (serv, QUEUE_TYPE4_RESEND_ACK, pak->ref);
            if (!event2 || !(msg = event2->data))
                break;
            msg->type = INT_MSGACK_V8;
            if ((msg->send_message || msg->plain_message) && !msg->otrinjected)
                IMIntMsgMsg (msg, NOW, ims_offline);
            break;
        case 2: /* msg was received by server */
            event2 = QueueDequeue (serv, QUEUE_TYPE2_RESEND_ACK, pak->ref);
            if (!event2 || !(msg = event2->data))
                break;
            if (pak->ref == PEEK_REFID)
                rl_print (i18n (2573, "The user is probably offline.\n"));
            break;
    }
    if (event2)
    {
        MsgD (event2->data);
        event2->data = NULL;
        EventD (event2);
    }
}

void SrvMsgAdvanced (Packet *pak, UDWORD seq, UWORD msgtype, status_t status,
                     status_t deststatus, UWORD flags, const char *msg)
{
    if (msgtype == MSG_SSL_OPEN)
        status = ims_online;
    
    if (flags == (UWORD)-1)
    {
        switch (ContactClearInv (deststatus))
        {
            case imr_offline: /* keep */ break;
            case imr_dnd:
            case imr_occ:     flags = TCP_MSGF_CLIST; break;
            case imr_na:
            case imr_away:    flags = TCP_MSGF_1; break;
            case imr_ffc:
            case imr_online:  flags = TCP_MSGF_LIST | TCP_MSGF_1; break;
        }
    }

    PacketWriteLen    (pak);
     PacketWrite2      (pak, seq);       /* sequence number */
     PacketWrite4      (pak, 0);
     PacketWrite4      (pak, 0);
     PacketWrite4      (pak, 0);
    PacketWriteLenDone (pak);
    PacketWrite2      (pak, msgtype);    /* message type    */
    PacketWrite2      (pak, IcqFromStatus (status));
                                         /* status          */
    PacketWrite2      (pak, flags);      /* flags           */
    PacketWriteLNTS   (pak, msg);        /* the message     */
}

/*
 * Append the "geeting" part to an advanced message packet.
 */
void SrvMsgGreet (Packet *pak, UWORD cmd, const char *reason, UWORD port, UDWORD len, const char *msg)
{
    PacketWrite2     (pak, cmd);
    switch (cmd)
    {
        case 0x2d:
        default:
            PacketWriteB4  (pak, 0xbff720b2);
            PacketWriteB4  (pak, 0x378ed411);
            PacketWriteB4  (pak, 0xbd280004);
            PacketWriteB4  (pak, 0xac96d905);
            break;
        case 0x29:
        case 0x32:
            PacketWriteB4  (pak, 0xf02d12d9);
            PacketWriteB4  (pak, 0x3091d311);
            PacketWriteB4  (pak, 0x8dd70010);
            PacketWriteB4  (pak, 0x4b06462e);
    }
    PacketWrite2     (pak, 0);
    switch (cmd)
    {
        case 0x29:  PacketWriteDLStr (pak, "File");          break;
        case 0x2d:  PacketWriteDLStr (pak, "ICQ Chat");      break;
        case 0x32:  PacketWriteDLStr (pak, "File Transfer"); break;
        default:    PacketWriteDLStr (pak, "");
    }
    PacketWrite2     (pak, 0);
    PacketWrite1     (pak, 1);
    switch (cmd)
    {
        case 0x29:
        case 0x2d:  PacketWriteB4    (pak, 0x00000100);      break;
        case 0x32:  PacketWriteB4    (pak, 0x01000000);      break;
        default:    PacketWriteB4    (pak, 0);
    }
    PacketWriteB4    (pak, 0);
    PacketWriteB4    (pak, 0);
    PacketWriteLen4  (pak);
    PacketWriteDLStr (pak, c_out (reason));
    PacketWriteB2    (pak, port);
    PacketWriteB2    (pak, 0);
    PacketWriteLNTS  (pak, c_out (msg));
    PacketWrite4     (pak, len);
    if (cmd != 0x2d)
        PacketWrite4     (pak, port);
    PacketWriteLen4Done (pak);
}

/*
 * Process an advanced message.
 *
 * Note: swallows only acknowledge event/packet;
 * the caller destructs inc_event after return.
 */
void SrvReceiveAdvanced (Connection *serv, Event *inc_event, Packet *inc_pak, Event *ack_event)
{
#ifdef ENABLE_PEER2PEER
    Connection *flist;
    Event *e1;
    UDWORD opt_acc;
#endif
    Contact *cont = inc_event->cont;
    Opt *opt = inc_event->opt, *opt2;
    Packet *ack_pak = ack_event->pak;
    const char *txt, *ack_msg = "", *tauto;
    strc_t text, cname, ctext, reason, cctmp;
    char *name;
    UDWORD tmp, cmd, flen;
    UWORD unk, seq, msgtype, unk2, pri;
    UWORD ack_flags, ack_status, accept;

    unk     = PacketRead2    (inc_pak);  PacketWrite2 (ack_pak, unk);
    seq     = PacketRead2    (inc_pak);  PacketWrite2 (ack_pak, seq);
    tmp     = PacketRead4    (inc_pak);  PacketWrite4 (ack_pak, tmp);
    tmp     = PacketRead4    (inc_pak);  PacketWrite4 (ack_pak, tmp);
    tmp     = PacketRead4    (inc_pak);  PacketWrite4 (ack_pak, tmp);
    msgtype = PacketRead2    (inc_pak);  PacketWrite2 (ack_pak, msgtype);

    unk2    = PacketRead2    (inc_pak);
    pri     = PacketRead2    (inc_pak);
    text    = PacketReadL2Str (inc_pak, NULL);
    
#ifdef WIP
    if (prG->verbose)
    rl_printf ("FIXMEWIP: Starting advanced message: events %p, %p; type %d, seq %x.\n",
              inc_event, ack_event, msgtype, seq);
#endif
 
    OptSetVal (opt, CO_MSGTYPE, msgtype);
    OptSetStr (opt, CO_MSGTEXT, msgtype == MSG_NORM ? ConvFromCont (text, cont) : c_in_to_split (text, cont));
    
    accept = FALSE;

/*
 * Don't refuse until we have sensible preferences for that
 */
    switch (ContactClearInv (serv->status))
    {
        case imr_dnd:  ack_msg = (tauto = ContactPrefStr (cont, CO_TAUTODND))  && *tauto ?
                       tauto : ContactPrefStr (cont, CO_AUTODND);  ack_status = TCP_ACK_ONLINE; break;
        case imr_occ:  ack_msg = (tauto = ContactPrefStr (cont, CO_TAUTOOCC))  && *tauto ?
                       tauto : ContactPrefStr (cont, CO_AUTOOCC);  ack_status = TCP_ACK_ONLINE; break;
        case imr_na:   ack_msg = (tauto = ContactPrefStr (cont, CO_TAUTONA))   && *tauto ?
                       tauto : ContactPrefStr (cont, CO_AUTONA);   ack_status = TCP_ACK_NA;     break;
        case imr_away: ack_msg = (tauto = ContactPrefStr (cont, CO_TAUTOAWAY)) && *tauto ?
                       tauto : ContactPrefStr (cont, CO_AUTOAWAY); ack_status = TCP_ACK_AWAY;   break;
        default:       ack_status  = TCP_ACK_ONLINE;
    }

    ack_flags = 0;
    if (ContactIsInv (serv->status))  ack_flags |= TCP_MSGF_INV;

    switch (msgtype & ~MSGF_MASS)
    {
        /* Requests for auto-response message */
        do  {
                val_t val;

        case MSGF_GETAUTO | MSG_GET_AWAY:
            ack_msg = (tauto = ContactPrefStr (cont, CO_TAUTOAWAY)) && *tauto ? tauto : ContactPrefStr (cont, CO_AUTOAWAY);
            break;
        case MSGF_GETAUTO | MSG_GET_OCC:
            ack_msg = (tauto = ContactPrefStr (cont, CO_TAUTOOCC))  && *tauto ? tauto : ContactPrefStr (cont, CO_AUTOOCC);
            break;
        case MSGF_GETAUTO | MSG_GET_NA:
            ack_msg = (tauto = ContactPrefStr (cont, CO_TAUTONA))   && *tauto ? tauto : ContactPrefStr (cont, CO_AUTONA);
            break;
        case MSGF_GETAUTO | MSG_GET_DND:
            ack_msg = (tauto = ContactPrefStr (cont, CO_TAUTODND))  && *tauto ? tauto : ContactPrefStr (cont, CO_AUTODND);
            break;
        case MSGF_GETAUTO | MSG_GET_FFC:   ack_msg = ContactPrefStr (cont, CO_AUTOFFC);  break;
        case MSGF_GETAUTO | MSG_GET_VER:
                if (!OptGetVal (&prG->copts, CO_ENCODING, &val))
                    val = -1;
                ack_msg = s_sprintf ("%s\nLocale: %s %s %s %s %d %s", BuildVersionText,
                                     prG->locale, prG->locale_orig, prG->locale_full,
                                     ConvEncName (prG->enc_loc), prG->locale_broken, ConvEncName (val));
            } while (0);

#ifdef WIP
            if (1)
#else
            if (msgtype != 1012)
#endif
            {    
                if (((ack_flags & TCP_MSGF_INV) && !ContactPrefVal (cont, CO_INTIMATE)) || ContactPrefVal (cont, CO_HIDEFROM))
                {
                    if (ContactPrefVal (cont, CO_SHOWCHANGE))
                    {
                        rl_log_for (cont->nick, COLCONTACT);
                        rl_printf (i18n (2568, "Ignored request for auto-response from %s%s%s.\n"),
                                   COLCONTACT, cont->nick, COLNONE);
                    }
                    ack_event->due = 0;
                    ack_event->callback = NULL;
                    QueueDequeueEvent (ack_event);
                    EventD (ack_event);
                    return;
                }
                if (ContactPrefVal (cont, CO_SHOWCHANGE))
                {
                    rl_log_for (cont->nick, COLCONTACT);
                    rl_printf (i18n (1814, "Sent auto-response message to %s%s%s.\n"),
                               COLCONTACT, cont->nick, COLNONE);
                }
            }

            accept = TRUE;
            ack_flags = pri;
            ack_status = 0;
            break;

        case MSG_FILE:
#ifdef ENABLE_PEER2PEER
            cmd     = PacketRead4 (inc_pak);
            cname   = PacketReadL2Str (inc_pak, NULL);
            flen    = PacketRead4 (inc_pak);
            msgtype = PacketRead4 (inc_pak);
            
            name = strdup (ConvFromCont (cname, cont));
            
            flist = PeerFileCreate (serv);
            OptSetVal (opt, CO_FILEACCEPT, 0);
            OptSetVal (opt, CO_BYTES, flen);
            OptSetStr (opt, CO_MSGTEXT, name);
            OptSetVal (opt, CO_REF, ack_event->seq);
            if (!OptGetVal (opt, CO_FILEACCEPT, &opt_acc) && flen)
            {
                opt2 = OptC ();
                OptSetVal (opt2, CO_BYTES, flen);
                OptSetStr (opt2, CO_MSGTEXT, name);
                OptSetVal (opt2, CO_REF, ack_event->seq);
                OptSetVal (opt2, CO_MSGTYPE, msgtype);
                IMSrvMsgFat (cont, NOW, opt2);
                opt2 = OptC ();
                OptSetVal (opt2, CO_FILEACCEPT, 0);
                OptSetStr (opt2, CO_REFUSE, i18n (2514, "refused (ignored)"));
                e1 = QueueEnqueueData (serv, QUEUE_USERFILEACK, ack_event->seq, time (NULL) + 120,
                                       NULL, inc_event->cont, opt2, &PeerFileTO);
                QueueEnqueueDep (inc_event->conn, inc_event->type, ack_event->seq, e1,
                                 inc_event->pak, inc_event->cont, opt, inc_event->callback);
                inc_event->pak->rpos = inc_event->pak->tpos;
                inc_event->opt = NULL;
                inc_event->pak = NULL;
                ack_event->due = 0;
                ack_event->callback = NULL;
                QueueDequeueEvent (ack_event);
                EventD (ack_event);
                free (name);
#ifdef WIP
                rl_printf ("FIXMEWIP: Delaying advanced message: events %p, %p.\n", inc_event, ack_event);
#endif
                return;
            }
            free (name);
            if (serv->assoc && PeerFileIncAccept (serv->assoc, inc_event))
            {
                PacketWrite2    (ack_pak, ack_status);
                PacketWrite2    (ack_pak, ack_flags);
                PacketWriteLNTS (ack_pak, c_out (ack_msg));
                PacketWriteB2   (ack_pak, flist->port);
                PacketWrite2    (ack_pak, 0);
                PacketWriteStr  (ack_pak, "");
                PacketWrite4    (ack_pak, 0);
                if (serv->assoc->version > 6)
                    PacketWrite4 (ack_pak, 0x20726f66);
                PacketWrite4    (ack_pak, flist->port);
            }
            else
#endif
            {
                if (!OptGetStr (inc_event->wait->opt, CO_REFUSE, &txt))
                    txt = "";
                PacketWrite2    (ack_pak, TCP_ACK_REFUSE);
                PacketWrite2    (ack_pak, ack_flags);
                PacketWriteLNTS (ack_pak, c_out (ack_msg));
                PacketWriteB2   (ack_pak, 0);
                PacketWrite2    (ack_pak, 0);
                PacketWriteStr  (ack_pak, txt);
                PacketWrite4    (ack_pak, 0);
                if (serv->assoc->version > 6)
                    PacketWrite4 (ack_pak, 0x20726f66);
                PacketWrite4    (ack_pak, 0);
            }
            EventD (QueueDequeueEvent (inc_event->wait));
            inc_event->wait = NULL;
            accept = -1;
            break;
        case MSG_EXTENDED:
            {
                /* UWORD port, port2, pad; */
                char *gtext;
                char id[20];
                cmd    = PacketRead2 (inc_pak);
                {
                    str_s t = { 0, 0, 0 };
                    s_init (&t, "", 20);
                         PacketReadData (inc_pak, &t, 16);
                    memcpy (id, t.txt, 16);
                    id[17] = 0;
                    s_done (&t);
                }
                         PacketRead2 (inc_pak);
                ctext  = PacketReadL4Str (inc_pak, NULL);
                         PacketReadData (inc_pak, NULL, 15);
                         PacketRead4 (inc_pak);
                reason = PacketReadL4Str (inc_pak, NULL);
                /*port=*/PacketReadB2 (inc_pak);
                /*pad=*/ PacketRead2 (inc_pak);
                cname  = PacketReadL2Str (inc_pak, NULL);
                flen   = PacketRead4 (inc_pak);
                /*port2*/PacketRead4 (inc_pak);
                
                gtext = strdup (ConvFromCont (ctext, cont));
                name = strdup (ConvFromCont (cname, cont));
                
                switch (cmd)
                {
                    case 0x0029:
#ifdef ENABLE_PEER2PEER
                        flist = PeerFileCreate (serv);
                        OptSetVal (opt, CO_FILEACCEPT, 0);
                        OptSetVal (opt, CO_BYTES, flen);
                        OptSetStr (opt, CO_MSGTEXT, name);
                        OptSetVal (opt, CO_REF, ack_event->seq);
                        if (!inc_event->wait)
                        {
                            opt2 = OptC ();
                            OptSetVal (opt2, CO_BYTES, flen);
                            OptSetStr (opt2, CO_MSGTEXT, name);
                            OptSetVal (opt2, CO_REF, ack_event->seq);
                            OptSetVal (opt2, CO_MSGTYPE, MSG_FILE);
                            IMSrvMsgFat (cont, NOW, opt2);
                            opt2 = OptC ();
                            OptSetVal (opt2, CO_FILEACCEPT, 0);
                            OptSetStr (opt2, CO_REFUSE, i18n (2514, "refused (ignored)"));
                            e1 = QueueEnqueueData (serv, QUEUE_USERFILEACK, ack_event->seq, time (NULL) + 120,
                                                   NULL, inc_event->cont, opt2, &PeerFileTO);
                            QueueEnqueueDep (inc_event->conn, inc_event->type, ack_event->seq, e1,
                                             inc_event->pak, inc_event->cont, opt, inc_event->callback);
                            inc_event->pak->rpos = inc_event->pak->tpos;
                            inc_event->opt = NULL;
                            inc_event->pak = NULL;
                            ack_event->callback = NULL;
                            ack_event->due = 0;
                            QueueDequeueEvent (ack_event);
                            EventD (ack_event);
                            free (name);
                            free (gtext);
#ifdef WIP
                            rl_printf ("FIXMEWIP: Delaying advanced message: events %p, %p.\n", inc_event, ack_event);
#endif
                            return;
                        }
                        if (serv->assoc && PeerFileIncAccept (serv->assoc, inc_event))
                        {
                            PacketWrite2    (ack_pak, ack_status);
                            PacketWrite2    (ack_pak, ack_flags);
                            PacketWriteLNTS (ack_pak, "");
                            SrvMsgGreet     (ack_pak, cmd, "", flist->port, 0, "");
                        }
                        else
#endif
                        {
                            if (!OptGetStr (inc_event->wait->opt, CO_REFUSE, &txt))
                                txt = "";
                            PacketWrite2    (ack_pak, TCP_ACK_REFUSE);
                            PacketWrite2    (ack_pak, ack_flags);
                            PacketWriteLNTS (ack_pak, txt);
                            SrvMsgGreet     (ack_pak, cmd, "", 0, 0, "");
                        }
                        EventD (QueueDequeueEvent (inc_event->wait));
                        inc_event->wait = NULL;
                        break;

                    case 0x002d:
                        if (id[0] == (char)0xbf)
                        {
                            IMSrvMsg (cont, NOW, 0, msgtype, c_in_to_split (text, cont));
                            IMSrvMsg (cont, NOW, 0, MSG_CHAT, name);
                            opt2 = OptC ();
                            OptSetVal (opt2, CO_MSGTYPE, MSG_CHAT);
                            OptSetStr (opt2, CO_MSGTEXT, reason->txt);
                            PacketWrite2    (ack_pak, TCP_ACK_REFUSE);
                            PacketWrite2    (ack_pak, ack_flags);
                            PacketWriteLNTS (ack_pak, "");
                            SrvMsgGreet     (ack_pak, cmd, "", 0, 0, "");
                            break;
                        }
                        else if (id[0] == (char)0x2a)
                        {
                            IMSrvMsg (cont, NOW, 0, MSG_CONTACT, c_in_to_split (reason, cont));
                            PacketWrite2    (ack_pak, ack_status);
                            PacketWrite2    (ack_pak, ack_flags);
                            PacketWriteLNTS (ack_pak, "");
                            SrvMsgGreet     (ack_pak, cmd, "", 0, 0, "");
                            break;
                        }

                    case 0x0032:
                    default:
                        if (prG->verbose & DEB_PROTOCOL)
                            rl_printf (i18n (2065, "Unknown TCP_MSG_GREET_ command %04x.\n"), msgtype);
                        PacketWrite2    (ack_pak, TCP_ACK_REFUSE);
                        PacketWrite2    (ack_pak, ack_flags);
                        PacketWriteLNTS (ack_pak, "");
                        SrvMsgGreet     (ack_pak, cmd, "", 0, 0, "");
                        break;
                    
                }
                free (name);
                free (gtext);
                accept = -1;
            }
            break;
#ifdef ENABLE_SSL            
        case MSG_SSL_OPEN:  /* Licq compatible SSL handshake */
            if (!unk2)
            {
                PacketWrite2     (ack_pak, ack_status);
                PacketWrite2     (ack_pak, ack_flags);
                PacketWriteLNTS  (ack_pak, c_out ("1"));
                accept = -1;
                ack_event->conn->ssl_status = SSL_STATUS_INIT;
            }
            break;
        case MSG_SSL_CLOSE:
            if (!unk2)
            {
                PacketWrite2     (ack_pak, ack_status);
                PacketWrite2     (ack_pak, ack_flags);
                PacketWriteLNTS  (ack_pak, c_out (""));
                accept = -1;
                ack_event->conn->ssl_status = SSL_STATUS_CLOSE;
            }
            break;
#endif
        default:
            if (prG->verbose & DEB_PROTOCOL)
                rl_printf (i18n (2066, "Unknown TCP_MSG_ command %04x.\n"), msgtype);
            /* fall-through */
        case MSG_CHAT:
            /* chat ist not implemented, so reject chat requests */
            accept = FALSE;
            break;

        /* Regular messages */
        case MSG_AUTO:
        case MSG_NORM:
        case MSG_URL:
        case MSG_AUTH_REQ:
        case MSG_AUTH_DENY:
        case MSG_AUTH_GRANT:
        case MSG_AUTH_ADDED:
        case MSG_WEB:
        case MSG_EMAIL:
        case MSG_CONTACT:
            /**/    PacketRead4 (inc_pak);
            /**/    PacketRead4 (inc_pak);
            cctmp = PacketReadL4Str (inc_pak, NULL);
            if (!strcmp (cctmp->txt, CAP_GID_UTF8))
                OptSetStr (opt, CO_MSGTEXT, text->txt);
            if (*text->txt)
                IMSrvMsgFat (cont, NOW, opt);
            inc_event->opt = NULL;
            PacketWrite2     (ack_pak, ack_status);
            PacketWrite2     (ack_pak, ack_flags);
            PacketWriteLNTS  (ack_pak, c_out_for (ack_msg, cont, msgtype));
            if (msgtype == MSG_NORM)
            {
                PacketWrite4 (ack_pak, TCP_COL_FG);
                PacketWrite4 (ack_pak, TCP_COL_BG);
            }
            if (CONT_UTF8 (cont, msgtype))
                PacketWriteDLStr     (ack_pak, CAP_GID_UTF8);
            accept = -1;
            break;
    }
    switch (accept)
    {
        case FALSE:
            PacketWrite2      (ack_pak, TCP_ACK_REFUSE);
            PacketWrite2      (ack_pak, ack_flags);
            PacketWriteLNTS   (ack_pak, c_out_to (ack_msg, cont));
            break;

        case TRUE:
            PacketWrite2      (ack_pak, ack_status);
            PacketWrite2      (ack_pak, ack_flags);
            PacketWriteLNTS   (ack_pak, c_out_to (ack_msg, cont));
    }
#ifdef WIP
    if (prG->verbose)
    rl_printf ("FIXMEWIP: Finishing advanced message: events %p, %p.\n", inc_event, ack_event);
#endif
    QueueDequeueEvent (ack_event);
    ack_event->callback (ack_event);
}

