/*
 * Handles incoming and creates outgoing SNAC packets.
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
#include "util.h"
#include "util_ui.h"
#include "util_io.h"
#include "util_extra.h"
#include "util_syntax.h"
#include "contact.h"
#include "server.h"
#include "packet.h"
#include "session.h"
#include "preferences.h"
#include "icq_response.h"
#include "cmd_pkt_cmd_v5_util.h"
#include "cmd_pkt_v8.h"
#include "cmd_pkt_v8_snac.h"
#include "cmd_pkt_v8_flap.h"
#include "cmd_pkt_v8_tlv.h"
#include "im_icq8.h"
#include "file_util.h"
#include "buildmark.h"
#include "cmd_user.h"
#include "conv.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

typedef void (jump_snac_f)(Event *);
typedef struct { UWORD fam; UWORD cmd; const char *name; jump_snac_f *f; } SNAC;
#define JUMP_SNAC_F(f) void f (Event *event)

extern int reconn;

static jump_snac_f SnacSrvFamilies, SnacSrvFamilies2, SnacSrvMotd,
    SnacSrvRates, SnacSrvReplyicbm, SnacSrvReplybuddy, SnacSrvReplybos,
    SnacSrvReplyinfo, SnacSrvReplylocation, SnacSrvUseronline, SnacSrvContacterr,
    SnacSrvRegrefused, SnacSrvUseroffline, SnacSrvRecvmsg, SnacSrvUnknown,
    SnacSrvFromicqsrv, SnacSrvAddedyou, SnacSrvToicqerr, SnacSrvNewuin,
    SnacSrvSetinterval, SnacSrvSrvackmsg, SnacSrvAckmsg, SnacSrvAuthreq,
    SnacSrvAuthreply, SnacSrvIcbmerr, SnacSrvReplyroster, SnacSrvContrefused,
    SnacSrvRateexceeded, SnacSrvReplylists, SnacSrvUpdateack, SnacServerpause;

static void SrvCallBackKeepalive (Event *event);
static Packet *SnacC (Connection *conn, UWORD fam, UWORD cmd, UWORD flags, UDWORD ref);
static void SnacCallbackType2Ack (Event *event);
static void SnacCallbackType2 (Event *event);
static Packet *SnacMetaC (Connection *conn, UWORD sub, UWORD type, UWORD ref);
static void SnacMetaSend (Connection *conn, Packet *pak);

static SNAC SNACv[] = {
    {  1,  3, NULL, NULL},
    { 19,  4, NULL, NULL},
    {  2,  1, NULL, NULL},
    {  3,  1, NULL, NULL},
    { 21,  1, NULL, NULL},
    {  4,  1, NULL, NULL},
    {  6,  1, NULL, NULL},
    {  8,  0, NULL, NULL},
    {  9,  1, NULL, NULL},
    { 10,  1, NULL, NULL},
    { 11,  1, NULL, NULL},
    { 12,  1, NULL, NULL},
    {  0,  0, NULL, NULL}
};

static SNAC SNACS[] = {
    {  1,  3, "SRV_FAMILIES",        SnacSrvFamilies},
    {  1,  7, "SRV_RATES",           SnacSrvRates},
    {  1, 10, "SRV_RATEEXCEEDED",    SnacSrvRateexceeded},
    {  1, 11, "SRV_SERVERPAUSE",     SnacServerpause},
    {  1, 15, "SRV_REPLYINFO",       SnacSrvReplyinfo},
    {  1, 18, "SRV_MIGRATIONREQ",    SnacServerpause},
    {  1, 19, "SRV_MOTD",            SnacSrvMotd},
    {  1, 24, "SRV_FAMILIES2",       SnacSrvFamilies2},
    {  2,  3, "SRV_REPLYLOCATION",   SnacSrvReplylocation},
    {  3,  1, "SRV_CONTACTERR",      SnacSrvContacterr},
    {  3,  3, "SRV_REPLYBUDDY",      SnacSrvReplybuddy},
    {  3, 11, "SRV_USERONLINE",      SnacSrvUseronline},
    {  3, 10, "SRV_REFUSE",          SnacSrvContrefused},
    {  3, 12, "SRV_USEROFFLINE",     SnacSrvUseroffline},
    {  4,  1, "SRV_ICBMERR",         SnacSrvIcbmerr},
    {  4,  5, "SRV_REPLYICBM",       SnacSrvReplyicbm},
    {  4,  7, "SRV_RECVMSG",         SnacSrvRecvmsg},
    {  4, 11, "SRV/CLI_ACKMSG",      SnacSrvAckmsg},
    {  4, 12, "SRV_SRVACKMSG",       SnacSrvSrvackmsg},
    {  9,  3, "SRV_REPLYBOS",        SnacSrvReplybos},
    { 11,  2, "SRV_SETINTERVAL",     SnacSrvSetinterval},
    { 19,  3, "SRV_REPLYLISTS",      SnacSrvReplylists},
    { 19,  6, "SRV_REPLYROSTER",     SnacSrvReplyroster},
    { 19, 14, "SRV_UPDATEACK",       SnacSrvUpdateack},
    { 19, 15, "SRV_REPLYROSTEROK",   NULL},
    { 19, 25, "SRV_AUTHREQ",         SnacSrvAuthreq},
    { 19, 27, "SRV_AUTHREPLY",       SnacSrvAuthreply},
    { 19, 28, "SRV_ADDEDYOU",        SnacSrvAddedyou},
    { 21,  1, "SRV_TOICQERR",        SnacSrvToicqerr},
    { 21,  3, "SRV_FROMICQSRV",      SnacSrvFromicqsrv},
    { 23,  1, "SRV_REGREFUSED",      SnacSrvRegrefused},
    { 23,  5, "SRV_NEWUIN",          SnacSrvNewuin},
    {  1,  2, "CLI_READY",           NULL},
    {  1,  6, "CLI_RATESREQUEST",    NULL},
    {  1,  8, "CLI_ACKRATES",        NULL},
    {  1, 12, "CLI_ACKSERVERPAUSE",  NULL},
    {  1, 14, "CLI_REQINFO",         NULL},
    {  1, 23, "CLI_FAMILIES",        NULL},
    {  1, 30, "CLI_SETSTATUS",       NULL},
    {  2,  2, "CLI_REQLOCATION",     NULL},
    {  2,  4, "CLI_SETUSERINFO",     NULL},
    {  3,  2, "CLI_REQBUDDY",        NULL},
    {  3,  4, "CLI_ADDCONTACT",      NULL},
    {  3,  5, "CLI_REMCONTACT",      NULL},
    {  4,  2, "CLI_SETICBM",         NULL},
    {  4,  4, "CLI_REQICBM",         NULL},
    {  4,  6, "CLI_SENDMSG",         NULL},
    {  9,  2, "CLI_REQBOS",          NULL},
    {  9,  5, "CLI_ADDVISIBLE",      NULL},
    {  9,  6, "CLI_REMVISIBLE",      NULL},
    {  9,  7, "CLI_ADDINVISIBLE",    NULL},
    {  9,  8, "CLI_REMINVISIBLE",    NULL},
    { 19,  2, "CLI_REQLISTS",        NULL},
    { 19,  4, "CLI_REQROSTER",       NULL},
    { 19,  5, "CLI_CHECKROSTER",     NULL},
    { 19,  7, "CLI_ROSTERACK",       NULL},
    { 19,  8, "CLI_ROSTERADD",       NULL},
    { 19,  9, "CLI_ROSTERUPDATE",    NULL},
    { 19, 10, "CLI_ROSTERDELETE",    NULL},
    { 19, 17, "CLI_ADDSTART",        NULL},
    { 19, 18, "CLI_ADDEND",          NULL},
    { 19, 20, "CLI_GRANTAUTH?",      NULL},
    { 19, 24, "CLI_REQAUTH",         NULL},
    { 19, 26, "CLI_AUTHORIZE",       NULL},
    { 21,  2, "CLI_TOICQSRV",        NULL},
    { 23,  4, "CLI_REGISTERUSER",    NULL},
    {  0,  0, "unknown",             NULL}
};

#define SnacSend FlapSend

/*
 * Interprets the given SNAC.
 */
void SnacCallback (Event *event)
{
    Packet  *pak  = event->pak;
    SNAC *s;
    UWORD family;
    
    ASSERT_SERVER(event->conn);
    
    family     = PacketReadB2 (pak);
    pak->cmd   = PacketReadB2 (pak);
    pak->flags = PacketReadB2 (pak);
    pak->ref   = PacketReadB4 (pak);
    
    if (pak->flags & 0x8000)
        PacketReadData (pak, NULL, PacketReadB2 (pak));
    
    for (s = SNACS; s->fam; s++)
        if (s->fam == family && s->cmd == pak->cmd)
            break;
    
    if (s->f)
        s->f (event);
    else
        SnacSrvUnknown (event);

    EventD (event);
}

/*
 * Keeps track of sending a keep alive every 30 seconds.
 */
static void SrvCallBackKeepalive (Event *event)
{
    if (event->conn && event->conn->connect & CONNECT_OK && event->conn->type == TYPE_SERVER)
    {
        FlapCliKeepalive (event->conn);
        event->due = time (NULL) + 30;
        QueueEnqueue (event);
        return;
    }
    EventD (event);
}

/*
 * Returns the name of the SNAC, or "unknown".
 */
const char *SnacName (UWORD fam, UWORD cmd)
{
    SNAC *s;
    
    for (s = SNACS; s->fam; s++)
        if (s->fam == fam && s->cmd == cmd)
            return s->name;
    return s->name;
}

/*
 * Creates a new SNAC.
 */
static Packet *SnacC (Connection *conn, UWORD fam, UWORD cmd, UWORD flags, UDWORD ref)
{
    Packet *pak;
    
    if (!conn->our_seq2)
        conn->our_seq2 = rand () & 0x7fff;
    if (!ref)
        ref = rand () & 0x7fff;
    
    pak = FlapC (2);
    PacketWriteB2 (pak, fam);
    PacketWriteB2 (pak, cmd);
    PacketWriteB2 (pak, flags);
    PacketWriteB4 (pak, pak->ref = ref);
    return pak;
}

/*
 * Print a given SNAC.
 */
void SnacPrint (Packet *pak)
{
    UWORD fam, cmd, flag, opos = pak->rpos;
    UDWORD ref, len;

    pak->rpos = 6;
    fam  = PacketReadB2 (pak);
    cmd  = PacketReadB2 (pak);
    flag = PacketReadB2 (pak);
    ref  = PacketReadB4 (pak);
    len  = PacketReadAtB2 (pak, pak->rpos);

    M_printf ("%s %sSNAC     (%x,%x) [%s] flags %x ref %lx",
             s_dumpnd (pak->data + 6, flag & 0x8000 ? 10 + len : 10),
             COLDEBUG, fam, cmd, SnacName (fam, cmd), flag, ref);
    if (flag & 0x8000)
    {
        M_printf (" extra (%ld)", len);
        pak->rpos += len + 2;
    }
    M_printf ("%s\n", COLNONE);

    if (prG->verbose & DEB_PACK8DATA || ~prG->verbose & DEB_PACK8)
    {
        char *f, *syn = strdup (s_sprintf ("gs%dx%ds", fam, cmd));
        M_print (f = PacketDump (pak, syn));
        free (f);
        free (syn);
    }

    pak->rpos = opos;
}

/*
 * Any unknown SNAC.
 */
static JUMP_SNAC_F(SnacSrvUnknown)
{
    if (!(prG->verbose & DEB_PACK8))
    {
        M_printf ("%s " COLINDENT "%s%s ", s_now, COLSERVER, i18n (1033, "Incoming v8 server packet:"));
        FlapPrint (event->pak);
        M_print (COLEXDENT "\n");
    }
}

/***********************************************/

/*
 * SRV_FAMILIES - SNAC(1,3),
 */
static JUMP_SNAC_F(SnacSrvFamilies)
{
    Packet *pak;
    SNAC *s;
    UWORD fam;

    pak = event->pak;
    while (PacketReadLeft (pak) >= 2)
    {
        fam = PacketReadB2 (pak);
        
        for (s = SNACv; s->fam; s++)
            if (s->fam == fam && !s->f)
                break;
        if (!s->fam)
        {
            M_printf (i18n (1899, "Unknown family requested: %d\n"), fam);
            continue;
        }
    }
    SnacCliFamilies (event->conn);
}

/*
 * SRV_RATES - SNAC(1,7)
 * CLI_ACKRATES - SNAC(1,8)
 */
static JUMP_SNAC_F(SnacSrvRates)
{
    UWORD nr, grp;
    Packet *pak;
    
    pak = SnacC (event->conn, 1, 8, 0, 0);
    nr = PacketReadB2 (event->pak); /* ignore the remainder */
    while (nr--)
    {
        grp = PacketReadB2 (event->pak);
        event->pak->rpos += 33;
        PacketWriteB2 (pak, grp);
    }
    SnacSend (event->conn, pak);
}


/*
 * SRV_RATEEXCEEDED - SNAC(1,10)
 */
static JUMP_SNAC_F(SnacSrvRateexceeded)
{
    M_print (i18n (2188, "You're sending data too fast - stop typing now, or the server will disconnect!\n"));
}

/*
 * SRV_SERVERPAUSE - SNAC(1,11)
 * SRV_MIGRATIONREQ - SNAC(1,18)
 */
static JUMP_SNAC_F(SnacServerpause)
{
    ContactGroup *cg = event->conn->contacts;
    Contact *cont;
    int i;

#ifdef WIP
    M_printf ("%s WIP: reconnecting because of serverpause.\n", s_time (NULL));
#endif
    ConnectionInitServer(event->conn);
    for (i = 0; (cont = ContactIndex (cg, i)); i++)
        cont->status = STATUS_OFFLINE;
}

static void SrvCallbackTodoEg (Event *event)
{
    if (event && event->conn && event->conn->uin && event->conn->type == TYPE_SERVER)
        CmdUser (s_sprintf ("\\as %ld eg", event->conn->uin));
}

/*
 * SRV_REPLYINFO - SNAC(1,15)
 */
static JUMP_SNAC_F(SnacSrvReplyinfo)
{
    Contact *cont;
    Packet *pak;
    TLV *tlv;
    UDWORD status;
    
    pak = event->pak;
    cont = PacketReadCont (pak, event->conn);
    
    if (cont->uin != event->conn->uin)
        M_printf (i18n (1907, "Warning: Server thinks our UIN is %ld, when it is %ld.\n"),
                  cont->uin, event->conn->uin);
    PacketReadB2 (pak);
    PacketReadB2 (pak);
    tlv = TLVRead (pak, PacketReadLeft (pak));
    if (tlv[10].str.len)
    {
        event->conn->our_outside_ip = tlv[10].nr;
        if (prG->verbose)
            M_printf (i18n (1915, "Server says we're at %s.\n"), s_ip (event->conn->our_outside_ip));
        if (event->conn->assoc)
            event->conn->assoc->our_outside_ip = event->conn->our_outside_ip;
    }
    if (tlv[6].str.len)
    {
        status = tlv[6].nr;
        if (status != event->conn->status)
        {
            event->conn->status = status;
            M_printf ("%s %s\n", s_now, s_status (event->conn->status));
        }
    }
    /* TLV 1 c f 2 3 ignored */
    TLVD (tlv);
    
    if (~event->conn->connect & CONNECT_OK)
    {
        SnacCliSetrandom (event->conn, prG->chat);
        event->conn->connect = CONNECT_OK | CONNECT_SELECT_R;
        reconn = 0;
        QueueEnqueueData (event->conn, QUEUE_SRV_KEEPALIVE, 0, time (NULL) + 30,
                          NULL, event->cont, NULL, &SrvCallBackKeepalive);
        QueueEnqueueData (event->conn, QUEUE_TODO_EG, 0, time (NULL) + 3,
                          NULL, 0, NULL, &SrvCallbackTodoEg);
    }
}

/*
 * SRV_FAMILIES2 - SNAC(1,18)
 */
static JUMP_SNAC_F(SnacSrvFamilies2)
{
    Packet *pak;
    SNAC *s;
    UWORD fam, ver;
    
    pak = event->pak;
    while (PacketReadLeft (pak) >= 4)
    {
        fam = PacketReadB2 (pak);
        ver = PacketReadB2 (pak);
        for (s = SNACv; s->fam; s++)
            if (s->fam == fam)
                break;
        if (!s->fam)
            continue;
        if (s->cmd > ver)
            M_printf (i18n (1904, "Server doesn't understand ver %d (only %d) for family %d!\n"), s->cmd, ver, fam);
    }
}

/*
 * SRV_MOTD - SNAC(1,13)
 */
static JUMP_SNAC_F(SnacSrvMotd)
{
    if (!(event->conn->connect & CONNECT_OK))
        event->conn->connect++;

    SnacCliRatesrequest (event->conn);
    SnacCliReqinfo      (event->conn);
    SnacCliReqlists     (event->conn);
    if (event->conn->flags & CONN_WIZARD)
    {
        SnacCliReqroster  (event->conn);
        QueueEnqueueData (event->conn, QUEUE_REQUEST_ROSTER, IMROSTER_IMPORT, 0x7fffffffL,
                          NULL, NULL, NULL, NULL);
    }
    else
    {
        SnacCliCheckroster  (event->conn);
        QueueEnqueueData (event->conn, QUEUE_REQUEST_ROSTER, IMROSTER_DIFF, 0x7fffffffL,
                          NULL, NULL, NULL, NULL);
    }
    SnacCliReqlocation  (event->conn);
    SnacCliBuddy        (event->conn);
    SnacCliReqicbm      (event->conn);
    SnacCliReqbos       (event->conn);
}

/*
 * SRV_REPLYLOCATION - SNAC(2,3)
 */
static JUMP_SNAC_F(SnacSrvReplylocation)
{
    /* ignore all data, do nothing */
}

/*
 * SRV_CONTACTERR - SNAC(3,1)
 */
static JUMP_SNAC_F(SnacSrvContacterr)
{
    UWORD err, cnt;
    Contact *cont;
    char first = 0, empty = 0;
    const char *errtxt;
    
    err = PacketReadB2 (event->pak);
    cnt = PacketReadB2 (event->pak);
    
    switch (err)
    {
        case 0x0e: errtxt = i18n (2329, "syntax error");
        case 0x14: errtxt = i18n (2330, "removing non-contact");
        default:   errtxt = i18n (2331, "unknown");
    }

    M_printf (i18n (2332, "Contact error %d (%s) for %d contacts: "), err, errtxt, cnt);

    while (empty < 3)
    {
        if ((cont = PacketReadCont (event->pak, event->conn)))
        {
            if (first)
                M_print (", ");
            if (cont)
                M_printf ("%s (%ld)", cont->nick, cont->uin);
            first = 1;
        }
        else
            empty++;
    }
    M_print ("\n");
}

/*
 * SRV_REPLYBUDDY - SNAC(3,3)
 */
static JUMP_SNAC_F(SnacSrvReplybuddy)
{
    /* ignore all data, do nothing */
}

/*
 * SRV_REFUSE - SNAC(3,A)
 */
static JUMP_SNAC_F(SnacSrvContrefused)
{
    Contact *cont;
    
    cont = PacketReadCont (event->pak, event->conn);
    if (cont)
        M_printf (i18n (2315, "Cannot watch status of %s - too many watchers.\n"), cont->nick);
}

/*
 * SRV_USERONLINE - SNAC(3,B)
 */
static JUMP_SNAC_F(SnacSrvUseronline)
{
    Contact *cont;
    Packet *p, *pak;
    TLV *tlv;
    
    pak = event->pak;
    cont = PacketReadCont (pak, event->conn);

    PacketReadB2 (pak);
    PacketReadB2 (pak);
    tlv = TLVRead (pak, PacketReadLeft (pak));
    if (tlv[10].str.len && CONTACT_DC (cont))
        cont->dc->ip_rem = tlv[10].nr;
    
    if (tlv[12].str.len && CONTACT_DC (cont))
    {
        UDWORD ip;
        p = PacketCreate (&tlv[12].str);
        
                       ip = PacketReadB4 (p);
        cont->dc->port    = PacketReadB4 (p);
        cont->dc->type    = PacketRead1  (p);
        cont->dc->version = PacketReadB2 (p);
        cont->dc->cookie  = PacketReadB4 (p);
                            PacketReadB4 (p);
                            PacketReadB4 (p);
        cont->dc->id1     = PacketReadB4 (p);
        cont->dc->id2     = PacketReadB4 (p);
        cont->dc->id3     = PacketReadB4 (p);
        /* remainder ignored */
        PacketD (p);
        if (ip && ~ip)
            cont->dc->ip_loc = ip;
    }
    /* TLV 1, d, f, 2, 3 ignored */
    if (!~cont->status)
        cont->caps = 0;

    if (tlv[13].str.len)
    {
        p = PacketCreate (&tlv[13].str);
        
        while (PacketReadLeft (p))
            ContactSetCap (cont, PacketReadCap (p));
        PacketD (p);
    }
    ContactSetVersion (cont);
    IMOnline (cont, event->conn, tlv[6].str.len ? tlv[6].nr : 0);
    TLVD (tlv);
}

/*
 * SRV_USEROFFLINE - SNAC(3,C)
 */
static JUMP_SNAC_F(SnacSrvUseroffline)
{
    Contact *cont;
    Packet *pak;
    
    pak = event->pak;
    cont = PacketReadCont (pak, event->conn);

    IMOffline (cont, event->conn);
}

/*
 * SRV_ICBMERR - SNAC(4,1)
 */
static JUMP_SNAC_F(SnacSrvIcbmerr)
{
    UWORD err = PacketReadB2 (event->pak);

    if ((event->pak->ref & 0xffff) == 0x1771 && (err == 0xe || err == 0x4))
    {
        if (err == 0xe)
            M_print (i18n (2017, "The user is online, but possibly invisible.\n"));
        else
            M_print (i18n (2022, "The user is offline.\n"));
        return;
    }

    event = QueueDequeue (event->conn, QUEUE_TYPE2_RESEND_ACK, event->pak->ref);
    if (event && event->callback)
        event->callback (event);
    else if (err == 4)
        M_print (i18n (2022, "The user is offline.\n"));
    else if (err != 0xd)
        M_printf (i18n (2191, "Instant message error: %d.\n"), err);
}

/*
 * SRV_REPLYICBM - SNAC(4,5)
 */
static JUMP_SNAC_F(SnacSrvReplyicbm)
{
    SnacCliSeticbm (event->conn);
}

static JUMP_SNAC_F(SnacSrvAckmsg)
{
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
    cont =    PacketReadCont (pak, event->conn);
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

    event = QueueDequeue (event->conn, QUEUE_TYPE2_RESEND, seq_dc);

    if ((msgtype & 0x300) == 0x300)
        IMSrvMsg (cont, event->conn, NOW, ExtraSet (ExtraSet (NULL,
                  EXTRA_ORIGIN, EXTRA_ORIGIN_v8, NULL),
                  EXTRA_MESSAGE, msgtype, text));
    else if (event)
    {
        IMIntMsg (cont, event->conn, NOW, STATUS_OFFLINE, INT_MSGACK_TYPE2, ExtraGetS (event->extra, EXTRA_MESSAGE), NULL);
        if ((~cont->oldflags & CONT_SEENAUTO) && strlen (text))
        {
            IMSrvMsg (cont, event->conn, NOW, ExtraSet (ExtraSet (NULL,
                      EXTRA_ORIGIN, EXTRA_ORIGIN_dc, NULL),
                      EXTRA_MESSAGE, MSG_AUTO, text));
            cont->oldflags |= CONT_SEENAUTO;
        }
    }
    EventD (event);
    free (text);
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
static JUMP_SNAC_F(SnacSrvRecvmsg)
{
    Contact *cont;
    Event *newevent;
    Cap *cap1, *cap2;
    Packet *p = NULL, *pp = NULL, *pak;
    Extra *extra = NULL;
    TLV *tlv;
    UDWORD midtim, midrnd, midtime, midrand, unk, tmp, type1enc;
    UWORD seq1, tcpver, len, i, msgtyp, type;
    const char *txt = NULL;
    strc_t ctext;
    str_s str = { NULL, 0, 0 };

    pak = event->pak;

    midtime = PacketReadB4 (pak);
    midrand = PacketReadB4 (pak);
    type    = PacketReadB2 (pak);
    cont    = PacketReadCont (pak, event->conn);
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
        extra = ExtraSet (extra, EXTRA_STATUS, tlv[6].nr, NULL);

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
            IMSrvMsg (cont, event->conn, NOW, ExtraSet (ExtraSet (extra,
                      EXTRA_ORIGIN, EXTRA_ORIGIN_v5, NULL),
                      EXTRA_MESSAGE, MSG_NORM, txt));
            Auto_Reply (event->conn, cont);
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
                    M_printf ("%s " COLCONTACT "%*s" COLNONE " ", s_now, uiG.nick_len + s_delta (cont->nick), cont->nick);
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

                    p = SnacC (event->conn, 4, 11, 0, 0);
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
                    
                    event->extra = ExtraSet (extra, EXTRA_ORIGIN, EXTRA_ORIGIN_v8, NULL);
                    event->cont = cont;
                    newevent = QueueEnqueueData (event->conn, QUEUE_ACKNOWLEDGE, seq1,
                                 (time_t)-1, p, cont, NULL, &SnacSrvCallbackSendack);
                    SrvReceiveAdvanced (event->conn, event, pp, newevent);
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
            unk  = PacketRead4 (p);
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

            IMSrvMsg (cont, event->conn, NOW, ExtraSet (ExtraSet (extra,
                      EXTRA_ORIGIN, EXTRA_ORIGIN_v5, NULL),
                      EXTRA_MESSAGE, msgtyp, msgtyp == MSG_NORM ?
                      ConvFromCont (ctext, cont) : c_in_to_split (ctext, cont)));
            Auto_Reply (event->conn, cont);
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
static JUMP_SNAC_F(SnacSrvSrvackmsg)
{
    Packet *pak;
    Contact *cont;
    /* UDWORD mid1, mid2; */
    UWORD type;

    pak = event->pak;

    /*mid1=*/PacketReadB4 (pak);
    /*mid2=*/PacketReadB4 (pak);
    type = PacketReadB2 (pak);

    cont = PacketReadCont (pak, event->conn);
    
    if (!cont)
        return;
    
    switch (type)
    {
        case 1:
        case 4:
            IMOffline (cont, event->conn);

            M_printf ("%s %s%*s%s ", s_now, COLCONTACT, uiG.nick_len + s_delta (cont->nick), cont->nick, COLNONE);
            M_print  (i18n (2126, "is offline, message queued on server.\n"));

/*          cont->status = STATUS_OFFLINE;
            putlog (event->conn, NOW, cont, STATUS_OFFLINE, LOG_ACK, 0xFFFF, 
                s_sprintf ("%08lx%08lx\n", mid1, mid2)); */
            break;
        case 2: /* msg was received by server */
            EventD (QueueDequeue (event->conn, QUEUE_TYPE2_RESEND_ACK, pak->ref));
            break;
    }
}

/*
 * SRV_REPLYBOS - SNAC(9,3)
 */
static JUMP_SNAC_F(SnacSrvReplybos)
{
    SnacCliSetuserinfo (event->conn);
    SnacCliSetstatus (event->conn, event->conn->status, 3);
    SnacCliReady (event->conn);
    SnacCliAddcontact (event->conn, 0);
    SnacCliReqofflinemsgs (event->conn);
}

/*
 * SRV_SETINTERVAL - SNAC(b,2)
 */
static JUMP_SNAC_F(SnacSrvSetinterval)
{
    Packet *pak;
    UWORD interval;

    pak = event->pak;
    interval = PacketReadB2 (pak);
    if (prG->verbose & DEB_PROTOCOL)
        M_printf (i18n (1918, "Ignored server request for a minimum report interval of %d.\n"), 
            interval);
}

/*
 * SRV_REPLYLISTS - SNAC(13,3)
 */
static JUMP_SNAC_F(SnacSrvReplylists)
{
    /* ignore */
}

static JUMP_SNAC_F(SnacSrvReplyrosterexport)
{
    Packet *pak;
    ContactGroup *cg = NULL;
    Contact *cont;
    strc_t cname;
    char *name, *nick;
    TLV *tlv;
    UWORD type, tag, id, TLVlen, j, data = 0;
    int cnt_sbl_add, cnt_sbl_chn, cnt_sbl_del;
    int i, k, l, count;
    
    pak = event->pak;
    cnt_sbl_add = cnt_sbl_chn = cnt_sbl_del = 0;
    count = PacketReadB2 (pak);
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
                if (id) /* should be zero for groups */
                    break;
                if (!tag) /* meta list of groups */
                    break;
                if (!(cg = ContactGroupFind (event->conn, tag, name)))
                    if (!(cg = ContactGroupFind (event->conn, tag, NULL)))
                        if (!(cg = ContactGroupFind (event->conn, 0, name)))
                            if (!IMROSTER_ISDOWN (data) || !(cg = ContactGroupC (event->conn, tag, name)))
                                break;
                if (IMROSTER_ISDOWN (data))
                {
                    s_repl (&cg->name, name);
                    cg->id = tag;
                }
                M_printf ("FIXME: Group #%08d '%s'\n", tag, name);
                break;
            case 2:
            case 3:
            case 14:
            case 0:
                cg = ContactGroupFind (event->conn, tag, NULL);
                if (!tag || (!cg && data == IMROSTER_IMPORT))
                    break;
                j = TLVGet (tlv, 305);
                assert (j < 200 || j == (UWORD)-1);
                nick = strdup (j != (UWORD)-1 ? ConvFromServ (&tlv[j].str) : name);

                switch (data)
                {
                    case 3:
                        if (j != (UWORD)-1 || !(cont = ContactFind (event->conn->contacts, 0, atoi (name), NULL)))
                        {
                            cont = ContactFindCreate (event->conn->contacts, id, atoi (name), nick);
                            SnacCliAddcontact (event->conn, cont);
                            k++;
                        }
                        if (!cont)
                            break;
                        if (!ContactFind (event->conn->contacts, 0, atoi (name), nick))
                            ContactFindCreate (event->conn->contacts, id, atoi (name), nick);
                        cont->id = id;   /* FIXME: should be in ContactGroup? */
                        if (type == 2)
                        {
                            ContactOptionsSetVal (&cont->copts, CO_INTIMATE, 1);
                            ContactOptionsSetVal (&cont->copts, CO_HIDEFROM, 0);
                        }
                        else if (type == 3)
                        {
                            ContactOptionsSetVal (&cont->copts, CO_HIDEFROM, 1);
                            ContactOptionsSetVal (&cont->copts, CO_INTIMATE, 0);
                        }
                        else if (type == 14)
                            ContactOptionsSetVal (&cont->copts, CO_IGNORE, 1);
                        if (!ContactFind (cg, 0, cont->uin, NULL))
                        {
                            ContactAdd (cg, cont);
                            l++;
                        }
                        M_printf (" #%d %10d %s\n", id, atoi (name), nick);
                        break;
                    case 2:
                        if ((cont = ContactFind (event->conn->contacts, id, atoi (name), nick)))
                            break;
                    case 1:
                        M_printf (" #%08d %10d %s\n", id, atoi (name), nick);
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
    /* TIMESTAMP ignored */
    if (k || l)
    {
        M_printf (i18n (2242, "Imported %d new contacts, added %d times to a contact group.\n"), k, l);
        if (event->conn->flags & CONN_WIZARD)
        {
            if (Save_RC () == -1)
                M_print (i18n (1679, "Sorry saving your personal reply messages went wrong!\n"));
        }
        else
            M_print (i18n (1754, "Note: You need to 'save' to write new contact list to disc.\n"));
    }
    SnacCliRosterack (event->conn);
}


static JUMP_SNAC_F(SnacSrvReplyroster)
{
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
    
    event2 = QueueDequeue (event->conn, QUEUE_REQUEST_ROSTER, 0);
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
                        if (!ContactGroupFind (event->conn, id, NULL))
                            if (IMROSTER_ISDOWN (data))
                                ContactGroupC (event->conn, id, s_sprintf ("<group #%d>", tag));
                    PacketD (p);
                }
                else
                {
                    if (!(cg = ContactGroupFind (event->conn, tag, name)))
                        if (!(cg = ContactGroupFind (event->conn, tag, NULL)))
                            if (!(cg = ContactGroupFind (event->conn, 0, name)))
                                if (!IMROSTER_ISDOWN (data) || !(cg = ContactGroupC (event->conn, tag, name)))
                                    break;
                    if (IMROSTER_ISDOWN (data))
                    {
                        s_repl (&cg->name, name);
                        cg->id = tag;
                    }
                    M_printf ("FIXME: Group #%08d '%s'\n", tag, name);
                }
                break;
            case 2:
            case 3:
            case 14:
            case 0:
                cg = ContactGroupFind (event->conn, tag, NULL);
                if (!tag || (!cg && data == IMROSTER_IMPORT))
                    break;
                if (!(cg = ContactGroupFind (event->conn, tag, NULL)))
                    if (!(cg = ContactGroupC (event->conn, tag, s_sprintf ("<group #%d>", tag))))
                        break;
                j = TLVGet (tlv, 305);
                assert (j < 200 || j == (UWORD)-1);
                nick = strdup (j != (UWORD)-1 && tlv[j].str.len ? ConvFromServ (&tlv[j].str) : name);

                switch (data)
                {
                    case 3:
                        if (j != (UWORD)-1 || !(cont = ContactFind (event->conn->contacts, 0, atoi (name), NULL)))
                        {
                            cont = ContactFindCreate (event->conn->contacts, id, atoi (name), nick);
                            SnacCliAddcontact (event->conn, cont);
                            k++;
                        }
                        if (!cont)
                            break;
                        if (!ContactFind (event->conn->contacts, 0, atoi (name), nick))
                            ContactFindCreate (event->conn->contacts, id, atoi (name), nick);
                        cont->id = id;   /* FIXME: should be in ContactGroup? */
                        if (type == 2)
                        {
                            ContactOptionsSetVal (&cont->copts, CO_INTIMATE, 1);
                            ContactOptionsSetVal (&cont->copts, CO_HIDEFROM, 0);
                        }
                        else if (type == 3)
                        {
                            ContactOptionsSetVal (&cont->copts, CO_HIDEFROM, 1);
                            ContactOptionsSetVal (&cont->copts, CO_INTIMATE, 0);
                        }
                        else if (type == 14)
                            ContactOptionsSetVal (&cont->copts, CO_IGNORE, 1);
                        if (!ContactFind (cg, 0, cont->uin, NULL))
                        {
                            ContactAdd (cg, cont);
                            l++;
                        }
                        M_printf (" #%d %10d %s\n", id, atoi (name), nick);
                        break;
                    case 2:
                        if ((cont = ContactFind (event->conn->contacts, id, atoi (name), nick)))
                            break;
                    case 1:
                        M_printf (" #%08d %10d %s\n", id, atoi (name), nick);
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
        M_printf (i18n (2242, "Imported %d new contacts, added %d times to a contact group.\n"), k, l);
        if (event->conn->flags & CONN_WIZARD)
        {
            if (Save_RC () == -1)
                M_print (i18n (1679, "Sorry saving your personal reply messages went wrong!\n"));
        }
        else
            M_print (i18n (1754, "Note: You need to 'save' to write new contact list to disc.\n"));
    }
    SnacCliRosterack (event->conn);
}

/*
 *
 */
static JUMP_SNAC_F(SnacSrvUpdateack)
{
    Contact *cont = NULL;
    Event *event2;
    UWORD err;
    
    event2 = QueueDequeue (event->conn, QUEUE_CHANGE_ROSTER, event->pak->ref);
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
                SnacCliRosteradd (event->conn, event->conn->contacts, cont);
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
    
    M_printf (i18n (2325, "Warning: server based contact list change failed with error code %d.\n"), err);
    if (event2)
        EventD (event2);
}

/*
 * SRV_AUTHREQ - SNAC(13,19)
 */
static JUMP_SNAC_F(SnacSrvAuthreq)
{
    Packet *pak;
    Contact *cont;
    strc_t ctext;
    char *text;

    pak   = event->pak;
    cont  = PacketReadCont (pak, event->conn);
    ctext = PacketReadB2Str (pak, NULL);
    
    if (!cont)
        return;
    
    text = strdup (c_in_to_split (ctext, cont));
    
    IMSrvMsg (cont, event->conn, NOW, ExtraSet (ExtraSet (NULL,
              EXTRA_ORIGIN, EXTRA_ORIGIN_v8, NULL),
              EXTRA_MESSAGE, MSG_AUTH_REQ, text));

    free (text);
}

/*
 * SRV_AUTHREPLY - SNAC(13,1b)
 */
static JUMP_SNAC_F(SnacSrvAuthreply)
{
    Packet *pak;
    Contact *cont;
    strc_t ctext;
    char *text;
    UBYTE acc;

    pak = event->pak;
    cont  = PacketReadCont (pak, event->conn);
    acc   = PacketRead1    (pak);
    ctext = PacketReadB2Str (pak, NULL);
    
    if (!cont)
        return;
    
    text = strdup (c_in_to_split (ctext, cont));

    IMSrvMsg (cont, event->conn, NOW, ExtraSet (ExtraSet (NULL,
              EXTRA_ORIGIN, EXTRA_ORIGIN_v8, NULL),
              EXTRA_MESSAGE, acc ? MSG_AUTH_GRANT : MSG_AUTH_DENY, text));

    free (text);
}

/*
 * SRV_ADDEDYOU - SNAC(13,1c)
 */
static JUMP_SNAC_F(SnacSrvAddedyou)
{
    Contact *cont;
    Packet *pak;

    pak = event->pak;
    cont = PacketReadCont (pak, event->conn);

    IMSrvMsg (cont, event->conn, NOW, ExtraSet (ExtraSet (NULL,
              EXTRA_ORIGIN, EXTRA_ORIGIN_v8, NULL),
              EXTRA_MESSAGE, MSG_AUTH_ADDED, ""));
}

/*
 * SRV_TOICQERR - SNAC(15,1)
 */
static JUMP_SNAC_F(SnacSrvToicqerr)
{
    Packet *pak = event->pak;
    if ((pak->ref & 0xffff) == 0x4231)
    {
        M_print (i18n (2206, "The server doesn't want to give us offline messages.\n"));
    }
    else
    {
        UWORD err = PacketReadB2 (pak);
        M_printf (i18n (2207, "Protocol error in command to old ICQ server: %d.\n"), err);
        M_print (s_dump (pak->data + pak->rpos, pak->len - pak->rpos));
    }
}

/*
 * SRV_FROMOLDICQ - SNAC(15,3)
 */
static JUMP_SNAC_F(SnacSrvFromicqsrv)
{
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
    if (uin != event->conn->uin)
    {
        if (prG->verbose & DEB_PROTOCOL)
        {
            M_printf (i18n (1919, "UIN mismatch: %ld vs %ld.\n"), event->conn->uin, uin);
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
                Recv_Message (event->conn, p);
            break;

        case 66:
            SnacCliAckofflinemsgs (event->conn);
            break;

        case 2010:
            Meta_User (event->conn, ContactUIN (event->conn, uin), p);
            break;

        default:
            SnacSrvUnknown (event);
            break;
    }
    PacketD (p);
}

/*
 * SRV_REGREFUSED - SNAC(17,1)
 */
static JUMP_SNAC_F(SnacSrvRegrefused)
{
    M_print (i18n (1920, "Registration of new UIN refused.\n"));
    if (event->conn->flags & CONN_WIZARD)
    {
        M_print (i18n (1792, "I'm sorry, AOL doesn't want to give us a new UIN, probably because of too many new UIN requests from this IP. Please try again later.\n"));
        exit (0);
    }
}

/*
 * SRV_NEWUIN - SNAC(17,5)
 */
static JUMP_SNAC_F(SnacSrvNewuin)
{
    Contact *cont;

    cont = ContactUIN (event->conn, PacketReadAt4 (event->pak, 6 + 10 + 46));
    event->conn->uin = event->conn->spref->uin = cont->uin;
    M_printf (i18n (1762, "Your new UIN is: %ld.\n"), cont->uin);
    if (event->conn->flags & CONN_WIZARD)
    {
        assert (event->conn->spref);
        assert (event->conn->assoc);
        assert (event->conn->assoc->spref);
        assert (event->conn->open);
        assert (event->conn->assoc->open);

        event->conn->spref->flags |= CONN_AUTOLOGIN;
        event->conn->assoc->spref->flags |= CONN_AUTOLOGIN;

        s_repl (&event->conn->contacts->name, s_sprintf ("contacts-icq8-%ld", cont->uin));
        M_print (i18n (1790, "Setup wizard finished. Congratulations to your new UIN!\n"));

        if (Save_RC () == -1)
            M_print (i18n (1679, "Sorry saving your personal reply messages went wrong!\n"));
        event->conn->assoc->open (event->conn->assoc);
        event->conn->open (event->conn);
    }
}

/*************************************/

/*
 * CLI_READY - SNAC(1,2)
 */
void SnacCliReady (Connection *conn)
{
    Packet *pak;
    SNAC *s;

    pak = SnacC (conn, 1, 2, 0, 0);
    
    for (s = SNACv; s->fam; s++)
    {
        if (s->fam == 12 || s->fam == 8)
            continue;

        PacketWriteB2 (pak, s->fam);
        PacketWriteB2 (pak, s->cmd);
        PacketWriteB4 (pak, s->fam == 2 ? 0x0101047B : 0x0110047B);
    }
    SnacSend (conn, pak);
}

/*
 * CLI_FAMILIES - SNAC(1,17)
 */
void SnacCliFamilies (Connection *conn)
{
    Packet *pak;
    SNAC *s;

    pak = SnacC (conn, 1, 0x17, 0, 0);
    
    for (s = SNACv; s->fam; s++)
    {
        if (s->fam == 12 || s->fam == 8)
            continue;

        PacketWriteB2 (pak, s->fam);
        PacketWriteB2 (pak, s->cmd);
    }
    SnacSend (conn, pak);
}

/*
 * CLI_SETSTATUS - SNAC(1,1E)
 *
 * action: 1 = send status 2 = send connection info (3 = both)
 */
void SnacCliSetstatus (Connection *conn, UDWORD status, UWORD action)
{
    Packet *pak;
    
    if (prG->flags & FLAG_WEBAWARE)
        status |= STATUSF_WEBAWARE;
    if (prG->flags & FLAG_DC_AUTH)
        status |= STATUSF_DC_AUTH;
    if (prG->flags & FLAG_DC_CONT)
        status |= STATUSF_DC_CONT;
    
    pak = SnacC (conn, 1, 0x1e, 0, 0);
    if ((action & 1) && (status & STATUSF_INV))
        SnacCliAddvisible (conn, 0);
    if (action & 1)
        PacketWriteTLV4 (pak, 6, status);
    if (action & 2)
    {
        PacketWriteB2 (pak, 0x0c); /* TLV 0C */
        PacketWriteB2 (pak, 0x25);
        PacketWriteB4 (pak, prG->flags & FLAG_HIDEIP ? 0 : conn->our_local_ip);
        if (conn->assoc && conn->assoc->connect & CONNECT_OK)
        {
            PacketWriteB4 (pak, conn->assoc->port);
            PacketWrite1  (pak, conn->assoc->status);
            PacketWriteB2 (pak, conn->assoc->ver);
            PacketWriteB4 (pak, conn->assoc->our_session);
        }
        else
        {
            PacketWriteB4 (pak, 0);
            PacketWrite1  (pak, 1);
            PacketWriteB2 (pak, 8);
            PacketWriteB4 (pak, 0);
        }
        PacketWriteB2 (pak, 0);
        PacketWriteB2 (pak, 80);
        PacketWriteB2 (pak, 0);
        PacketWriteB2 (pak, 3);
        PacketWriteB4 (pak, BUILD_MICQ);
        PacketWriteB4 (pak, BuildVersionNum);
        PacketWriteB4 (pak, BuildPlatformID);
        PacketWriteB2 (pak, 0);
        PacketWriteTLV2 (pak, 8, 0);
    }
    SnacSend (conn, pak);
    if ((action & 1) && !(status & STATUSF_INV))
        SnacCliAddinvis (conn, 0);
}

/*
 * CLI_RATESREQUEST - SNAC(1,6)
 */
void SnacCliRatesrequest (Connection *conn)
{
    SnacSend (conn, SnacC (conn, 1, 6, 0, 0));
}

/*
 * CLI_REQINFO - SNAC (1,E)
 */
void SnacCliReqinfo (Connection *conn)
{
    SnacSend (conn, SnacC (conn, 1, 0xE, 0, 0));
}

/*
 * CLI_REQLOCATION - SNAC(2,2)
 */
void SnacCliReqlocation (Connection *conn)
{
    SnacSend (conn, SnacC (conn, 2, 2, 0, 0));
}

/*
 * CLI_SETUSERINFO - SNAC(2,4)
 */
void SnacCliSetuserinfo (Connection *conn)
{
    Packet *pak;
    
    pak = SnacC (conn, 2, 4, 0, 0);
    PacketWriteTLV     (pak, 5);
    PacketWriteCapID   (pak, CAP_ISICQ);
    PacketWriteCapID   (pak, CAP_SRVRELAY);
    PacketWriteCapID   (pak, CAP_UTF8);
    PacketWriteCapID   (pak, CAP_MICQ);
    PacketWriteTLVDone (pak);
    SnacSend (conn, pak);
}

/*
 * CLI_REQBUDDY - SNAC(3,2)
 */
void SnacCliBuddy (Connection *conn)
{
    SnacSend (conn, SnacC (conn, 3, 2, 0, 0));
}

/*
 * CLI_ADDCONTACT - SNAC(3,4)
 */
void SnacCliAddcontact (Connection *conn, Contact *cont)
{
    ContactGroup *cg;
    Packet *pak;
    int i;
    
    cg = conn->contacts;
    pak = SnacC (conn, 3, 4, 0, 0);
    if (cont)
        PacketWriteCont (pak, cont);
    else
    {
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
        {
            if (i && !(i % 256))
            {
                SnacSend (conn, pak);
                pak = SnacC (conn, 3, 4, 0, 0);
            }
            PacketWriteCont (pak, cont);
            
        }
    }
    SnacSend (conn, pak);
}

/*
 * CLI_REMCONTACT - SNAC(3,5)
 */
void SnacCliRemcontact (Connection *conn, Contact *cont)
{
    Packet *pak;
    
    if (!cont)
        return;
    pak = SnacC (conn, 3, 5, 0, 0);
    PacketWriteCont (pak, cont);
    SnacSend (conn, pak);
}

/*
 * CLI_SETICBM - SNAC(4,2)
 */
void SnacCliSeticbm (Connection *conn)
{
   Packet *pak;
   
   pak = SnacC (conn, 4, 2, 0, 0);
   PacketWriteB2 (pak, 0);
   PacketWriteB2 (pak, 0);
   PacketWriteB2 (pak, 3);
   PacketWriteB2 (pak, 8000);
   PacketWriteB2 (pak, 999);
   PacketWriteB2 (pak, 999);
   PacketWriteB2 (pak, 0);
   PacketWriteB2 (pak, 0);
   SnacSend (conn, pak);
}
/*
 * CLI_REQICBM - SNAC(4,4)
 */
void SnacCliReqicbm (Connection *conn)
{
    SnacSend (conn, SnacC (conn, 4, 4, 0, 0));
}

/*
 * CLI_SENDMSG - SNAC(4,6)
 */
UBYTE SnacCliSendmsg (Connection *conn, Contact *cont, const char *text, UDWORD type, UBYTE format)
{
    Packet *pak;
    UDWORD mtime = rand() % 0xffff, mid = rand() % 0xffff;
    
    if (!cont)
        return RET_DEFER;
    
    if (format == 2 || type == MSG_GET_PEEK)
        return SnacCliSendmsg2 (conn, cont, ExtraSet (NULL, EXTRA_MESSAGE, type, text));
    
    IMIntMsg (cont, conn, NOW, STATUS_OFFLINE, INT_MSGACK_V8, text, NULL);
        
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
    
    pak = SnacC (conn, 4, 6, 0, 0);
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
            str_s bstr = { NULL, 0, 0 };

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

            str = ConvTo (text, enc);
            s_init (&bstr, "", str->len + 2);
            memcpy (bstr.txt, str->txt, str->len + 1);
            bstr.len = str->len;
            if (bstr.len > 450)
            {
                bstr.len = 450;
                bstr.txt[450] = '\0';
            }

            PacketWriteTLV     (pak, 2);
            PacketWriteTLV     (pak, 1281);
            if (icqenc == 0x20000)
                PacketWriteB2  (pak, 0x0106);
            else
                PacketWrite1   (pak, 0x01);
            PacketWriteTLVDone (pak);
            PacketWriteTLV     (pak, 257);
            PacketWriteB4      (pak, icqenc);
            PacketWriteData    (pak, bstr.txt, bstr.len);
            PacketWriteTLVDone (pak);
            PacketWriteTLVDone (pak);
            PacketWriteB2 (pak, 6);
            PacketWriteB2 (pak, 0);
            SnacSend (conn, pak);
            if (bstr.len == 450) /* FIXME - message splitting */
                return SnacCliSendmsg (conn, cont, text + 450, type, format);
            }
            break;
        case 4:
            remenc = ContactPrefVal (cont, CO_ENCODING);
            convtext = ConvTo (text, remenc)->txt;
            
            PacketWriteTLV     (pak, 5);
            PacketWrite4       (pak, conn->uin);
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
            SnacSend (conn, pak);
    }
    return RET_OK;
}

static void SnacCallbackType2Ack (Event *event)
{
    Contact *cont = event->cont;
    Connection *serv = event->conn;
    Event *aevent;

    if (!serv)
    {
        EventD (event);
        return;
    }
    aevent = QueueDequeue (serv, QUEUE_TYPE2_RESEND, ExtraGet (event->extra, EXTRA_REF));
    if (!aevent)
    {
        EventD (event);
        return;
    }
    ASSERT_SERVER (serv);
    
    IMCliMsg (serv, cont, aevent->extra);
    aevent->extra = NULL;
    EventD (aevent);
    EventD (event);
}

static void SnacCallbackType2 (Event *event)
{
    Contact *cont = event->cont;
    Connection *serv = event->conn;
    Packet *pak = event->pak;

    if (!serv || !cont)
    {
        if (!serv)
            M_printf (i18n (2234, "Message %s discarded - lost session.\n"), ExtraGetS (event->extra, EXTRA_MESSAGE));
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
                          ExtraGetS (event->extra, EXTRA_MESSAGE), NULL);
            SnacSend (serv, PacketClone (pak));
            event->attempts++;
            /* allow more time for the peer's ack than the server's ack */
            event->due = time (NULL) + RETRY_DELAY_TYPE2 + 5;
            QueueEnqueueData (serv, QUEUE_TYPE2_RESEND_ACK, pak->ref,
                              time (NULL) + RETRY_DELAY_TYPE2, NULL, cont,
                              ExtraSet (NULL, EXTRA_REF, event->seq, NULL),
                              &SnacCallbackType2Ack);
        }
        else
            event->due = time (NULL) + 1;
        QueueEnqueue (event);
        return;
    }
    
    IMCliMsg (serv, cont, event->extra);
    event->extra = NULL;
    EventD (event);
}

/*
 * CLI_SENDMSG - SNAC(4,6) - type2
 */
UBYTE SnacCliSendmsg2 (Connection *conn, Contact *cont, Extra *extra)
{
    Packet *pak;
    UDWORD mtime = rand() % 0xffff, mid = rand() % 0xffff;
    BOOL peek = 0;
    UDWORD type, e_trans;
    const char *text;
    
    text = ExtraGetS (extra, EXTRA_MESSAGE);
    type = ExtraGet  (extra, EXTRA_MESSAGE);

    if (type == MSG_GET_PEEK)
    {
        peek = 1;
        type = MSG_GET_AWAY;
    }
    
    if (!text || !cont || !(peek || (type & 0xff) == MSG_GET_VER || ExtraGet (extra, EXTRA_FORCE) ||
        (HAS_CAP (cont->caps, CAP_SRVRELAY) && HAS_CAP (cont->caps, CAP_ISICQ))))
        return RET_DEFER;
    
    if (!ExtraGet (extra, EXTRA_FORCE))
    {
        switch (type & 0xff)
        {
            case MSG_AUTO:
            case MSG_AUTH_REQ:
            case MSG_AUTH_GRANT:
            case MSG_AUTH_DENY:
            case MSG_AUTH_ADDED:
                return RET_DEFER;
        }
    }

    conn->our_seq_dc--;
    
    e_trans = ExtraGet (extra, EXTRA_TRANS) & ~EXTRA_TRANS_TYPE2;
    extra = ExtraSet (extra, EXTRA_TRANS, e_trans, NULL);

    pak = SnacC (conn, 4, 6, 0, peek ? 0x1771 : 0);
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
       PacketWrite2       (pak, conn->assoc && conn->assoc->connect & CONNECT_OK
                              ? conn->assoc->ver : 8);
       PacketWriteCapID   (pak, CAP_NONE);
       PacketWrite2       (pak, 0);
       PacketWrite4       (pak, 3);
       PacketWrite1       (pak, 0);
       PacketWrite2       (pak, conn->our_seq_dc);
      PacketWriteLenDone (pak);
      SrvMsgAdvanced     (pak, conn->our_seq_dc, type, conn->status, cont->status, -1, c_out_for (text, cont, type));
      PacketWrite4       (pak, TCP_COL_FG);
      PacketWrite4       (pak, TCP_COL_BG);
      if (CONT_UTF8 (cont, type))
          PacketWriteDLStr     (pak, CAP_GID_UTF8);
     PacketWriteTLVDone (pak);
     if (peek) /* make a syntax error */
         PacketWriteB4  (pak, 0x00030000);
    PacketWriteTLVDone (pak);
    PacketWriteB4      (pak, 0x00030000); /* empty TLV(3) */
    
    if (peek)
    {
        SnacSend (conn, pak);
        ExtraD (extra);
    }
    else
        QueueEnqueueData (conn, QUEUE_TYPE2_RESEND, conn->our_seq_dc,
                          time (NULL), pak, cont, extra, &SnacCallbackType2);
    return RET_INPR;
}

/*
 * CLI_REQBOS - SNAC(9,2)
 */
void SnacCliReqbos (Connection *conn)
{
    SnacSend (conn, SnacC (conn, 9, 2, 0, 0));
}

/*
 * CLI_ADDVISIBLE - SNAC(9,5)
 */
void SnacCliAddvisible (Connection *conn, Contact *cont)
{
    ContactGroup *cg;
    Packet *pak;
    int i;
    
    cg = conn->contacts;
    pak = SnacC (conn, 9, 5, 0, 0);
    if (cont)
        PacketWriteCont (pak, cont);
    else
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
            if (ContactPrefVal (cont, CO_INTIMATE))
                PacketWriteCont (pak, cont);
    SnacSend (conn, pak);
}

/*
 * CLI_REMVISIBLE - SNAC(9,6)
 */
void SnacCliRemvisible (Connection *conn, Contact *cont)
{
    Packet *pak;
    
    if (!cont)
        return;
    pak = SnacC (conn, 9, 6, 0, 0);
    PacketWriteCont (pak, cont);
    SnacSend (conn, pak);
}

/*
 * CLI_ADDINVIS - SNAC(9,7)
 */
void SnacCliAddinvis (Connection *conn, Contact *cont)
{
    ContactGroup *cg;
    Packet *pak;
    int i;
    
    pak = SnacC (conn, 9, 7, 0, 0);
    cg = conn->contacts;
    if (cont)
        PacketWriteCont (pak, cont);
    else
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
            if (ContactPrefVal (cont, CO_HIDEFROM))
                PacketWriteCont (pak, cont);
    SnacSend (conn, pak);
}

/*
 * CLI_REMINVIS - SNAC(9,8)
 */
void SnacCliReminvis (Connection *conn, Contact *cont)
{
    Packet *pak;
    
    if (!cont)
        return;
    pak = SnacC (conn, 9, 8, 0, 0);
    PacketWriteCont (pak, cont);
    SnacSend (conn, pak);
}

/*
 * CLI_REQLISTS - SNAC(13,2)
 */
void SnacCliReqlists (Connection *conn)
{
    Packet *pak;
    
    pak = SnacC (conn, 19, 2, 0, 0);
    SnacSend (conn, pak);
}

/*
 * CLI_REQROSTER - SNAC(13,4)
 */

void SnacCliReqroster (Connection *conn)
{
    Packet *pak;
    
    pak = SnacC (conn, 19, 4, 0, 0);
    SnacSend (conn, pak);
}

/*
 * CLI_CHECKROSTER - SNAC(13,5)
 */
void SnacCliCheckroster (Connection *conn)
{
    ContactGroup *cg;
/*    Contact *cont;*/
    Packet *pak;
    int i;
    
    cg = conn->contacts;
    pak = SnacC (conn, 19, 5, 0, 0);
    for (i = 0; (/*cont =*/ ContactIndex (cg, i)); )
        i++;

    PacketWriteB4 (pak, 0);  /* last modification of server side contact list */
    PacketWriteB2 (pak, i);  /* # of entries */
    SnacSend (conn, pak);
}

/*
 * CLI_ROSTERACK - SNAC(13,7)
 */
void SnacCliRosterack (Connection *conn)
{
    Packet *pak;
    
    pak = SnacC (conn, 19, 7, 0, 0);
    SnacSend (conn, pak);
}

/*
 * CLI_ADDBUDDY - SNAC(13,8)
 */
void SnacCliRosteradd (Connection *conn, ContactGroup *cg, Contact *cont)
{
    Packet *pak;
    
    if (cont)
    {
/*        SnacCliGrantauth (conn, cont);  */
        SnacCliAddstart (conn);
        
        pak = SnacC (conn, 19, 8, 0, 0);
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
        QueueEnqueueData (conn, QUEUE_CHANGE_ROSTER, pak->ref, 0x7fffffffL, NULL, cont, NULL, NULL);
    }
    else
    {
        pak = SnacC (conn, 19, 8, 0, 0);
        PacketWriteStrB     (pak, cg->name);
        PacketWriteB2       (pak, ContactGroupID (cg));
        PacketWriteB2       (pak, 0);
        PacketWriteB2       (pak, 1);
        PacketWriteBLen     (pak);
        PacketWriteBLenDone (pak);
    }
    SnacSend (conn, pak);
}

/*
 * CLI_ROSTERUPDATE - SNAC(12,9)
 */
void SnacCliRosterupdate (Connection *conn, ContactGroup *cg, Contact *cont)
{
    Packet *pak;
    int i;

    pak = SnacC (conn, 19, 9, 0, 0);
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
void SnacCliSetvisibility (Connection *conn)
{
    Packet *pak;
    
    pak = SnacC (conn, 19, 9, 0, 0);
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
    SnacSend (conn, pak);
}

/*
 * CLI_ROSTERDELETE - SANC(12,a)
 */
void SnacCliRosterdelete (Connection *conn, ContactGroup *cg, Contact *cont)
{
    Packet *pak;

    pak = SnacC (conn, 19, 10, 0, 0);
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
 * CLI_ADDSTART - SNAC(13,11)
 */
void SnacCliAddstart (Connection *conn)
{
    Packet *pak;
    
    pak = SnacC (conn, 19, 17, 0, 0);
    SnacSend (conn, pak);
}

/*
 * CLI_ADDEND - SNAC(13,12)
 */
void SnacCliAddend (Connection *conn)
{
    Packet *pak;

    pak = SnacC (conn, 19, 18, 0, 0);
    SnacSend (conn, pak);
}

/*
 * CLI_GRANTAUTH - SNAC(13,14)
 */
void SnacCliGrantauth (Connection *conn, Contact *cont)
{
    Packet *pak;
    
    if (!cont)
        return;
    pak = SnacC (conn, 19, 20, 0, 0);
    PacketWriteCont (pak, cont);
    PacketWrite4    (pak, 0);
    SnacSend (conn, pak);
}

/*
 * CLI_REQAUTH - SNAC(13,18)
 */
void SnacCliReqauth (Connection *conn, Contact *cont, const char *msg)
{
    Packet *pak;
    
    if (!cont)
        return;
    pak = SnacC (conn, 19, 24, 0, 0);
    PacketWriteCont (pak, cont);
    PacketWriteStrB (pak, c_out_to (msg, cont));
    PacketWrite2    (pak, 0);
    SnacSend (conn, pak);
}

/*
 * CLI_AUTHORIZE - SNAC(13,1a)
 */
void SnacCliAuthorize (Connection *conn, Contact *cont, BOOL accept, const char *msg)
{
    Packet *pak;
    
    if (!cont)
        return;
    pak = SnacC (conn, 19, 26, 0, 0);
    PacketWriteCont (pak, cont);
    PacketWrite1    (pak, accept ? 1 : 0);
    PacketWriteStrB (pak, accept ? "" : c_out_to (msg, cont));
    SnacSend (conn, pak);
}

/*
 * Create meta request package.
 */
static Packet *SnacMetaC (Connection *conn, UWORD sub, UWORD type, UWORD ref)
{
    Packet *pak;

    conn->our_seq3 = conn->our_seq3 ? (conn->our_seq3 + 1) % 0x7fff : 2;
    
    pak = SnacC (conn, 21, 2, 0, (ref ? ref : rand () % 0x7fff) + (conn->our_seq3 << 16));
    PacketWriteTLV (pak, 1);
    PacketWriteLen (pak);
    PacketWrite4   (pak, conn->uin);
    PacketWrite2   (pak, sub);
    PacketWrite2   (pak, conn->our_seq3);
    if (type)
        PacketWrite2 (pak, type);

    return pak;
}

/*
 * Complete & send meta request package.
 */
static void SnacMetaSend (Connection *conn, Packet *pak)
{
    PacketWriteLenDone (pak);
    PacketWriteTLVDone (pak);
    SnacSend (conn, pak);
}

/*
 * CLI_REQOFFLINEMSGS - SNAC(15,2) - 60
 */
void SnacCliReqofflinemsgs (Connection *conn)
{
    Packet *pak;

    pak = SnacMetaC (conn, 60, 0, 0x4231);
    SnacMetaSend    (conn, pak);
}

/*
 * CLI_ACKOFFLINEMSGS - SNAC(15,2) - 62
 */
void SnacCliAckofflinemsgs (Connection *conn)
{
    Packet *pak;

    pak = SnacMetaC (conn, 62, 0, 0);
    SnacMetaSend    (conn, pak);
}

/*
 * CLI_METASETGENERAL - SNAC(15,2) - 2000/1002
 */
void SnacCliMetasetgeneral (Connection *conn, Contact *cont)
{
    Packet *pak;

    pak = SnacMetaC (conn, 2000, META_SET_GENERAL_INFO, 0);
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
    SnacMetaSend    (conn, pak);
}

/*
 * CLI_METASETABOUT - SNAC(15,2) - 2000/1030
 */
void SnacCliMetasetabout (Connection *conn, const char *text)
{
    Packet *pak;

    pak = SnacMetaC (conn, 2000, META_SET_ABOUT_INFO, 0);
    PacketWriteLNTS (pak, c_out (text));
    SnacMetaSend    (conn, pak);
}

/*
 * CLI_METASETMORE - SNAC(15,2) - 2000/1021
 */
void SnacCliMetasetmore (Connection *conn, Contact *cont)
{
    Packet *pak;
    
    pak = SnacMetaC (conn, 2000, META_SET_MORE_INFO, 0);
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
    SnacMetaSend    (conn, pak);
}

/*
 * CLI_METASETPASS - SNAC(15,2) - 2000/1070
 */
void SnacCliMetasetpass (Connection *conn, const char *newpass)
{
    Packet *pak;
    
    pak = SnacMetaC (conn, 2000, 1070, 0);
    PacketWriteLNTS (pak, c_out (newpass));
    SnacMetaSend    (conn, pak);
}

/*
 * CLI_METAREQINFO - SNAC(15,2) - 2000/1202
 */
UDWORD SnacCliMetareqmoreinfo (Connection *conn, Contact *cont)
{
    Packet *pak;
    UDWORD ref;

    pak = SnacMetaC (conn, 2000, 1202, 0);
    ref = pak->ref;
    PacketWrite4    (pak, cont->uin);
    SnacMetaSend    (conn, pak);
    return ref;
}

/*
 * CLI_METAREQINFO - SNAC(15,2) - 2000/1232
 */
UDWORD SnacCliMetareqinfo (Connection *conn, Contact *cont)
{
    Packet *pak;
    UDWORD ref;

    pak = SnacMetaC (conn, 2000, META_REQ_INFO, 0);
    ref = pak->ref;
    PacketWrite4    (pak, cont->uin);
    SnacMetaSend    (conn, pak);
    return ref;
}

/*
 * CLI_SEARCHBYPERSINF - SNAC(15,2) - 2000/1375
 */
void SnacCliSearchbypersinf (Connection *conn, const char *email, const char *nick, const char *name, const char *surname)
{
    Packet *pak;

    pak = SnacMetaC  (conn, 2000, META_SEARCH_PERSINFO, 0);
    PacketWrite2     (pak, 320); /* key: first name */
    PacketWriteLLNTS (pak, c_out (name));
    PacketWrite2     (pak, 330); /* key: last name */
    PacketWriteLLNTS (pak, c_out (surname));
    PacketWrite2     (pak, 340); /* key: nick */
    PacketWriteLLNTS (pak, c_out (nick));
    PacketWrite2     (pak, 350); /* key: email address */
    PacketWriteLLNTS (pak, c_out (email));
    SnacMetaSend     (conn, pak);
}

/*
 * CLI_SEARCHBYMAIL - SNAC(15,2) - 2000/{1395,1321}
 */
void SnacCliSearchbymail (Connection *conn, const char *email)
{
    Packet *pak;

    pak = SnacMetaC  (conn, 2000, META_SEARCH_EMAIL, 0);
    PacketWrite2     (pak, 350); /* key: email address */
    PacketWriteLLNTS (pak, c_out (email));
    SnacMetaSend     (conn, pak);
}

/*
 * CLI_SEARCHRANDOM - SNAC(15,2) - 2000/1870
 */
UDWORD SnacCliSearchrandom (Connection *conn, UWORD group)
{
    Packet *pak;
    UDWORD ref;

    pak = SnacMetaC (conn, 2000, META_SEARCH_RANDOM, 0);
    ref = pak->ref;
    PacketWrite2    (pak, group);
    SnacMetaSend    (conn, pak);
    return ref;
}

/*
 * CLI_SETRANDOM - SNAC(15,2) - 2000/1880
 */
void SnacCliSetrandom (Connection *conn, UWORD group)
{
    Packet *pak;

    pak = SnacMetaC (conn, 2000, META_SET_RANDOM, conn->connect & CONNECT_OK ? 0 : 0x4242);
    PacketWrite2    (pak, group);
    if (group)
    {
        PacketWriteB4 (pak, 0x00000220);
        PacketWriteB4 (pak, 0);
        PacketWriteB4 (pak, 0);
        PacketWriteB4 (pak, 0);
        PacketWrite1  (pak, conn->assoc && conn->assoc->connect & CONNECT_OK
                            ? conn->assoc->status : 0);
        PacketWrite2  (pak, conn->assoc && conn->assoc->connect & CONNECT_OK
                            ? conn->assoc->ver : 0);
        PacketWriteB4 (pak, 0);
        PacketWriteB4 (pak, 0x00005000);
        PacketWriteB4 (pak, 0x00000300);
        PacketWrite2  (pak, 0);
    }
    SnacMetaSend (conn, pak);
}

/*
 * CLI_SEARCHWP - SNAC(15,2) - 2000/1331
 */
void SnacCliSearchwp (Connection *conn, const MetaWP *wp)
{
    Packet *pak;

    pak = SnacMetaC (conn, 2000, META_SEARCH_WP, 0);
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
    SnacMetaSend (conn, pak);
}

/*
 * CLI_SENDSMS - SNAC(15,2) - 2000/5250
 */
void SnacCliSendsms (Connection *conn, const char *target, const char *text)
{
    Packet *pak;
    char buf[2000], tbuf[100];
    time_t t = time (NULL);

    strftime (tbuf, sizeof (tbuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime (&t));
    snprintf (buf, sizeof (buf), "<icq_sms_message><destination>%s</destination>"
             "<text>%s (%ld www.mICQ.org)</text><codepage>1252</codepage><senders_UIN>%ld</senders_UIN>"
             "<senders_name>%s</senders_name><delivery_receipt>Yes</delivery_receipt>"
             "<time>%s</time></icq_sms_message>",
             target, text, conn->uin, conn->uin, "mICQ", tbuf);

    pak = SnacMetaC (conn, 2000, META_SEND_SMS, 0);
    PacketWriteB2      (pak, 1);
    PacketWriteB2      (pak, 22);
    PacketWriteB4      (pak, 0);
    PacketWriteB4      (pak, 0);
    PacketWriteB4      (pak, 0);
    PacketWriteB4      (pak, 0);
    PacketWriteTLVStr  (pak, 0, buf);
    SnacMetaSend (conn, pak);
}

/*
 * CLI_REGISTERUSER - SNAC(17,4)
 */
#define _REG_X1 0x28000300
#define _REG_X2 0x8a4c0000
#define _REG_X3 0x00000602
#define REG_X1 0x28000300
#define REG_X2 0x9e270000
#define REG_X3 0x00000302
void SnacCliRegisteruser (Connection *conn)
{
    Packet *pak;
    
    pak = SnacC (conn, 23, 4, 0, 0);
    PacketWriteTLV (pak, 1);
    PacketWriteB4 (pak, 0);
    PacketWriteB4 (pak, REG_X1);
    PacketWriteB4 (pak, 0);
    PacketWriteB4 (pak, 0);
    PacketWriteB4 (pak, REG_X2);
    PacketWriteB4 (pak, REG_X2);
    PacketWriteB4 (pak, 0);
    PacketWriteB4 (pak, 0);
    PacketWriteB4 (pak, 0);
    PacketWriteB4 (pak, 0);
    PacketWriteLNTS (pak, c_out (conn->passwd));
    PacketWriteB4 (pak, REG_X2);
    PacketWriteB4 (pak, REG_X3);
    PacketWriteTLVDone (pak);
    SnacSend (conn, pak);
}
