
/*
 * Handles incoming and creates outgoing SNAC packets.
 *
 * This file is Copyright © Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 */

#include "micq.h"
#include "util.h"
#include "util_ui.h"
#include "util_io.h"
#include "contact.h"
#include "server.h"
#include "packet.h"
#include "session.h"
#include "preferences.h"
#include "icq_response.h"
#include "cmd_pkt_cmd_v5_util.h"
#include "cmd_pkt_v8_snac.h"
#include "cmd_pkt_v8_flap.h"
#include "cmd_pkt_v8_tlv.h"
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
    SnacSrvReplyinfo, SnacSrvReplylocation, SnacSrvUseronline, SnacSrvRegrefused,
    SnacSrvUseroffline, SnacSrvRecvmsg, SnacSrvUnknown, SnacSrvFromoldicq,
    SnacSrvAddedyou, SnacSrvNewuin, SnacSrvSetinterval, SnacSrvAckmsg,
    SnacSrvAuthreq, SnacSrvAuthreply, SnacSrvIcbmerr, SnacSrvReplyroster;

static SNAC SNACv[] = {
    {  1,  3, NULL, NULL},
    { 19,  2, NULL, NULL},
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
    {  1, 15, "SRV_REPLYINFO",       SnacSrvReplyinfo},
    {  1, 19, "SRV_MOTD",            SnacSrvMotd},
    {  1, 24, "SRV_FAMILIES2",       SnacSrvFamilies2},
    {  2,  3, "SRV_REPLYLOCATION",   SnacSrvReplylocation},
    {  3,  3, "SRV_REPLYBUDDY",      SnacSrvReplybuddy},
    {  3, 11, "SRV_USERONLINE",      SnacSrvUseronline},
    {  3, 12, "SRV_USEROFFLINE",     SnacSrvUseroffline},
    {  4,  1, "SRV_ICBMERR",         SnacSrvIcbmerr},
    {  4,  5, "SRV_REPLYICBM",       SnacSrvReplyicbm},
    {  4,  7, "SRV_RECVMSG",         SnacSrvRecvmsg},
    {  4, 12, "SRV_ACKMSG",          SnacSrvAckmsg},
    {  9,  3, "SRV_REPLYBOS",        SnacSrvReplybos},
    { 11,  2, "SRV_SETINTERVAL",     SnacSrvSetinterval},
    { 19,  3, "SRV_REPLYUNKNOWN",    SnacSrvUnknown},
    { 19,  6, "SRV_REPLYROSTER",     SnacSrvReplyroster},
    { 19, 14, "SRV_UPDATEACK",       SnacSrvUnknown},
    { 19, 15, "SRV_REPLYROSTEROK",   SnacSrvUnknown},
    { 19, 25, "SRV_AUTHREQ",         SnacSrvAuthreq},
    { 19, 27, "SRV_AUTHREPLY",       SnacSrvAuthreply},
    { 19, 28, "SRV_ADDEDYOU",        SnacSrvAddedyou},
    { 21,  3, "SRV_FROMOLDICQ",      SnacSrvFromoldicq},
    { 23,  1, "SRV_REGREFUSED",      SnacSrvRegrefused},
    { 23,  5, "SRV_NEWUIN",          SnacSrvNewuin},
    {  1,  2, "CLI_READY",           NULL},
    {  1,  6, "CLI_RATESREQUEST",    NULL},
    {  1,  8, "CLI_ACKRATES",        NULL},
    {  1, 14, "CLI_REQINFO",         NULL},
    {  1, 23, "CLI_FAMILIES",        NULL},
    {  1, 30, "CLI_SETSTATUS",       NULL},
    {  2,  2, "CLI_REQLOCATION",     NULL},
    {  2,  4, "CLI_SETUSERINFO",     NULL},
    {  3,  2, "CLI_REQBUDDY",        NULL},
    {  3,  4, "CLI_ADDCONTACT",      NULL},
    {  3,  5, "CLI_RREMCONTACT",     NULL},
    {  4,  2, "CLI_SETICBM",         NULL},
    {  4,  4, "CLI_REQICBM",         NULL},
    {  4,  6, "CLI_SENDMSG",         NULL},
    {  9,  2, "CLI_REQBOS",          NULL},
    {  9,  5, "CLI_ADDVISIBLE",      NULL},
    {  9,  6, "CLI_REMVISIBLE",      NULL},
    {  9,  7, "CLI_ADDINVIS",        NULL},
    {  9,  8, "CLI_REMINVIS",        NULL},
    { 19,  2, "CLI_REQUNKNOWN",      NULL},
    { 19,  5, "CLI_REQROSTER",       NULL},
    { 19,  7, "CLI_UNKNOWN1",        NULL},
    { 19,  8, "CLI_ADDBUDDY",        NULL},
    { 19,  9, "CLI_UPDATEGROUP",     NULL},
    { 19, 17, "CLI_ADDSTART",        NULL},
    { 19, 18, "CLI_ADDREND",         NULL},
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
void SrvCallBackSnac (Event *event)
{
    Packet  *pak  = event->pak;
    SNAC *s;
    UWORD family;
    
    family     = PacketReadB2 (pak);
    pak->cmd   = PacketReadB2 (pak);
    pak->flags = PacketReadB2 (pak);
    pak->id    = PacketReadB4 (pak);
    
    for (s = SNACS; s->fam; s++)
        if (s->fam == family && s->cmd == pak->cmd)
            break;
    
    if (s->fam)
        s->f (event);
    else
        SnacSrvUnknown (event);

    PacketD (pak);
    free (event);
}

/*
 * Keeps track of sending a keep alive every 30 seconds.
 */
void SrvCallBackKeepalive (Event *event)
{
    if (event->sess && event->sess->connect & CONNECT_OK)
    {
        FlapCliKeepalive (event->sess);
        event->due = time (NULL) + 30;
        QueueEnqueue (event);
        return;
    }
    free (event);
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
Packet *SnacC (Session *sess, UWORD fam, UWORD cmd, UWORD flags, UDWORD ref)
{
    Packet *pak;
    
    if (!sess->our_seq2)
        sess->our_seq2 = rand () & 0x7fff;
    
    pak = FlapC (2);
    PacketWriteB2 (pak, fam);
    PacketWriteB2 (pak, cmd);
    PacketWriteB2 (pak, flags);
    PacketWriteB4 (pak, ref /*? ref : sess->our_seq++*/);
    return pak;
}

/*
 * Any unknown SNAC.
 */
static JUMP_SNAC_F(SnacSrvUnknown)
{
    if (!(prG->verbose & DEB_PACK8))
    {
        Time_Stamp ();
        M_print (" " COLINDENT COLSERVER "%s ", i18n (1033, "Incoming v8 server packet:"));
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
            M_print (i18n (1899, "Unknown family requested: %d\n"), fam);
            continue;
        }
    }
    SnacCliFamilies (event->sess);
}

/*
 * SRV_RATES - SNAC(1,7)
 * CLI_ACKRATES - SNAC(1,8)
 */
static JUMP_SNAC_F(SnacSrvRates)
{
    UWORD nr, grp;
    Packet *pak;
    
    pak = SnacC (event->sess, 1, 8, 0, 0);
    nr = PacketReadB2 (event->pak); /* ignore the remainder */
    while (nr--)
    {
        grp = PacketReadB2 (event->pak);
        event->pak->rpos += 33;
        PacketWriteB2 (pak, grp);
    }
    SnacSend (event->sess, pak);
}

/*
 * SRV_REPLYINFO - SNAC(1,15)
 */
static JUMP_SNAC_F(SnacSrvReplyinfo)
{
    Packet *pak;
    TLV *tlv;
    UDWORD uin, status;
    
    pak = event->pak;
    if (pak->flags & 0x8000)
    {
        PacketReadB4 (pak);
        PacketReadB4 (pak);
    }
    uin = PacketReadUIN (pak);
    
    if (uin != event->sess->uin)
        M_print (i18n (1907, "Warning: Server thinks our UIN is %d, while it is %d.\n"),
                 uin, event->sess->uin);
    PacketReadB2 (pak);
    PacketReadB2 (pak);
    tlv = TLVRead (pak, PacketReadLeft (pak));
    if (tlv[10].len)
    {
        event->sess->our_outside_ip = tlv[10].nr;
        if (prG->verbose)
            M_print (i18n (1915, "Server says we're at %s.\n"), UtilIOIP (event->sess->our_outside_ip));
        if (event->sess->assoc)
            event->sess->assoc->our_outside_ip = event->sess->our_outside_ip;
    }
    if (tlv[6].len)
    {
        status = tlv[6].nr;
        if (status != event->sess->status)
        {
            event->sess->status = status;
            Time_Stamp ();
            M_print (" ");
            Print_Status (event->sess->status);
            M_print ("\n");
        }
    }
    /* TLV 1 c f 2 3 ignored */
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
            M_print (i18n (1904, "Server doesn't understand ver %d (only %d) for family %d!\n"), s->cmd, ver, fam);
    }
}

/*
 * SRV_MOTD - SNAC(1,13)
 */
static JUMP_SNAC_F(SnacSrvMotd)
{
    if (!(event->sess->connect & CONNECT_OK))
        event->sess->connect++;

    SnacCliRatesrequest (event->sess);
    SnacCliReqinfo      (event->sess);
    if (event->sess->flags & CONN_WIZARD)
        SnacCliReqroster (event->sess);
    SnacCliReqlocation  (event->sess);
    SnacCliBuddy        (event->sess);
    SnacCliReqicbm      (event->sess);
    SnacCliReqbos       (event->sess);
}

/*
 * SRV_REPLYLOCATION - SNAC(2,3)
 */
static JUMP_SNAC_F(SnacSrvReplylocation)
{
    /* ignore all data, do nothing */
}

/*
 * SRV_REPLYBUDDY - SNAC(3,3)
 */
static JUMP_SNAC_F(SnacSrvReplybuddy)
{
    /* ignore all data, do nothing */
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
    cont = ContactFind (PacketReadUIN (pak));
    if (!cont)
    {
        if (prG->verbose & DEB_PROTOCOL)
            M_print (i18n (1908, "Received USERONLINE packet for non-contact.\n"));
        return;
    }
    
    PacketReadB2 (pak);
    PacketReadB2 (pak);
    tlv = TLVRead (pak, PacketReadLeft (pak));
    if (tlv[10].len)
        cont->outside_ip = tlv[10].nr;
    
    if (tlv[12].len)
    {
        UDWORD ip;
        p = TLVPak (tlv + 12);
        
                           ip = PacketReadB4 (p);
        cont->port            = PacketReadB4 (p);
        cont->connection_type = PacketRead1 (p);
        cont->TCP_version     = PacketReadB2 (p);
        cont->cookie          = PacketReadB4 (p);
        if (ip)
            cont->local_ip = ip;
        PacketReadB4 (p);
        PacketReadB4 (p);
        cont->id1 = PacketReadB4 (p);
        cont->id2 = PacketReadB4 (p);
        cont->id3 = PacketReadB4 (p);
        /* remainder ignored */
        PacketD (p);
    }
    /* TLV 1, d, f, 2, 3 ignored */

    UtilUIUserOnline (event->sess, cont, tlv[6].len ? tlv[6].nr : 0);
}

/*
 * SRV_USEROFFLINE - SNAC(3,C)
 */
static JUMP_SNAC_F(SnacSrvUseroffline)
{
    Contact *cont;
    Packet *pak;
    
    pak = event->pak;
    cont = ContactFind (PacketReadUIN (pak));
    if (!cont)
    {
        if (prG->verbose & DEB_PROTOCOL)
            M_print (i18n (1909, "Received USEROFFLINE packet for non-contact.\n"));
        return;
    }

    cont->status = STATUS_OFFLINE;
    
    UtilUIUserOffline (event->sess, cont);
}

/*
 * SRV_ICBMERR - SNAC(4,1)
 */
static JUMP_SNAC_F(SnacSrvIcbmerr)
{
    if (event->pak->id == 0x1771)
    {
        UWORD err = PacketReadB2 (event->pak);
        if (err == 0xe)
            M_print (i18n (2017, "The user is actually online.\n"));
        else if (err == 0x4)
            M_print (i18n (2022, "The user is indeed offline.\n"));
    }
}

/*
 * SRV_REPLYICBM - SNAC(4,5)
 */
static JUMP_SNAC_F(SnacSrvReplyicbm)
{
   SnacCliSeticbm (event->sess);
}

/*
 * SRV_RECVMSG - SNAC(4,7)
 */
static JUMP_SNAC_F(SnacSrvRecvmsg)
{
    Contact *cont;
    Packet *p = NULL, *pp = NULL, *pak;
    TLV *tlv;
    UDWORD uin;
    int i, type, t;
    char *text = NULL;
    const char *txt = NULL;

    pak = event->pak;

           PacketReadB4 (pak); /* TIME */
           PacketReadB2 (pak); /* RANDOM */
           PacketReadB2 (pak); /* WARN */
    type = PacketReadB2 (pak);
    uin =  PacketReadUIN (pak);
           PacketReadB2 (pak); /* WARNING */
           PacketReadB2 (pak); /* COUNT */
    
    UtilCheckUIN (event->sess, uin);
    cont = ContactFind (uin);
    tlv = TLVRead (pak, PacketReadLeft (pak));

    if (tlv[6].len && cont && cont->status != STATUS_OFFLINE)
        UtilUIUserOnline (event->sess, cont, tlv[6].nr);

    /* tlv[2] may be there twice - ignore the member since time(NULL). */
    if (tlv[2].len == 4)
        TLVDone (tlv, 2);

    switch (type)
    {
        case 1:
            if (!tlv[2].len)
            {
                SnacSrvUnknown (event);
                return;
            }

            p = TLVPak (tlv + 2);
            PacketReadB2 (p);
            PacketReadData (p, NULL, PacketReadB2 (p));
            PacketReadB2 (p);
            text = PacketReadStrB (p);
            txt = text + 4;
            type = NORM_MESS;
            /* TLV 1, 2(!), 3, 4, f ignored */
            break;
        case 2:
            p = TLVPak (tlv + 5);
            type = PacketReadB2 (p); /* ACKTYPE */
                   PacketReadB4 (p); /* TIME */
                   PacketReadB2 (p); /* RANDOM */
                   PacketReadB2 (p); /* UNKNOWN */
                   PacketReadB4 (p); PacketReadB4 (p); PacketReadB4 (p); PacketReadB4 (p); /* CAP */
            tlv = TLVRead (pak, PacketReadLeft (pak));
            for (i = 16; i < 20; i++)
                if (tlv[i].nr == 0x2711)
                    break;
            if (i == 20)
            {
                SnacSrvUnknown (event);
                return;
            }
            pp = TLVPak (tlv + i);
            if (PacketRead1 (pp) != 0x1b)
            {
                SnacSrvUnknown (event);
                return;
            }
            t = PacketReadB2 (pp);
            if (cont)
                cont->TCP_version = t;
            PacketRead1 (pp); /* UNKNOWN */
            PacketRead4 (pp); PacketRead4 (pp); PacketRead4 (pp); PacketRead4 (pp); /* CAP */
            PacketRead1 (pp); PacketRead2 (pp); /* UNKNOWN */
            PacketReadB4 (pp); /* UNKNOWN */
            PacketReadB2 (pp); /* SEQ1 */
            PacketReadB2 (pp); /* UNKNOWN */
            PacketReadB2 (pp); /* SEQ2 */
            PacketRead4 (pp); PacketRead4 (pp); PacketRead4 (pp); /* UNKNOWN */
            type = PacketRead1 (pp);
                   PacketRead1 (pp); /* FLAGS */
            PacketReadB2 (pp); /* UNKNOWN */
            PacketReadB2 (pp); /* UNKNOWN */
            txt = text = PacketReadLNTS (pp);
            /* FOREGROUND / BACKGROUND ignored */
            /* TLV 1, 2(!), 3, 4, f ignored */
            break;
        case 4:
            p = TLVPak (tlv + 5);
            uin  = PacketRead4 (p);
            type = PacketRead1 (p);
                   PacketRead1 (p);
            txt = text = PacketReadLNTS (p);
            /* FOREGROUND / BACKGROUND ignored */
            /* TLV 1, 2(!), 3, 4, f ignored */
            break;
        default:
            SnacSrvUnknown (event);
            return;
    }

    Do_Msg (event->sess, NULL, type, txt, uin, tlv[6].len ? tlv[6].nr : STATUS_OFFLINE, 0);
    Auto_Reply (event->sess, uin);

    if (text)
        free (text);
    
    if (pp)
        PacketD (pp);
    
    if (p)
        PacketD (p);

    if (prG->sound & SFLAG_CMD)
        ExecScript (prG->sound_cmd, uin, 0, NULL);
    else if (prG->sound & SFLAG_BEEP)
        printf ("\a");
}

/*
 * SRV_ACKMSG - SNAC(4,C)
 */
static JUMP_SNAC_F(SnacSrvAckmsg)
{
    Packet *pak;
    UDWORD uin, mid1, mid2;
/*  UWORD vers; */

    pak = event->pak;

    mid1 = PacketReadB4 (pak);
    mid2 = PacketReadB4 (pak);
/*  vers=*/PacketReadB2 (pak);

    uin = PacketReadUIN (pak);
    Time_Stamp ();
    M_print (" ");
    M_print (i18n (1913, "Received server acknowledge for %s#%08lx:%08lx%s sent to %s%s%s.\n"),
             COLSERVER, mid1, mid2, COLNONE, COLCONTACT, ContactFindName (uin), COLNONE);
    log_event (uin, LOG_EVENT, "Received ACK for #%08lx%08lx to %s\n",
               mid1, mid2, ContactFindName (uin));
}

/*
 * SRV_REPLYBOS - SNAC(9,3)
 */
static JUMP_SNAC_F(SnacSrvReplybos)
{
    SnacCliSetuserinfo (event->sess);
    SnacCliSetstatus (event->sess, event->sess->spref->status, 3);
    SnacCliReady (event->sess);
    SnacCliAddcontact (event->sess, 0);
    SnacCliReqofflinemsgs (event->sess);
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
        M_print (i18n (1918, "Ignored server request for a minimum report interval of %d.\n"), 
            interval);
}

/*
 * SRV_REPLYROSTER - SNAC(13,6)
 */
static JUMP_SNAC_F(SnacSrvReplyroster)
{
    Packet *pak;
    TLV *tlv;
    Event *event2;

    int i, k;
    char *name, *nick;
    UWORD count, type, tag, id, TLVlen, j, data;

    pak = event->pak;
    if (pak->flags & 0x8000)
    {
        PacketReadB4 (pak);
        PacketReadB4 (pak);
    }
    
    event2 = QueueDequeue (0, QUEUE_REQUEST_ROSTER);
    if (event2)
    {
        data = event2->uin;
        free (event2);
    }
    else
        data = 1;
        

    PacketRead1 (pak);
    count = PacketReadB2 (pak);          /* COUNT */
    for (i = k = 0; i < count; i++)
    {
        name   = PacketReadStrB (pak);   /* GROUP NAME */
        tag    = PacketReadB2 (pak);     /* TAG  */
        id     = PacketReadB2 (pak);     /* ID   */
        type   = PacketReadB2 (pak);     /* TYPE */
        TLVlen = PacketReadB2 (pak);     /* TLV length */
        tlv    = TLVRead (pak, TLVlen);

        switch (type)
        {
            case 1:
                if (!tag && !id)
                    break;
                M_print (i18n (2049, "Receiving group \"%s\":\n"), name);
                break;
            case 0:
                if (!tag)
                    break;
                j = TLVGet (tlv, 305);
                assert (j < 200 || j == (UWORD)-1);
                nick = strdup (j != (UWORD)-1 ? tlv[j].str : name);
                ConvWinUnix (nick);
                   
                switch (data)
                {
                    case 3:
                        if (ContactFindAlias (atoi (name), nick))
                            break;
                        ContactAdd (atoi (name), nick);
                        k++;
                        M_print ("  %10d %s\n", atoi (name), nick);
                        break;
                    case 2:
                        if (ContactFindAlias (atoi (name), nick))
                            break;
                    case 1:
                        M_print ("  %10d %s\n", atoi (name), nick);
                }
                free (nick);
                break;
            case 4:
            case 9:
            case 17:
                /* unknown / ignored */
        }
        free (name);
        TLVD (tlv);
    }
    /* TIMESTAMP ignored */
    if (k)
    {
        M_print (i18n (2050, "Imported %d contacts.\n"), k);
        if (event->sess->flags & CONN_WIZARD)
        {
            if (Save_RC () == -1)
                M_print (i18n (1679, "Sorry saving your personal reply messages went wrong!\n"));
        }
        else
            M_print (i18n (1754, " Note: You need to 'save' to write new contact list to disc.\n"));
    }
}

/*
 * SRV_AUTHREQ - SNAC(13,19)
 */
static JUMP_SNAC_F(SnacSrvAuthreq)
{
    Packet *pak;
    UDWORD uin;
    char *text;

    pak = event->pak;
    PacketReadData (pak, NULL, PacketReadB2 (pak));
    /* TLV 1 ignored */
    uin = PacketReadUIN (pak);
    text = PacketReadStrB (pak);
    Time_Stamp ();
    M_print (i18n (1590, COLCONTACT "%10s" COLNONE " has requested your authorization to be added to their contact list.\n"),
             ContactFindName (uin));
    M_print ("%-15s " COLMESSAGE "%s" COLNONE "\n", i18n (1591, "Reason:"), text);
    free (text);
    if (prG->sound & SFLAG_CMD)
        ExecScript (prG->sound_cmd, uin, 0, NULL);
    else if (prG->sound & SFLAG_BEEP)
        printf ("\a");
}

/*
 * SRV_AUTHREPLY - SNAC(13,1b)
 */
static JUMP_SNAC_F(SnacSrvAuthreply)
{
    Packet *pak;
    UDWORD uin;

    pak = event->pak;
    PacketReadData (pak, NULL, PacketReadB2 (pak));
    /* TLV 1 ignored */
    uin = PacketReadUIN (pak);
    Time_Stamp ();
    M_print (" " COLCONTACT "%10s" COLNONE " ", ContactFindName (uin));
    M_print (i18n (1901, "has authorized you to add them to your contact list."));
    M_print ("\n");
    if (prG->sound & SFLAG_CMD)
        ExecScript (prG->sound_cmd, uin, 0, NULL);
    else if (prG->sound & SFLAG_BEEP)
        printf ("\a");
}

/*
 * SRV_ADDEDYOU - SNAC(13,1c)
 */
static JUMP_SNAC_F(SnacSrvAddedyou)
{
    Packet *pak;
    UDWORD uin;

    pak = event->pak;
    PacketReadData (pak, NULL, PacketReadB2 (pak));
    /* TLV 1 ignored */
    uin = PacketReadUIN (pak);
    Time_Stamp ();
    M_print (" " COLCONTACT "%10s" COLNONE " ", ContactFindName (uin));
    M_print (i18n (1755, "added you to their contact list.\n"));
    log_event (uin, LOG_EVENT, "Added to the contact list by %s\n",
               ContactFindName (uin));
    /* Not using Do_Msg/USER_ADDED_MESS here because it's way too
     * v5 specific.  --rtc
     */
}
/*
 * SRV_FROMOLDICQ - SNAC(15,3)
 */
static JUMP_SNAC_F(SnacSrvFromoldicq)
{
    TLV *tlv;
    Packet *p, *pak;
    char *text;
    UDWORD len, uin, type /*, id*/;
    
    pak = event->pak;
    tlv = TLVRead (pak, PacketReadLeft (pak));
    if (tlv[1].len < 10)
    {
        SnacSrvUnknown (event);
        return;
    }
    p = TLVPak (tlv + 1);
    len = PacketRead2 (p);
    uin = PacketRead4 (p);
    type= PacketRead2 (p);
/*  id=*/ PacketRead2 (p);
    if (uin != event->sess->uin)
    {
        if (prG->verbose & DEB_PROTOCOL)
        {
            M_print (i18n (1919, "UIN mismatch: %d vs %d.\n"), event->sess->uin, uin);
            SnacSrvUnknown (event);
        }
    }
    else if (len != tlv[1].len - 2)
    {
        if (prG->verbose & DEB_PROTOCOL)
        {
            M_print (i18n (1743, "Size mismatch in packet lengths.\n"));
            SnacSrvUnknown (event);
        }
    }
    else
    switch (type)
    {
        case 65:
        {
            UWORD year, mon, mday, hour, min, flags;
            char buf[20];

            if (len < 14)
                break;
            uin  = PacketRead4 (p);
            year = PacketRead2 (p);
            mon  = PacketRead1 (p);
            mday = PacketRead1 (p);
            hour = PacketRead1 (p);
            min  = PacketRead1 (p);
            flags= PacketRead2 (p);
            text = PacketReadLNTS (p);
            
            snprintf (buf, sizeof (buf), "%04d-%02d-%02d %2d:%02d UTC", year, mon, mday, hour, min);

            Do_Msg (event->sess, buf, flags, text, uin, STATUS_OFFLINE, 0);
            free (text);
            break;
        }
        case 66:
            SnacCliAckofflinemsgs (event->sess);

            event->sess->connect = CONNECT_OK | CONNECT_SELECT_R;
            reconn = 0;
            CmdUser ("¶e");
            
            QueueEnqueueData (event->sess, 0, QUEUE_SRV_KEEPALIVE,
                              event->sess->uin, time (NULL) + 30,
                              NULL, NULL, &SrvCallBackKeepalive);
            break;

        case 2010:
            Meta_User (event->sess, uin, p);
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
    if (event->sess->flags & CONN_WIZARD)
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
    event->sess->uin = event->sess->spref->uin = PacketReadAt4 (event->pak, 6 + 10 + 46);
    M_print (i18n (1762, "Your new UIN is: %d.\n"), event->sess->uin);
    if (event->sess->flags & CONN_WIZARD)
    {
        assert (event->sess->spref);
        assert (event->sess->assoc);
        assert (event->sess->assoc->spref);

        event->sess->spref->flags |= CONN_AUTOLOGIN;
        event->sess->assoc->spref->flags |= CONN_AUTOLOGIN;
        M_print (i18n (1790, "Setup wizard finished. Congratulations to your new UIN!\n"));
        if (Save_RC () == -1)
        {
            M_print (i18n (1679, "Sorry saving your personal reply messages went wrong!\n"));
        }
        SessionInit (event->sess);
        SessionInit (event->sess->assoc);
    }
}

/*************************************/

/*
 * CLI_READY - SNAC(1,2)
 */
void SnacCliReady (Session *sess)
{
    Packet *pak;
    SNAC *s;

    pak = SnacC (sess, 1, 2, 0, 0);
    
    for (s = SNACv; s->fam; s++)
    {
        if (s->fam == 12 || s->fam == 8)
            continue;

        PacketWriteB2 (pak, s->fam);
        PacketWriteB2 (pak, s->cmd);
        PacketWriteB4 (pak, s->fam == 2 ? 0x0101047B : 0x0110047B);
    }
    SnacSend (sess, pak);
}

/*
 * CLI_FAMILIES - SNAC(1,17)
 */
void SnacCliFamilies (Session *sess)
{
    Packet *pak;
    SNAC *s;

    pak = SnacC (sess, 1, 0x17, 0, 0);
    
    for (s = SNACv; s->fam; s++)
    {
        if (s->fam == 12 || s->fam == 8)
            continue;

        PacketWriteB2 (pak, s->fam);
        PacketWriteB2 (pak, s->cmd);
    }
    SnacSend (sess, pak);
}

/*
 * CLI_SETSTATUS - SNAC(1,1E)
 *
 * action: 1 = send status 2 = send connection info (3 = both)
 */
void SnacCliSetstatus (Session *sess, UWORD status, UWORD action)
{
    Packet *pak;
    
    pak = SnacC (sess, 1, 0x1e, 0, 0);
    if ((action & 1) && (status & STATUSF_INV))
        SnacCliAddvisible (sess, 0);
    if (action & 1)
        PacketWriteTLV4 (pak, 6, status);
    if (action & 2)
    {
        PacketWriteB2 (pak, 0x0c); /* TLV 0C */
        PacketWriteB2 (pak, 0x25);
        PacketWriteB4 (pak, sess->our_local_ip);
        if (sess->assoc && sess->assoc->connect & CONNECT_OK)
        {
            PacketWriteB4 (pak, sess->assoc->port);
            PacketWrite1  (pak, sess->assoc->status);
            PacketWriteB2 (pak, sess->assoc->ver);
            PacketWriteB4 (pak, sess->assoc->our_session);
        }
        else
        {
            PacketWriteB4 (pak, 0);
            PacketWrite1  (pak, 0);
            PacketWriteB2 (pak, 0);
            PacketWriteB4 (pak, 0);
        }
        PacketWriteB2 (pak, 0);
        PacketWriteB2 (pak, 80);
        PacketWriteB2 (pak, 0);
        PacketWriteB2 (pak, 3);
        PacketWriteB4 (pak, BUILD_MICQ);
        PacketWriteB4 (pak, BuildVersionNum);
        PacketWriteB4 (pak, time (NULL));
        PacketWriteB2 (pak, 0);
        PacketWriteTLV2 (pak, 8, 0);
    }
    SnacSend (sess, pak);
    if ((action & 1) && !(status & STATUSF_INV))
        SnacCliAddinvis (sess, 0);
}

/*
 * CLI_RATESREQUEST - SNAC(1,6)
 */
void SnacCliRatesrequest (Session *sess)
{
    SnacSend (sess, SnacC (sess, 1, 6, 0, 0));
}

/*
 * CLI_REQINFO - SNAC (1,E)
 */
void SnacCliReqinfo (Session *sess)
{
    SnacSend (sess, SnacC (sess, 1, 0xE, 0, 0));
}

/*
 * CLI_REQLOCATION - SNAC(2,2)
 */
void SnacCliReqlocation (Session *sess)
{
    SnacSend (sess, SnacC (sess, 2, 2, 0, 0));
}

/*
 * CLI_SETUSERINFO - SNAC(2,4)
 */
void SnacCliSetuserinfo (Session *sess)
{
    Packet *pak;
    
    pak = SnacC (sess, 2, 4, 0, 0);
    PacketWriteTLVData (pak, 4,
       "\x09\x46\x13\x49\x4C\x7F\x11\xD1\x82\x22\x44\x45\x53\x54\x00\x00"
       "\x97\xB1\x27\x51\x24\x3C\x43\x34\xAD\x22\xD6\xAB\xF7\x3F\x14\x92"
       "\x2E\x7A\x64\x75\xFA\xDF\x4D\xC8\x88\x6F\xEA\x35\x95\xFD\xB6\xDF"
       "\x09\x46\x13\x44\x4C\x7F\x11\xD1\x82\x22\x44\x45\x53\x54\x00\x00", 64);
    SnacSend (sess, pak);
}

/*
 * CLI_REQBUDDY - SNAC(3,2)
 */
void SnacCliBuddy (Session *sess)
{
    SnacSend (sess, SnacC (sess, 3, 2, 0, 0));
}

/*
 * CLI_ADDCONTACT - SNAC(3,4)
 */
void SnacCliAddcontact (Session *sess, UDWORD uin)
{
    Packet *pak;
    Contact *cont;
    
    pak = SnacC (sess, 3, 4, 0, 0);
    if (uin)
        PacketWriteUIN (pak, uin);
    else
        for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
            PacketWriteUIN (pak, cont->uin);
    SnacSend (sess, pak);
}

/*
 * CLI_REMCONTACT - SNAC(3,5)
 */
void SnacCliRemcontact (Session *sess, UDWORD uin)
{
    Packet *pak;
    
    pak = SnacC (sess, 3, 5, 0, 0);
    PacketWriteUIN (pak, uin);
    SnacSend (sess, pak);
}

/*
 * CLI_SETICBM - SNAC(4,2)
 */
void SnacCliSeticbm (Session *sess)
{
   Packet *pak;
   
   pak = SnacC (sess, 4, 2, 0, 0);
   PacketWriteB2 (pak, 0);
   PacketWriteB2 (pak, 0);
   PacketWriteB2 (pak, 3);
   PacketWriteB2 (pak, 8000);
   PacketWriteB2 (pak, 999);
   PacketWriteB2 (pak, 999);
   PacketWriteB2 (pak, 0);
   PacketWriteB2 (pak, 0);
   SnacSend (sess, pak);
}
/*
 * CLI_REQICBM - SNAC(4,4)
 */
void SnacCliReqicbm (Session *sess)
{
    SnacSend (sess, SnacC (sess, 4, 4, 0, 0));
}

/*
 * CLI_SENDMSG - SNAC(4,6)
 */
void SnacCliSendmsg (Session *sess, UDWORD uin, const char *text, UDWORD type)
{
    Packet *pak;
    UBYTE format;
    UDWORD mtime = 0, mid = 0;
    
    if (type != 0xe8)
    {
        Time_Stamp ();
        M_print (" " COLACK "%10s" COLNONE " " MSGSENTSTR "%s\n", ContactFindName (uin), MsgEllipsis (text));
    }
    
    switch (type)
    {
        case URL_MESS:
        case AUTH_REQ_MESS:
        case AUTH_OK_MESS:
        case AUTH_REF_MESS:
            format = 4;
            break;
        case NORM_MESS:
            format = 1;
            break;
        case 0xe8:
            format = 2;
            break;
        default:
            M_print (i18n (1930, "Can't send this (%x) yet.\n"), type);
            return;
    }
    
    pak = SnacC (sess, 4, 6, 0, format == 2 ? 0x1771 : 0);
    PacketWriteB4 (pak, mtime);
    PacketWriteB4 (pak, mid);
    PacketWriteB2 (pak, format);
    PacketWriteUIN (pak, uin);
    
    switch (format)
    {
        case 1:
            PacketWriteTLV     (pak, 2);
            PacketWriteTLV     (pak, 1281);
            PacketWrite1       (pak, 1);
            PacketWriteTLVDone (pak);
            PacketWriteTLV     (pak, 257);
            PacketWrite4       (pak, 0);
            PacketWriteStr     (pak, text);
            PacketWriteTLVDone (pak);
            PacketWriteTLVDone (pak);
            PacketWriteB2 (pak, 6);
            PacketWriteB2 (pak, 0);
            break;
        case 2:
            PacketWriteTLV     (pak, 5);
            PacketWrite2       (pak, 0);
            PacketWriteB4      (pak, mtime);
            PacketWriteB4      (pak, mid);
            PacketWriteB4      (pak, 0x09461349); /* capability */
            PacketWriteB4      (pak, 0x4c7f11d1);
            PacketWriteB4      (pak, 0x82224445);
            PacketWriteB4      (pak, 0x53540000);
            PacketWriteTLV2    (pak, 10, 1);
            PacketWriteB4      (pak, 0x000f0000); /* empty TLV(5) */
            PacketWriteTLV     (pak, 10001);
            PacketWrite1       (pak, 27);
            PacketWriteB2      (pak, sess->assoc && sess->assoc->connect & CONNECT_OK
                                     ? sess->assoc->ver : 0);
            PacketWrite1       (pak, 0);
            PacketWriteB4      (pak, 0); /* capability */
            PacketWriteB4      (pak, 0);
            PacketWriteB4      (pak, 0);
            PacketWriteB4      (pak, 0);
            PacketWrite2       (pak, 0);
            PacketWrite1       (pak, 0);
            PacketWrite4       (pak, 0);
            PacketWrite2       (pak, -1);
            PacketWrite2       (pak, 14);
            PacketWrite2       (pak, -1);
            PacketWrite4       (pak, 0);
            PacketWrite4       (pak, 0);
            PacketWrite4       (pak, 0);
            PacketWrite2       (pak, type);
            PacketWrite2       (pak, 0);
            PacketWrite2       (pak, 1);
            PacketWriteLNTS    (pak, text);
            PacketWriteB4      (pak, 0);
            PacketWriteB4      (pak, 0xffffff00);
            PacketWriteTLVDone (pak);
            PacketWriteB4      (pak, 0x00030000); /* empty TLV(3) */
            PacketWriteTLVDone (pak);
            break;
        case 4:
            PacketWriteTLV     (pak, 5);
            PacketWrite4       (pak, sess->uin);
            PacketWrite1       (pak, type);
            PacketWrite1       (pak, 0);
            PacketWriteLNTS    (pak, text);
            PacketWriteTLVDone (pak);
            PacketWriteB2 (pak, 6);
            PacketWriteB2 (pak, 0);
    }
    SnacSend (sess, pak);
}

/*
 * CLI_REQBOS - SNAC(9,2)
 */
void SnacCliReqbos (Session *sess)
{
    SnacSend (sess, SnacC (sess, 9, 2, 0, 0));
}

/*
 * CLI_ADDVISIBLE - SNAC(9,5)
 */
void SnacCliAddvisible (Session *sess, UDWORD uin)
{
    Packet *pak;
    Contact *cont;
    
    pak = SnacC (sess, 9, 5, 0, 0);
    if (uin)
        PacketWriteUIN (pak, uin);
    else
        for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
            if (cont->flags & CONT_INTIMATE)
                PacketWriteUIN (pak, cont->uin);
    SnacSend (sess, pak);
}

/*
 * CLI_REMVISIBLE - SNAC(9,6)
 */
void SnacCliRemvisible (Session *sess, UDWORD uin)
{
    Packet *pak;
    
    pak = SnacC (sess, 9, 6, 0, 0);
    PacketWriteUIN (pak, uin);
    SnacSend (sess, pak);
}

/*
 * CLI_ADDINVIS - SNAC(9,7)
 */
void SnacCliAddinvis (Session *sess, UDWORD uin)
{
    Packet *pak;
    Contact *cont;
    
    pak = SnacC (sess, 9, 7, 0, 0);
    if (uin)
        PacketWriteUIN (pak, uin);
    else
        for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
            if (cont->flags & CONT_HIDEFROM)
                PacketWriteUIN (pak, cont->uin);
    SnacSend (sess, pak);
}

/*
 * CLI_REMINVIS - SNAC(9,8)
 */
void SnacCliReminvis (Session *sess, UDWORD uin)
{
    Packet *pak;
    
    pak = SnacC (sess, 9, 8, 0, 0);
    PacketWriteUIN (pak, uin);
    SnacSend (sess, pak);
}

/*
 * CLI_REQROSTER - SNAC(13,5)
 */
void SnacCliReqroster (Session *sess)
{
    Packet *pak;
    Contact *cont;
    int i;
    
    pak = SnacC (sess, 19, 5, 0, 0);
    for (i = 0, cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
        i++;

    PacketWriteB4 (pak, 0);  /* last modification of server side contact list */
    PacketWriteB2 (pak, i);  /* # of entries */
    SnacSend (sess, pak);
}

/*
 * CLI_REQAUTH - SNAC(13,18)
 */
void SnacCliReqauth (Session *sess, UDWORD uin, const char *msg)
{
    Packet *pak;
    
    pak = SnacC (sess, 19, 24, 0, 0);
    PacketWriteUIN  (pak, uin);
    PacketWriteStrB (pak, msg);
    PacketWrite2    (pak, 0);
    SnacSend (sess, pak);
}

/*
 * CLI_AUTHORIZE - SNAC(13,1a)
 */
void SnacCliAuthorize (Session *sess, UDWORD uin, BOOL accept, const char *msg)
{
    Packet *pak;
    
    pak = SnacC (sess, 19, 26, 0, 0);
    PacketWriteUIN  (pak, uin);
    PacketWrite1    (pak, accept ? 1 : 0);
    PacketWriteStrB (pak, accept ? "" : msg);
    SnacSend (sess, pak);
}

/*
 * Create meta request package.
 */
Packet *SnacMetaC (Session *sess, UWORD sub, UWORD type)
{
    Packet *pak;

    sess->our_seq3 = sess->our_seq3 ? sess->our_seq3 + 1 : 2;
    
    pak = SnacC (sess, 21, 2, 0, rand () % 0xffffff);
    PacketWriteTLV (pak, 1);
    PacketWriteLen     (pak);
    PacketWrite4  (pak, sess->uin);
    PacketWrite2  (pak, sub);
    PacketWrite2  (pak, sess->our_seq3);
    if (type)
        PacketWrite2 (pak, type);

    return pak;
}

/*
 * Complete & send meta request package.
 */
void SnacMetaSend (Session *sess, Packet *pak)
{
    PacketWriteLenDone (pak);
    PacketWriteTLVDone (pak);
    SnacSend (sess, pak);
}

/*
 * CLI_REQOFFLINEMSGS - SNAC(15,2) - 60
 */
void SnacCliReqofflinemsgs (Session *sess)
{
    Packet *pak;

    pak = SnacMetaC (sess, 60, 0);
    SnacMetaSend    (sess, pak);
}

/*
 * CLI_ACKOFFLINEMSGS - SNAC(15,2) - 62
 */
void SnacCliAckofflinemsgs (Session *sess)
{
    Packet *pak;

    pak = SnacMetaC (sess, 62, 0);
    SnacMetaSend    (sess, pak);
}

/*
 * CLI_METASETGENERAL - SNAC(15,2) - 2000/1002
 */
void SnacCliMetasetgeneral (Session *sess, const MetaGeneral *user)
{
    Packet *pak;

    pak = SnacMetaC (sess, 2000, META_SET_GENERAL_INFO);
    PacketWriteLNTS (pak, user->nick);
    PacketWriteLNTS (pak, user->first);
    PacketWriteLNTS (pak, user->last);
    PacketWriteLNTS (pak, user->email);
    PacketWriteLNTS (pak, user->city);
    PacketWriteLNTS (pak, user->state);
    PacketWriteLNTS (pak, user->phone);
    PacketWriteLNTS (pak, user->fax);
    PacketWriteLNTS (pak, user->street);
    PacketWriteLNTS (pak, user->cellular);
    PacketWriteLNTS (pak, UtilFill ("%05d", user->zip));
    PacketWrite2    (pak, user->country);
    PacketWrite1    (pak, user->tz);
    PacketWrite1    (pak, user->webaware);
    SnacMetaSend    (sess, pak);
}

/*
 * CLI_METASETABOUT - SNAC(15,2) - 2000/1030
 */
void SnacCliMetasetabout (Session *sess, const char *text)
{
    Packet *pak;

    pak = SnacMetaC (sess, 2000, META_SET_ABOUT_INFO);
    PacketWriteLNTS (pak, text);
    SnacMetaSend    (sess, pak);
}

/*
 * CLI_METASETMORE - SNAC(15,2) - 2000/1021
 */
void SnacCliMetasetmore (Session *sess, const MetaMore *user)
{
    Packet *pak;

    pak = SnacMetaC (sess, 2000, META_SET_MORE_INFO);
    PacketWrite2    (pak, user->age);
    PacketWrite1    (pak, user->sex);
    PacketWriteLNTS (pak, user->hp);
    PacketWrite2    (pak, user->year);
    PacketWrite1    (pak, user->month);
    PacketWrite1    (pak, user->day);
    PacketWrite1    (pak, user->lang1);
    PacketWrite1    (pak, user->lang2);
    PacketWrite1    (pak, user->lang3);
    SnacMetaSend    (sess, pak);
}

/*
 * CLI_METASETPASS - SNAC(15,2) - 2000/1070
 */
void SnacCliMetasetpass (Session *sess, const char *newpass)
{
    Packet *pak;
    
    pak = SnacMetaC (sess, 2000, 1070);
    PacketWriteLNTS (pak, newpass);
    SnacMetaSend    (sess, pak);
}

/*
 * CLI_METAREQINFO - SNAC(15,2) - 2000/1232
 */
void SnacCliMetareqinfo (Session *sess, UDWORD uin)
{
    Packet *pak;

    pak = SnacMetaC (sess, 2000, META_REQ_INFO);
    PacketWrite4    (pak, uin);
    SnacMetaSend    (sess, pak);
}

/*
 * CLI_SEARCHBYPERSINF - SNAC(15,2) - 2000/1375
 */
void SnacCliSearchbypersinf (Session *sess, const char *email, const char *nick, const char *name, const char *surname)
{
    Packet *pak;

    pak = SnacMetaC  (sess, 2000, META_SEARCH_PERSINFO);
    PacketWrite2     (pak, 320); /* key: first name */
    PacketWriteLLNTS (pak, name);
    PacketWrite2     (pak, 330); /* key: last name */
    PacketWriteLLNTS (pak, surname);
    PacketWrite2     (pak, 340); /* key: nick */
    PacketWriteLLNTS (pak, nick);
    PacketWrite2     (pak, 350); /* key: email address */
    PacketWriteLLNTS (pak, email);
    SnacMetaSend     (sess, pak);
}

/*
 * CLI_SEARCHBYMAIL - SNAC(15,2) - 2000/{1395,1321}
 */
void SnacCliSearchbymail (Session *sess, const char *email)
{
    Packet *pak;

    pak = SnacMetaC  (sess, 2000, META_SEARCH_EMAIL);
    PacketWrite2     (pak, 350); /* key: email address */
    PacketWriteLLNTS (pak, email);
    SnacMetaSend     (sess, pak);
}

/*
 * CLI_SEARCHRANDOM - SNAC(15,2) - 2000/1870
 */
void SnacCliSearchrandom (Session *sess, UWORD group)
{
    Packet *pak;

    pak = SnacMetaC (sess, 2000, META_SEARCH_RANDOM);
    PacketWrite2    (pak, group);
    SnacMetaSend    (sess, pak);
}

/*
 * CLI_SETRANDOM - SNAC(15,2) - 2000/1880
 */
void SnacCliSetrandom (Session *sess, UWORD group)
{
    Packet *pak;

    pak = SnacMetaC (sess, 2000, META_SET_RANDOM);
    PacketWrite2    (pak, group);
    if (group)
    {
        PacketWriteB4 (pak, 0x00000220);
        PacketWriteB4 (pak, 0);
        PacketWriteB4 (pak, 0);
        PacketWriteB4 (pak, 0);
        PacketWrite1  (pak, sess->assoc && sess->assoc->connect & CONNECT_OK
                            ? sess->assoc->status : 0);
        PacketWrite2  (pak, sess->assoc && sess->assoc->connect & CONNECT_OK
                            ? sess->assoc->ver : 0);
        PacketWriteB4 (pak, 0);
        PacketWriteB4 (pak, 0x00005000);
        PacketWriteB4 (pak, 0x00000300);
        PacketWrite2  (pak, 0);
    }
    SnacMetaSend (sess, pak);
}

/*
 * CLI_SEARCHWP - SNAC(15,2) - 2000/1331
 */
void SnacCliSearchwp (Session *sess, const MetaWP *wp)
{
    Packet *pak;

    pak = SnacMetaC (sess, 2000, META_SEARCH_WP);
    PacketWriteLNTS    (pak, wp->first);
    PacketWriteLNTS    (pak, wp->last);
    PacketWriteLNTS    (pak, wp->nick);
    PacketWriteLNTS    (pak, wp->email);
    PacketWrite2       (pak, wp->minage);
    PacketWrite2       (pak, wp->maxage);
    PacketWrite1       (pak, wp->sex);
    PacketWrite1       (pak, wp->language);
    PacketWriteLNTS    (pak, wp->city);
    PacketWriteLNTS    (pak, wp->state);
    PacketWriteB2      (pak, wp->country);
    PacketWriteLNTS    (pak, wp->company);
    PacketWriteLNTS    (pak, wp->department);
    PacketWriteLNTS    (pak, wp->position);
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
    SnacMetaSend (sess, pak);
}

/*
 * CLI_SENDSMS - SNAC(15,2) - 2000/5250
 */
void SnacCliSendsms (Session *sess, const char *target, const char *text)
{
    Packet *pak;
    char buf[2000], tbuf[100];
    time_t t = time (NULL);

    strftime (tbuf, sizeof (tbuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime (&t));
    snprintf (buf, sizeof (buf), "<icq_sms_message><destination>%s</destination>"
             "<text>%s (www.mICQ.org)</text><codepage>1252</codepage><senders_UIN>%ld</senders_UIN>"
             "<senders_name>%s</senders_name><delivery_receipt>Yes</delivery_receipt>"
             "<time>%s</time></icq_sms_message>",
             target, text, sess->uin, "mICQ", tbuf);

    pak = SnacMetaC (sess, 2000, META_SEND_SMS);
    PacketWriteB2      (pak, 1);
    PacketWriteB2      (pak, 22);
    PacketWriteB4      (pak, 0);
    PacketWriteB4      (pak, 0);
    PacketWriteB4      (pak, 0);
    PacketWriteB4      (pak, 0);
    PacketWriteTLVStr  (pak, 0, buf);
    SnacMetaSend (sess, pak);
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
void SnacCliRegisteruser (Session *sess)
{
    Packet *pak;
    
    pak = SnacC (sess, 23, 4, 0, 0);
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
    PacketWriteLNTS (pak, sess->passwd);
    PacketWriteB4 (pak, REG_X2);
    PacketWriteB4 (pak, REG_X3);
    PacketWriteTLVDone (pak);
    SnacSend (sess, pak);
}
