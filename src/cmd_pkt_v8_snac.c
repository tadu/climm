
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
#include "preferences.h"
#include "icq_response.h"
#include "cmd_pkt_v8_snac.h"
#include "cmd_pkt_v8_flap.h"
#include "cmd_pkt_v8_tlv.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

typedef void (jump_snac_f)(struct Event *);
typedef struct { UWORD fam; UWORD cmd; const char *name; jump_snac_f *f; } SNAC;
#define JUMP_SNAC_F(f) void f (struct Event *event)

static jump_snac_f SnacSrvFamilies, SnacSrvFamilies2, SnacSrvMotd,
    SnacSrvRates, SnacSrvReplyicbm, SnacSrvReplybuddy, SnacSrvReplybos,
    SnacSrvReplyinfo, SnacSrvReplylocation, SnacSrvUseronline, SnacSrvRegrefused,
    SnacSrvUseroffline, SnacSrvRecvmsg, SnacSrvUnknown, SnacSrvFromoldicq,
    SnacSrvAddedyou, SnacSrvNewuin;

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
    {  4,  5, "SRV_REPLYICBM",       SnacSrvReplyicbm},
    {  4,  7, "SRV_RECVMSG",         SnacSrvRecvmsg},
    {  9,  3, "SRV_REPLYBOS",        SnacSrvReplybos},
    { 19,  3, "SRV_REPLYUNKNOWN",    SnacSrvUnknown},
    { 19,  6, "SRV_REPLYROSTER",     SnacSrvUnknown},
    { 19, 14, "SRV_UPDATEACK",       SnacSrvUnknown},
    { 19, 15, "SRV_REPLYROSTEROK",   SnacSrvUnknown},
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
    { 21,  2, "CLI_TOICQSRV",        NULL},
    { 23,  4, "CLI_REGISTERUSER",    NULL},
    {  0,  0, "unknown",             NULL}
};

#define SnacSend FlapSend

/*
 * Interprets the given SNAC.
 */
void SrvCallBackSnac (struct Event *event)
{
    Packet  *pak  = event->pak;
    SNAC *s;
    UWORD family, flags;
    UDWORD ref;
    
    family   = PacketReadB2 (pak);
    pak->cmd = PacketReadB2 (pak);
    flags    = PacketReadB2 (pak);
    ref      = PacketReadB4 (pak);
    
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
 * Prints a given SNAC packet.
 */
void SnacPrint (Packet *pak)
{
    assert (pak->len >= 16);

    M_print (i18n (58, "SNAC (%x,%x) [%s] flags %x ref %x\n"),
             PacketReadBAt2 (pak, 6), PacketReadBAt2 (pak, 8),
             SnacName (PacketReadBAt2 (pak, 6), PacketReadBAt2 (pak, 8)),
             PacketReadBAt2 (pak, 10), PacketReadBAt4 (pak, 12));
    M_print (COLNONE);
    Hex_Dump (pak->data + 16, pak->len - 16);
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
JUMP_SNAC_F(SnacSrvUnknown)
{
    if (!(prG->verbose & 128))
    {
        Time_Stamp ();
        M_print (" " ESC "«");
        M_print (" " ESC "«" COLSERV "%s ", i18n (59, COLSERV "Incoming v8 server packet:"));
        SnacPrint (event->pak);
        M_print (ESC "»\n");
    }
}

/***********************************************/

/*
 * SRV_FAMILIES - SNAC(1,3),
 */
JUMP_SNAC_F(SnacSrvFamilies)
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
            M_print (i18n (899, "Unknown family requested: %d\n"), fam);
            continue;
        }
    }
    SnacCliFamilies (event->sess);
}

/*
 * SRV_REPLYLOCATION - SNAC(2,3)
 */
JUMP_SNAC_F(SnacSrvReplylocation)
{
    /* ignore all data, do nothing */
}

/*
 * SRV_RATES - SNAC(1,7)
 * CLI_ACKRATES - SNAC(1,8)
 */
JUMP_SNAC_F(SnacSrvRates)
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
JUMP_SNAC_F(SnacSrvReplyinfo)
{
    Packet *pak;
    TLV *tlv;
    UDWORD uin;
    
    pak = event->pak;
    if (PacketReadBAt2 (pak, 10) & 0x8000)
    {
        PacketReadB4 (pak);
        PacketReadB4 (pak);
    }
    uin = PacketReadUIN (pak);
    
    if (uin != event->sess->uin)
        M_print (i18n (67, "Warning: Server thinks our UIN is %d, while it is %d.\n"),
                 uin, event->sess->uin);
    PacketReadB2 (pak);
    PacketReadB2 (pak);
    tlv = TLVRead (pak);
    if (tlv[10].len)
    {
        event->sess->our_outside_ip = tlv[10].nr;
        if (prG->verbose)
            M_print (i18n (915, "Server says we're at %s.\n"), UtilIOIP (event->sess->our_outside_ip));
        if (event->sess->assoc)
            event->sess->assoc->our_outside_ip = event->sess->our_outside_ip;
    }
    if (tlv[6].len)
    {
        event->sess->status = tlv[6].nr;
        Time_Stamp ();
        M_print (" ");
        Print_Status (event->sess->status);
        M_print ("\n");
    }
    /* TLV 1 c f 2 3 ignored */
}

/*
 * SRV_FAMILIES2 - SNAC(1,18)
 */
JUMP_SNAC_F(SnacSrvFamilies2)
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
            M_print (i18n (904, "Server doesn't understand ver %d (only %d) for family %d!\n"), s->cmd, ver, fam);
    }
}

/*
 * SRV_MOTD - SNAC(1,13)
 */
JUMP_SNAC_F(SnacSrvMotd)
{
    if (!(event->sess->connect & CONNECT_OK))
        event->sess->connect++;

    SnacCliRatesrequest (event->sess);
    SnacCliReqinfo      (event->sess);
    SnacCliReqlocation  (event->sess);
    SnacCliBuddy        (event->sess);
    SnacCliReqicbm      (event->sess);
    SnacCliReqbos       (event->sess);
}

/*
 * SRV_REPLYBUDDY - SNAC(3,3)
 */
JUMP_SNAC_F(SnacSrvReplybuddy)
{
    /* ignore all data, do nothing */
}

/*
 * SRV_USERONLINE - SNAC(3,B)
 */
JUMP_SNAC_F(SnacSrvUseronline)
{
    Contact *cont;
    Packet *p, *pak;
    TLV *tlv;
    
    pak = event->pak;
    cont = ContactFind (PacketReadUIN (pak));
    if (!cont)
    {
        if (prG->verbose)
            M_print (i18n (725, "Received USERONLINE packet for non-contact.\n"));
        return;
    }
    
    PacketReadB2 (pak);
    PacketReadB2 (pak);
    tlv = TLVRead (pak);
    if (tlv[10].len)
        cont->outside_ip = tlv[10].nr;
    
    if (tlv[12].len)
    {
        p = TLVPak (tlv + 12);
        cont->local_ip = PacketReadB4 (p);
        cont->port     = PacketReadB4 (p);
        cont->connection_type = PacketRead1 (p);
        cont->TCP_version = PacketReadB2 (p);
        PacketReadB4 (p);
        PacketReadB4 (p);
        PacketReadB4 (p);
        UserOnlineSetVersion (cont, PacketReadB4 (p));
        /* remainder ignored */
    }
    /* TLV 1, d, f, 2, 3 ignored */

    UtilUIUserOnline (cont, tlv[6].len ? tlv[6].nr : 0);
}

/*
 * SRV_USEROFFLINE - SNAC(3,C)
 */
JUMP_SNAC_F(SnacSrvUseroffline)
{
    Contact *cont;
    Packet *pak;
    
    pak = event->pak;
    cont = ContactFind (PacketReadUIN (pak));
    if (!cont)
    {
        if (prG->verbose)
            M_print (i18n (726, "Received USEROFFLINE packet for non-contact.\n"));
        return;
    }

    cont->status = STATUS_OFFLINE;
    cont->last_time = time (NULL);
    
    UtilUIUserOffline (cont);
}

/*
 * SRV_REPLYICBM - SNAC(4,5)
 */
JUMP_SNAC_F(SnacSrvReplyicbm)
{
   SnacCliSeticbm (event->sess);
}

/*
 * SRV_RECVMSG - SNAC(4,7)
 */
JUMP_SNAC_F(SnacSrvRecvmsg)
{
    Contact *cont;
    Packet *p, *pak;
    TLV *tlv;
    UDWORD uin;
    int i, len;
    const char *text;

    pak = event->pak;

    PacketReadB4 (pak); /* TIME */
    PacketReadB2 (pak); /* RANDOM */
    PacketReadB2 (pak); /* ??? */

    switch (PacketReadB2 (pak))
    {
        case 1:
            uin = PacketReadUIN (pak);
            cont = ContactFind (uin);
            PacketReadB2 (pak); /* WARNING */
            PacketReadB2 (pak); /* COUNT */
            tlv = TLVRead (pak);

            if (tlv[6].len && cont)
                UtilUIUserOnline (cont, tlv[6].nr);

            if (tlv[2].len == 4)
            {
                for (i = 16; i < 20; i++)
                    if (tlv[i].tlv == 2)
                    {
                        int typ = tlv[i].tlv, len = tlv[i].len, nr = tlv[i].nr;
                        const char *str = tlv[i].str;
                        tlv[i].tlv = tlv[2].tlv; tlv[i].len = tlv[2].len;
                        tlv[i].nr  = tlv[2].nr;  tlv[i].str = tlv[2].str;
                        tlv[2].tlv = typ; tlv[2].len = len;
                        tlv[2].nr  = nr;  tlv[2].str = str;
                    }
            }

            if (tlv[2].len > 4)
            {
                p = TLVPak (tlv + 2);
                PacketReadB2 (p);
                for (len = PacketReadB2 (p); len > 0; len--)
                    PacketRead1 (p);
                PacketReadB2 (p);
                len = PacketReadBAt2 (p, PacketReadPos (p));
                text = PacketReadStrB (p);

                Time_Stamp ();
                M_print (" " CYAN BOLD "%10s" COLNONE " ",
                         ContactFindName (uin));
                Do_Msg (event->sess, NORM_MESS, len - 4, text + 4, uin, 0);
                free ((char *)text);
            }
            /* TLV 1, 2(!), 3, 4, f ignored */
            if (prG->sound & SFLAG_CMD)
                ExecScript (prG->sound_cmd, uin, 0, NULL);
            else if (prG->sound & SFLAG_BEEP)
                printf ("\a");
            break;
        case 2:
        default:
            SnacSrvUnknown (event);
            break;
    }
}

/*
 * SRV_REPLYBOS - SNAC(9,3)
 */
JUMP_SNAC_F(SnacSrvReplybos)
{
    SnacCliSetuserinfo (event->sess);
    SnacCliSetstatus (event->sess, event->sess->spref->status);
    SnacCliReady (event->sess);
    SnacCliReqofflinemsgs (event->sess);
/*    SnacCliReqroster (event->sess); */
    SnacCliAddcontact (event->sess, 0);

    event->sess->connect = CONNECT_OK | CONNECT_SELECT_R;
}

/*
 * SRV_ADDEDYOU - SNAC(13,1c)
 */
JUMP_SNAC_F(SnacSrvAddedyou)
{
    Contact *cont;
    Packet *pak;
    UDWORD uin;
    int len;

    pak = event->pak;

    for (len = PacketReadB2 (pak); len > 0; len--)
        PacketRead1 (pak);
    /* TLV 1 ignored */
    uin = PacketReadUIN (pak);
    cont = ContactFind (uin);
    Time_Stamp ();
    M_print (" " COLCONTACT "%10s" COLNONE " ", ContactFindName (uin));
    M_print (i18n (755, "added you to their contact list.\n"));
    log_event (uin, LOG_EVENT, "Added to the contact list by %s\n",
               ContactFindName (uin));
    /* Not using Do_Msg/USER_ADDED_MESS here because it's way too
     * v5 specific.  --rtc
     */
}

/*
 * SRV_FROMOLDICQ - SNAC(15,3)
 */
JUMP_SNAC_F(SnacSrvFromoldicq)
{
    TLV *tlv;
    Packet *p, *pak;
    const char *text;
    UDWORD len, uin, type, id;
    
    pak = event->pak;
    tlv = TLVRead (pak);
    if (tlv[1].len < 10)
    {
        SnacSrvUnknown (event);
        return;
    }
    p = TLVPak (tlv + 1);
    len = PacketRead2 (p);
    uin = PacketRead4 (p);
    type= PacketRead2 (p);
    id  = PacketRead2 (p);
    if (prG->verbose && uin != event->sess->uin)
    {
        M_print (i18n (733, "UIN mismatch: %d vs %d.\n"), event->sess->uin, uin);
        SnacSrvUnknown (event);
        return;
    }
    if (prG->verbose && len != tlv[1].len - 2)
    {
        M_print (i18n (743, "Size mismatch in packet lengths.\n"));
        SnacSrvUnknown (event);
        return;
    }
    switch (type)
    {
        case 0x41:
        {
            UDWORD year, mon, mday, hour, min, flags;
            if (len < 14)
                break;
            uin  = PacketRead4 (p);
            year = PacketRead2 (p);
            mon  = PacketRead1 (p);
            mday = PacketRead1 (p);
            hour = PacketRead1 (p);
            min  = PacketRead1 (p);
            flags= PacketRead2 (p);
            text = PacketReadStrN (p);

            M_print ("%04d-%02d-%02d %2d:%02d:%02d", year, mon, mday, hour, min, 0);
            M_print (" " CYAN BOLD "%10s" COLNONE " ",
                     ContactFindName (uin));
            Do_Msg (event->sess, flags, strlen (text), text, uin, 0);
            return;
        }
        case 0x42:
            SnacCliAckofflinemsgs (event->sess);
            return;
        default:
    }
    SnacSrvUnknown (event);
}

/*
 * SRV_REGREFUSED - SNAC(17,1)
 */
JUMP_SNAC_F(SnacSrvRegrefused)
{
    M_print (i18n (780, "Registration of new UIN refused.\n"));
}

/*
 * SRV_NEWUIN - SNAC(17,5)
 */
JUMP_SNAC_F(SnacSrvNewuin)
{
    event->sess->uin = event->sess->spref->uin = PacketReadAt4 (event->pak, 6 + 10 + 46);
    M_print (i18n (762, "Your new UIN is: %d.\n"), event->sess->uin);
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
 * CLI_SETSTATUS - SNAC(1,1E)
 */
void SnacCliSetstatus (Session *sess, UWORD status)
{
    Packet *pak;
    
    if (status & STATUS_INVISIBLE)
        SnacCliAddvisible (sess, 0);
    pak = SnacC (sess, 1, 0x1e, 0, 0);
    PacketWriteTLV4 (pak, 6, status);
    if (!(status & 256) && !(sess->connect & CONNECT_OK))
    {
        PacketWriteB2 (pak, 0x0c); /* TLV 0C */
        PacketWriteB2 (pak, 0x25);
        PacketWriteB4 (pak, sess->our_local_ip);
        PacketWriteB4 (pak, sess->assoc && sess->assoc->connect & CONNECT_OK 
                            ? sess->assoc->port : 0);
        PacketWrite1  (pak, sess->assoc && sess->assoc->connect & CONNECT_OK
                            ? 0x04 : 0);
        PacketWriteB2 (pak, TCP_VER);
        PacketWriteB4 (pak, rand() % 0x7fff); /* TCP cookie */
        PacketWriteB2 (pak, 0);
        PacketWriteB2 (pak, 80);
        PacketWriteB2 (pak, 0);
        PacketWriteB2 (pak, 3);
        PacketWriteB4 (pak, BUILD_LICQ | MICQ_VERSION_NUM);
        PacketWriteB4 (pak, 0);
        PacketWriteB4 (pak, time (NULL));
        PacketWriteB2 (pak, 0);
        PacketWriteTLV2 (pak, 8, 0);
    }
    SnacSend (sess, pak);
    if (!(status & STATUS_INVISIBLE))
        SnacCliAddinvis (sess, 0);
    
/*  sess->status = status;  Note: this will be set by SRV_REPLYINFO */
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
void SnacCliSendmsg (Session *sess, UDWORD uin, char *text, UDWORD type)
{
    Packet *pak;
    UBYTE format;
    
    Time_Stamp ();
    M_print (" " COLACK "%10s" COLNONE " " MSGACKSTR "%s\n", ContactFindName (uin), MsgEllipsis (text));
    
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
        default:
            M_print (i18n (88, "Can't send this (%x) yet.\n"), type);
            return;
    }
    
    pak = SnacC (sess, 4, 6, 0, 0);
    PacketWriteB4 (pak, 0); /* message ID */
    PacketWriteB4 (pak, 0); /* message ID */
    PacketWriteB2 (pak, format);
    PacketWriteUIN (pak, uin);
    
    switch (format)
    {
        case 1:
            PacketWriteB2 (pak, 2);
            PacketWriteB2 (pak, strlen (text) + 13);
            PacketWriteB4 (pak, 0x05010001);
            PacketWriteB2 (pak, 0x0101);
            PacketWrite1  (pak, 0x01);
            PacketWriteB2 (pak, strlen (text) + 4);
            PacketWriteB4 (pak, 0);
            PacketWriteData (pak, text, strlen (text));
            break;
        case 4:
            PacketWriteB2 (pak, 5);
            PacketWriteB2 (pak, strlen (text) + 9);
            PacketWrite4  (pak, sess->uin);
            PacketWrite1  (pak, type);
            PacketWrite1  (pak, 0);
            PacketWriteStrN (pak, text);
    }
    PacketWriteB2 (pak, 6);
    PacketWriteB2 (pak, 0);
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
            if (cont->vis_list)
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
            if (cont->invis_list)
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
 * CLI_REQOFFLINEMSGS - SNAC(15,2) - 60
 */
void SnacCliReqofflinemsgs (Session *sess)
{
    Packet *pak;

    pak = SnacC (sess, 21, 2, 0, 0);
    PacketWriteB2 (pak, 1);  /* TLV(1) */
    PacketWriteB2 (pak, 10);
    PacketWrite2  (pak, 8);
    PacketWrite4  (pak, sess->uin);
    PacketWrite2  (pak, 0x3c);
    PacketWrite2  (pak, 2);
    SnacSend (sess, pak);
}

/*
 * CLI_ACKOFFLINEMSGS - SNAC(15,2) - 62
 */
void SnacCliAckofflinemsgs (Session *sess)
{
    Packet *pak;

    pak = SnacC (sess, 21, 2, 0, 0);
    PacketWriteB2 (pak, 1);  /* TLV(1) */
    PacketWriteB2 (pak, 10);
    PacketWrite2  (pak, 8);
    PacketWrite4  (pak, sess->uin);
    PacketWrite2  (pak, 0x3e);
    PacketWrite2  (pak, 2);
    SnacSend (sess, pak);
}

/*
 * CLI_REGISTERUSER - SNAC(17,4)
 */
#define _REG_X2 0x28000300
#define _REG_X3 0x8a4c0000
#define _REG_X4 0x00000602
#define REG_X1 0x00010033
#define REG_X2 0x28000300
#define REG_X3 0x9e270000
#define REG_X4 0x00000302
void SnacCliRegisteruser (Session *sess)
{
    Packet *pak;
    
    pak = SnacC (sess, 23, 4, 0, 0);
    PacketWriteB4 (pak, REG_X1 + strlen (sess->passwd));
    PacketWriteB4 (pak, 0);
    PacketWriteB4 (pak, REG_X2);
    PacketWriteB4 (pak, 0);
    PacketWriteB4 (pak, 0);
    PacketWriteB4 (pak, REG_X3);
    PacketWriteB4 (pak, REG_X3);
    PacketWriteB4 (pak, 0);
    PacketWriteB4 (pak, 0);
    PacketWriteB4 (pak, 0);
    PacketWriteB4 (pak, 0);
    PacketWriteStrN (pak, sess->passwd);
    PacketWriteB4 (pak, REG_X3);
    PacketWriteB4 (pak, REG_X4);
    SnacSend (sess, pak);
}
