
#include "micq.h"
#include "util.h"
#include "util_ui.h"
#include "cmd_pkt_v8_snac.h"
#include "cmd_pkt_v8_flap.h"

typedef void (jump_snac_f)(struct Event *);
typedef struct { UWORD fam; UWORD cmd; const char *name; jump_snac_f *f; } SNAC;
#define JUMP_SNAC_F(f) void f (struct Event *event)

static jump_snac_f SnacSrvFamilies, SnacSrvFamilies2, SnacSrvRates,
    SnacSrvReplyicbm, SnacSrvReplybuddy, SnacSrvReplaybos;

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
    {  1,  3, "SRV_FAMILIES",      SnacSrvFamilies},
    {  1,  7, "SRV_RATES",         SnacSrvRates},
    {  1, 15, "SRV_REPLYINFO",     NULL},
    {  1, 19, "SRV_MOTD",          NULL},
    {  1, 24, "SRV_FAMILIES2",     SnacSrvFamilies2},
    {  2,  3, "SRV_REPLYLOCATION", NULL},
    {  3,  3, "SRV_REPLYBUDDY",    SnacSrvReplybuddy},
    {  2, 11, "SRV_USERONLINE",    NULL},
    {  2, 12, "SRV_USEROFFLINE",   NULL},
    {  4,  5, "SRV_REPLYICBM",     SnacSrvReplyicbm},
    {  4,  7, "SRV_RECVMSG",       NULL},
    {  9,  3, "SRV_REPLYBOS",      SnacSrvReplaybos},
    { 19,  3, "SRV_REPLYUNKNOWN",  NULL},
    { 19,  6, "SRV_REPLYROSTER",   NULL},
    { 19, 14, "SRV_UPDATEACK",     NULL},
    { 19, 15, "SRV_REPLYROSTEROK", NULL},
    { 19, 28, "SRV_ADDEDYOU",      NULL},
    {  1,  2, "CLI_READY",         NULL},
    {  1,  6, "CLI_RATESREQUEST",  NULL},
    {  1,  8, "CLI_ACKRATES",      NULL},
    {  1, 14, "CLI_REQINFO",       NULL},
    {  1, 23, "CLI_FAMILIES",      NULL},
    {  1, 30, "CLI_SETSTATUS",     NULL},
    {  2,  2, "CLI_REQLOCATION",   NULL},
    {  2,  4, "CLI_SETUSERINFO",   NULL},
    {  3,  2, "CLI_REQBUDDY",      NULL},
    {  3,  4, "CLI_SENDCONTACTLIST", NULL},
    {  4,  2, "CLI_SETICBM",       NULL},
    {  4,  4, "CLI_REQICBM",       NULL},
    {  9,  2, "CLI_REQBOS",        NULL},
    { 19,  2, "CLI_REQUNKNOWN",    NULL},
    { 19,  5, "CLI_REQROSTER",     NULL},
    { 19,  7, "CLI_UNKNOWN1",      NULL},
    { 19,  8, "CLI_ADDBUDDY",      NULL},
    { 19,  9, "CLI_UPDATEGROUP",   NULL},
    { 19, 17, "CLI_ADDSTART",      NULL},
    { 19, 18, "CLI_ADDREND",       NULL},
    { 21,  2, "CLI_TOICQSRV",      NULL},
    {  0,  0, "unknown",           NULL}
};

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
    {
        if (s->f)
            s->f (event);
    }
    else
        M_print (i18n (903, "Unknown SNAC (%x,%x).\n"), family, pak->cmd);
}

const char *SnacName (UWORD fam, UWORD cmd)
{
    SNAC *s;
    
    for (s = SNACS; s->fam; s++)
        if (s->fam == fam && s->cmd == cmd)
            return s->name;
    return s->name;
}

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
    FlapSend (event->sess, pak);
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
}

/*
 * SRV_REPLYICBM - SNAC(4,5)
 * CLI_SETICBM - SNAC(4,2)
 */
JUMP_SNAC_F(SnacSrvReplyicbm)
{
   Packet *pak;
   
   pak = SnacC (event->sess, 4, 2, 0, 0);
   PacketWriteB2 (pak, 0);
   PacketWriteB2 (pak, 0);
   PacketWriteB2 (pak, 3);
   PacketWriteB2 (pak, 8000);
   PacketWriteB2 (pak, 999);
   PacketWriteB2 (pak, 999);
   PacketWriteB2 (pak, 0);
   PacketWriteB2 (pak, 0);
   FlapSend (event->sess, pak);
}

/*
 * SRV_REPLYBOS - SNAC(9,3)
 */
JUMP_SNAC_F(SnacSrvReplaybos)
{
    SnacCliSetuserinfo (event->sess);
    SnacCliSetstatus (event->sess);
    SnacCliReady (event->sess);
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
    FlapSend (sess, pak);
}

/*
 * CLI_SETSTATUS - SNAC(1,1E)
 */
void SnacCliSetstatus (Session *sess)
{
    Packet *pak;
    
    pak = SnacC (sess, 1, 0x1e, 0, 0);
    PacketWriteTLV4 (pak, 6, 0 /* |status */ );
    PacketWriteTLV2 (pak, 8, 0);
    PacketWriteB2 (pak, 0x0c); /* TLV 0C */
    PacketWriteB2 (pak, 0x25);
    PacketWriteB4 (pak, sess->our_local_ip);
    PacketWriteB4 (pak, sess->our_port);
    PacketWrite1  (pak, 0x04);
    PacketWriteB2 (pak, TCP_VER);
    PacketWriteB4 (pak, rand() % 0x7fff); /* TCP cookie */
    PacketWriteB2 (pak, 0);
    PacketWriteB2 (pak, 80);
    PacketWriteB2 (pak, 0);
    PacketWriteB2 (pak, 3);
    PacketWriteB4 (pak, time (NULL));
    PacketWriteB4 (pak, 0);
    PacketWriteB4 (pak, time (NULL));
    PacketWriteB2 (pak, 0);
    FlapSend (sess, pak);
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
    FlapSend (sess, pak);
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
    FlapSend (sess, pak);
}

/*
 * CLI_RATESREQUEST - SNAC(1,6)
 */
void SnacCliRatesrequest (Session *sess)
{
    FlapSend (sess, SnacC (sess, 1, 6, 0, 0));
}

/*
 * CLI_REQINFO - SNAC (1,E)
 */
void SnacCliReqinfo (Session *sess)
{
    FlapSend (sess, SnacC (sess, 1, 0xE, 0, 0));
}

/*
 * CLI_REQLOCATION - SNAC(2,2)
 */
void SnacCliReqlocation (Session *sess)
{
    FlapSend (sess, SnacC (sess, 2, 2, 0, 0));
}

/*
 * CLI_REQBUDDY - SNAC(3,2)
 */
void SnacCliBuddy (Session *sess)
{
    FlapSend (sess, SnacC (sess, 3, 2, 0, 0));
}

/*
 * CLI_REQICBM - SNAC(4,4)
 */
void SnacCliReqicbm (Session *sess)
{
    FlapSend (sess, SnacC (sess, 4, 4, 0, 0));
}

/*
 * CLI_REQBOS - SNAC(9,2)
 */
void SnacCliReqbos (Session *sess)
{
    FlapSend (sess, SnacC (sess, 9, 2, 0, 0));
}
