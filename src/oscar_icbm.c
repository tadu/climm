/*
 * Handles incoming and creates outgoing SNAC packets
 * for the family 4 (icbm) commands.
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
#include "oscar.h"
#include "oscar_tlv.h"
#include "oscar_flap.h"
#include "oscar_snac.h"
#include "oscar_icbm.h"
#include "packet.h"
#include "contact.h"
#include "connection.h"
#include "conv.h"
#include "server.h"
#include "icq_response.h"
#include "cmd_pkt_cmd_v5_util.h"

static void SnacCallbackType2Ack (Event *event);
static void SnacCallbackType2 (Event *event);

/*
 * SRV_ICBMERR - SNAC(4,1)
 */
JUMP_SNAC_F(SnacSrvIcbmerr)
{
    Connection *serv = event->conn;
    UWORD err = PacketReadB2 (event->pak);

    if ((event->pak->ref & 0xffff) == 0x1771 && (err == 0xe || err == 0x4))
    {
        if (err == 0xe)
            M_print (i18n (2017, "The user is online, but possibly invisible.\n"));
        else
            M_print (i18n (2022, "The user is offline.\n"));
        return;
    }

    event = QueueDequeue (serv, QUEUE_TYPE2_RESEND_ACK, event->pak->ref);
    if (event && event->callback)
        event->callback (event);
    else if (err == 4)
        M_print (i18n (2022, "The user is offline.\n"));
    else if (err != 0xd)
        M_printf (i18n (2191, "Instant message error: %d.\n"), err);
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

    event = QueueDequeue (serv, QUEUE_TYPE2_RESEND, seq_dc);

    if ((msgtype & 0x300) == 0x300)
        IMSrvMsg (cont, serv, NOW, OptSetVals (NULL,
                  CO_ORIGIN, CV_ORIGIN_v8, CO_MSGTYPE, msgtype, CO_MSGTEXT, text, 0));
    else if (event)
    {
        const char *opt_text;
        if (OptGetStr (event->opt, CO_MSGTEXT, &opt_text));
        {
            IMIntMsg (cont, serv, NOW, STATUS_OFFLINE, INT_MSGACK_TYPE2, opt_text, NULL);
            if ((~cont->oldflags & CONT_SEENAUTO) && strlen (text) && strcmp (text, opt_text))
            {
                IMSrvMsg (cont, serv, NOW, OptSetVals (NULL, CO_ORIGIN, CV_ORIGIN_v8,
                          CO_MSGTYPE, MSG_AUTO, CO_MSGTEXT, text, 0));
                cont->oldflags |= CONT_SEENAUTO;
            }
        }
    }
    EventD (event);
    free (text);
}

/*
 * CLI_SENDMSG - SNAC(4,6)
 */
UBYTE SnacCliSendmsg (Connection *serv, Contact *cont, const char *text, UDWORD type, UBYTE format)
{
    Packet *pak;
    UDWORD mtime = rand() % 0xffff, mid = rand() % 0xffff;
    
    if (!cont)
        return RET_DEFER;
    
    if (format == 2 || type == MSG_GET_PEEK)
        return SnacCliSendmsg2 (serv, cont, OptSetVals (NULL, CO_MSGTYPE, type, CO_MSGTEXT, text, 0));
    
    IMIntMsg (cont, serv, NOW, STATUS_OFFLINE, INT_MSGACK_V8, text, NULL);
        
    if (!format || format == 0xff)
    {
        switch (type & 0xff)
        {
            case MSG_AUTO:
            case MSG_URL:
            case MSG_AUTH_REQ:
            case MSG_AUTH_GRANT:
            case MSG_AUTH_DENY:
            case MSG_AUTH_ADDED:
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
                return RET_DEFER;
        }
    }
    
    pak = SnacC (serv, 4, 6, 0, 0);
    PacketWriteB4 (pak, mtime);
    PacketWriteB4 (pak, mid);
    PacketWriteB2 (pak, format);
    PacketWriteCont (pak, cont);
    
    switch (format)
    {
        const char *convtext;
        int remenc;

        case 1:
            {
            strc_t str;
            int enc = ENC_LATIN1, icqenc = 0;
            
            remenc = ContactPrefVal (cont, CO_ENCODING);
            
            if (cont->status != STATUS_OFFLINE &&
                HAS_CAP (cont->caps, CAP_UTF8) && cont->dc && cont->dc->version >= 7
                && !(cont->dc->id1 == (time_t)0xffffff42 && cont->dc->id2 < (time_t)0x00040a03)) /* exclude old mICQ */
            {
                enc = ENC_UCS2BE;
                icqenc = 0x20000;
            }
            else
            {
                /* too bad, there's nothing we can do */
                enc = remenc;
                icqenc = 0x30000;
            }
            if (type != 1)
            {
                icqenc = type;
                enc = ENC_LATIN9;
            }

            str = s_split (&text, enc, 450);

            PacketWriteTLV     (pak, 2);
            PacketWriteTLV     (pak, 1281);
            if (icqenc == 0x20000)
                PacketWriteB2  (pak, 0x0106);
            else
                PacketWrite1   (pak, 0x01);
            PacketWriteTLVDone (pak);
            PacketWriteTLV     (pak, 257);
            PacketWriteB4      (pak, icqenc);
            PacketWriteData    (pak, str->txt, str->len);
            PacketWriteTLVDone (pak);
            PacketWriteTLVDone (pak);
            PacketWriteB2 (pak, 6);
            PacketWriteB2 (pak, 0);
            SnacSend (serv, pak);
            if (*text)
                return SnacCliSendmsg (serv, cont, text, type, format);
            }
            break;
        case 4:
            remenc = ContactPrefVal (cont, CO_ENCODING);
            convtext = ConvTo (text, remenc)->txt;
            
            PacketWriteTLV     (pak, 5);
            PacketWrite4       (pak, serv->uin);
            PacketWrite1       (pak, type);
            PacketWrite1       (pak, 0);
#if 0
            PacketWrite2 (pak, strlen (convtext) + strlen (ConvEncName (remenc)) + 2);
            PacketWriteData (pak, convtext, strlen (convtext));
            PacketWrite1 (pak, 0);
            PacketWriteData (pak, ConvEncName (remenc), strlen (ConvEncName (remenc)));
            PacketWrite1 (pak, 0);
#else
            PacketWriteLNTS    (pak, c_out_to (text, cont));
#endif
            PacketWriteTLVDone (pak);
            PacketWriteB2 (pak, 6);
            PacketWriteB2 (pak, 0);
            SnacSend (serv, pak);
    }
    return RET_OK;
}

static void SnacCallbackType2Ack (Event *event)
{
    Connection *serv = event->conn;
    Contact *cont = event->cont;
    Event *aevent;
    UDWORD opt_ref;

    if (!serv)
    {
        EventD (event);
        return;
    }
    if (   !OptGetVal (event->opt, CO_REF, &opt_ref)
        || !(aevent = QueueDequeue (serv, QUEUE_TYPE2_RESEND, opt_ref)))
    {
        EventD (event);
        return;
    }
    ASSERT_SERVER (serv);
    
    IMCliMsg (serv, cont, aevent->opt);
    aevent->opt = NULL;
    EventD (aevent);
    EventD (event);
}

static void SnacCallbackType2 (Event *event)
{
    Connection *serv = event->conn;
    Contact *cont = event->cont;
    Packet *pak = event->pak;
    const char *opt_text;
    
    if (!OptGetStr (event->opt, CO_MSGTEXT, &opt_text))
        opt_text = "";

    if (!serv || !cont)
    {
        if (!serv && opt_text)
            M_printf (i18n (2234, "Message %s discarded - lost session.\n"), opt_text);
        EventD (event);
        return;
    }

    ASSERT_SERVER (serv);
    assert (pak);

    if (event->attempts < MAX_RETRY_TYPE2_ATTEMPTS && serv->connect & CONNECT_MASK)
    {
        if (serv->connect & CONNECT_OK)
        {
            if (event->attempts > 1)
                IMIntMsg (cont, serv, NOW, STATUS_OFFLINE, INT_MSGTRY_TYPE2,
                          opt_text, NULL);
            SnacSend (serv, PacketClone (pak));
            event->attempts++;
            /* allow more time for the peer's ack than the server's ack */
            event->due = time (NULL) + RETRY_DELAY_TYPE2 + 5;
            QueueEnqueueData (serv, QUEUE_TYPE2_RESEND_ACK, pak->ref,
                              time (NULL) + RETRY_DELAY_TYPE2, NULL, cont,
                              OptSetVals (NULL, CO_REF, event->seq, 0),
                              &SnacCallbackType2Ack);
        }
        else
            event->due = time (NULL) + 1;
        QueueEnqueue (event);
        return;
    }
    
    IMCliMsg (serv, cont, event->opt);
    event->opt = NULL;
    EventD (event);
}

/*
 * CLI_SENDMSG - SNAC(4,6) - type2
 */
UBYTE SnacCliSendmsg2 (Connection *serv, Contact *cont, Opt *opt)
{
    Packet *pak;
    UDWORD mtime = rand() % 0xffff, mid = rand() % 0xffff;
    BOOL peek = 0;
    UDWORD opt_type, opt_trans, opt_force;
    const char *opt_text;
    
    if (!OptGetStr (opt, CO_MSGTEXT, &opt_text))
        return RET_FAIL;
    if (!OptGetVal (opt, CO_MSGTYPE, &opt_type))
        opt_type = MSG_NORM;
    if (!OptGetVal (opt, CO_FORCE, &opt_force))
        opt_force = 0;
    if (!OptGetVal (opt, CO_MSGTRANS, &opt_trans))
        opt_trans = CV_MSGTRANS_ANY;

    if (opt_type == MSG_GET_PEEK)
    {
        peek = 1;
        opt_type = MSG_GET_AWAY;
    }
    
    
    if (!cont || !(peek || (opt_type & 0xff) == MSG_GET_VER || opt_force ||
        (HAS_CAP (cont->caps, CAP_SRVRELAY) && HAS_CAP (cont->caps, CAP_ISICQ))))
        return RET_DEFER;
    
    if (!opt_force)
    {
        switch (opt_type & 0xff)
        {
            case MSG_AUTO:
            case MSG_AUTH_REQ:
            case MSG_AUTH_GRANT:
            case MSG_AUTH_DENY:
            case MSG_AUTH_ADDED:
                return RET_DEFER;
        }
    }

    serv->our_seq_dc--;
    
    OptSetVal (opt, CO_MSGTRANS, opt_trans &= ~CV_MSGTRANS_TYPE2);

    pak = SnacC (serv, 4, 6, 0, peek ? 0x1771 : 0);
    PacketWriteB4 (pak, mtime);
    PacketWriteB4 (pak, mid);
    PacketWriteB2 (pak, 2);
    PacketWriteCont (pak, cont);
    
    PacketWriteTLV     (pak, 5);
     PacketWrite2       (pak, 0);
     PacketWriteB4      (pak, mtime);
     PacketWriteB4      (pak, mid);
     PacketWriteCapID   (pak, CAP_SRVRELAY);
     PacketWriteTLV2    (pak, 10, 1);
     PacketWriteB4      (pak, 0x000f0000); /* empty TLV(15) */
     PacketWriteTLV     (pak, 10001);
      PacketWriteLen     (pak);
       PacketWrite2       (pak, serv->assoc && serv->assoc->connect & CONNECT_OK
                              ? serv->assoc->version : 8);
       PacketWriteCapID   (pak, CAP_NONE);
       PacketWrite2       (pak, 0);
       PacketWrite4       (pak, 3);
       PacketWrite1       (pak, 0);
       PacketWrite2       (pak, serv->our_seq_dc);
      PacketWriteLenDone (pak);
      SrvMsgAdvanced     (pak, serv->our_seq_dc, opt_type, serv->status, cont->status, -1, c_out_for (opt_text, cont, opt_type));
      PacketWrite4       (pak, TCP_COL_FG);
      PacketWrite4       (pak, TCP_COL_BG);
      if (CONT_UTF8 (cont, opt_type))
          PacketWriteDLStr     (pak, CAP_GID_UTF8);
     PacketWriteTLVDone (pak);
     if (peek) /* make a syntax error */
         PacketWriteB4  (pak, 0x00030000);
    PacketWriteTLVDone (pak);
    PacketWriteB4      (pak, 0x00030000); /* empty TLV(3) */
    
    if (peek)
    {
        SnacSend (serv, pak);
        OptD (opt);
    }
    else
        QueueEnqueueData (serv, QUEUE_TYPE2_RESEND, serv->our_seq_dc,
                          time (NULL), pak, cont, opt, &SnacCallbackType2);
    return RET_INPR;
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
    TLV *tlv;
    Opt *opt;
    UDWORD midtim, midrnd, midtime, midrand, unk, tmp, type1enc;
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
              PacketReadB2 (pak); /* COUNT */
    
    if (!cont)
        return;

    tlv = TLVRead (pak, PacketReadLeft (pak));

#ifdef WIP
    if (tlv[6].str.len && tlv[6].nr != cont->status)
        M_printf ("FIXMEWIP: status for %ld embedded in message 0x%08lx different from server status 0x%08lx.\n", cont->uin, tlv[6].nr, cont->status);
#endif

    if (tlv[6].str.len)
        event->opt = OptSetVals (event->opt, CO_STATUS, tlv[6].nr, 0);

    /* tlv[2] may be there twice - ignore the member since time(NULL). */
    if (tlv[2].str.len == 4)
        TLVDone (tlv, 2);

    switch (type)
    {
        case 1:
            if (!tlv[2].str.len)
            {
                SnacSrvUnknown (event);
                TLVD (tlv);
                return;
            }

            p = PacketCreate (&tlv[2].str);
            PacketReadB2 (p);
            PacketReadData (p, NULL, PacketReadB2 (p));
                       PacketReadB2 (p);
            len      = PacketReadB2 (p);
            
            type1enc = PacketReadB4 (p);
            if (len < 4)
            {
                SnacSrvUnknown (event);
                TLVD (tlv);
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
            IMSrvMsg (cont, serv, NOW, opt);
            Auto_Reply (serv, cont);
            s_done (&str);
            break;
        case 2:
            p = PacketCreate (&tlv[5].str);
            type   = PacketReadB2 (p);
            midtim = PacketReadB4 (p);
            midrnd = PacketReadB4 (p);
            cap1   = PacketReadCap (p);
            TLVD (tlv);
            
            ContactSetCap (cont, cap1);
            if (midtim != midtime || midrnd != midrand)
            {
                SnacSrvUnknown (event);
                PacketD (p);
                return;
            }

            tlv = TLVRead (p, PacketReadLeft (p));
            PacketD (p);
            
            if ((i = TLVGet (tlv, 0x2711)) == (UWORD)-1)
            {
                if (TLVGet (tlv, 11) == (UWORD)-1)
                    SnacSrvUnknown (event);
#ifdef WIP
                else
                {
                    M_printf ("%s %s%*s%s ", s_now, COLCONTACT, uiG.nick_len + s_delta (cont->nick), cont->nick, COLNONE);
                    M_printf ("FIXMEWIP: tlv(b)-only packet.\n");
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
                        M_printf ("%s %*s FIXMEWIP: updates dc to %s:%ld|%ld|%ld v%d %d seq %ld\n",
                                  s_now, uiG.nick_len + s_delta (cont->nick), cont->nick,
                                  s_ip (sip), sp1, sp2, sop, sver, scon, sunk);
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
                    
                    event->opt = OptSetVals (event->opt, CO_ORIGIN, CV_ORIGIN_v8, 0);
                    event->cont = cont;
                    newevent = QueueEnqueueData (serv, QUEUE_ACKNOWLEDGE, seq1,
                                 (time_t)-1, p, cont, NULL, &SnacSrvCallbackSendack);
                    SrvReceiveAdvanced (serv, event, pp, newevent);
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
            p = PacketCreate (&tlv[5].str);
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
            IMSrvMsg (cont, serv, NOW, opt);
            Auto_Reply (serv, cont);
            break;
        default:
            SnacSrvUnknown (event);
            TLVD (tlv);
            return;
    }
    TLVD (tlv);
}

/*
 * SRV_ACKMSG - SNAC(4,C)
 */
JUMP_SNAC_F(SnacSrvSrvackmsg)
{
    Connection *serv = event->conn;
    Packet *pak;
    Contact *cont;
    /* UDWORD mid1, mid2; */
    UWORD type;

    pak = event->pak;

    /*mid1=*/PacketReadB4 (pak);
    /*mid2=*/PacketReadB4 (pak);
    type = PacketReadB2 (pak);

    cont = PacketReadCont (pak, serv);
    
    if (!cont)
        return;
    
    switch (type)
    {
        case 1:
        case 4:
            IMOffline (cont, serv);

            M_printf ("%s %s%*s%s ", s_now, COLCONTACT, uiG.nick_len + s_delta (cont->nick), cont->nick, COLNONE);
            M_print  (i18n (2126, "is offline, message queued on server.\n"));

/*          cont->status = STATUS_OFFLINE;
            putlog (serv, NOW, cont, STATUS_OFFLINE, LOG_ACK, 0xFFFF, 
                s_sprintf ("%08lx%08lx\n", mid1, mid2)); */
            break;
        case 2: /* msg was received by server */
            EventD (QueueDequeue (serv, QUEUE_TYPE2_RESEND_ACK, pak->ref));
            break;
    }
}

