/*
 * Decodes and creates FLAPs.
 *
 * This file is Copyright © Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 */

#include "micq.h"
#include "cmd_pkt_v8_flap.h"
#include "cmd_pkt_v8_snac.h"
#include "util.h"
#include "util_ui.h"
#include "util_io.h"
#include "preferences.h"
#include "session.h"
#include "packet.h"
#include "file_util.h"
#include "cmd_pkt_v8_tlv.h"
#include "util_str.h"
#include "conv.h"
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <sys/stat.h>

static void FlapChannel1 (Connection *conn, Packet *pak);
static void FlapChannel4 (Connection *conn, Packet *pak);

void SrvCallBackFlap (Event *event)
{
    assert (event->type == QUEUE_FLAP);

    if (!event->conn)
    {
        EventD (event);
        return;
    }
    
    event->conn->stat_pak_rcvd++;
    event->conn->stat_real_pak_rcvd++;
    switch (event->pak->cmd)
    {
        case 1: /* Client login */
            FlapChannel1 (event->conn, event->pak);
            break;
        case 2: /* SNAC packets */
            SnacCallback (event);
            return;
        case 3: /* Errors */
            break;
        case 4: /* Logoff */
            FlapChannel4 (event->conn, event->pak);
            break;
        case 5: /* Ping */
        default:
            M_printf (i18n (1884, "FLAP with unknown channel %d received.\n"), event->pak->cmd);
    }
    EventD (event);
}

/***********************************************/

static void FlapChannel1 (Connection *conn, Packet *pak)
{
    int i;

    if (PacketReadLeft (pak) < 4)
    {
        M_print (i18n (1881, "FLAP channel 1 out of data.\n"));
        return;
    }
    i = PacketReadB4 (pak);
    switch (i)
    {
        case 1:
            if (PacketReadLeft (pak))
            {
                M_print (i18n (1882, "FLAP channel 1 cmd 1 extra data:\n"));
                M_print (s_dump (pak->data + pak->rpos, PacketReadLeft (pak)));
                break;
            }
            if (!conn->uin)
            {
                FlapCliHello (conn);
                SnacCliRegisteruser (conn);
            }
            else if (conn->connect & 8)
            {
                TLV *tlv = conn->tlv;
                
                assert (tlv);
                FlapCliCookie (conn, tlv[6].str, tlv[6].len);
                TLVD (tlv);
                tlv = NULL;
            }
            else
                FlapCliIdent (conn);
            break;
        default:
            M_printf (i18n (1883, "FLAP channel 1 unknown command %d.\n"), i);
    }
}

static void FlapChannel4 (Connection *conn, Packet *pak)
{
    TLV *tlv;
    
    tlv = TLVRead (pak, PacketReadLeft (pak));
    if (!tlv[5].len)
    {
        M_printf ("%s " COLINDENT, s_now);
        if (!(conn->connect & CONNECT_OK))
            M_print (i18n (1895, "Login failed:\n"));
        else
            M_print (i18n (1896, "Server closed connection:\n"));
        M_printf (i18n (1048, "Error code: %d\n"), tlv[9].nr ? tlv[9].nr : tlv[8].nr);
        if (tlv[1].len && atoi (tlv[1].str) != conn->uin)
            M_printf (i18n (2218, "UIN: %s\n"), tlv[1].str);
        if (tlv[4].len)
            M_printf (i18n (1961, "URL: %s\n"), tlv[4].str);
        M_print (COLEXDENT "\n");

        if ((conn->connect & CONNECT_MASK) && conn->sok != -1)
            sockclose (conn->sok);
        conn->connect = 0;
        conn->sok = -1;
        TLVD (tlv);
        tlv = NULL;
    }
    else
    {
        assert (strchr (tlv[5].str, ':'));

        M_printf (i18n (1898, "Redirect to server %s... "), tlv[5].str);

        FlapCliGoodbye (conn);

        conn->port = atoi (strchr (tlv[5].str, ':') + 1);
        *strchr (tlv[5].str, ':') = '\0';
        conn->server = strdup (tlv[5].str);
        conn->ip = 0;

        conn->connect = 8;
        conn->tlv = tlv;
        UtilIOConnectTCP (conn);
    }
}

/***********************************************/

void FlapPrint (Packet *pak)
{
    UWORD opos = pak->rpos;
    UBYTE ch;
    UWORD seq, len;

    pak->rpos = 1;
    
    ch  = PacketRead1 (pak);
    seq = PacketReadB2 (pak);
    len = PacketReadB2 (pak);

    M_printf (COLEXDENT COLNONE "\n  " COLINDENT "%s " COLDEBUG "FLAP  ch %d seq %08x length %04x" COLNONE "\n",
             s_dumpnd (pak->data, 6), ch, seq, len);

    if (ch == 2)
        SnacPrint (pak);
    else
        if (prG->verbose & DEB_PACK8DATA || ~prG->verbose & DEB_PACK8)
            M_print (s_dump (pak->data + 6, pak->len - 6));

    pak->rpos = opos;
}

void FlapSave (Packet *pak, BOOL in)
{
    FILE *logf;
    char buf[200];
    
    snprintf (buf, sizeof (buf), "%s/debug", PrefUserDir (prG));
    mkdir (buf, 0700);
    snprintf (buf, sizeof (buf), "%s/debug/packets", PrefUserDir (prG));
    if (!(logf = fopen (buf, "a")))
        return;

    fprintf (logf, "%s FLAP seq %08x length %04x channel %d\n",
             in ? "<<<" : ">>>", PacketReadAtB2 (pak, 2),
             pak->len - 6, PacketReadAt1 (pak, 1));
    if (PacketReadAt1 (pak, 1) != 2)
        fHexDump (logf, pak->data + 6, pak->len - 6);
    else
    {
        fprintf (logf, i18n (1905, "SNAC (%x,%x) [%s] flags %x ref %x\n"),
                 PacketReadAtB2 (pak, 6), PacketReadAtB2 (pak, 8),
                 SnacName (PacketReadAtB2 (pak, 6), PacketReadAtB2 (pak, 8)),
                 PacketReadAtB2 (pak, 10), PacketReadAtB4 (pak, 12));
        fHexDump (logf, pak->data + 16, pak->len - 16);
    }
    fclose (logf);
}

Packet *FlapC (UBYTE channel)
{
    Packet *pak;
    
    pak = PacketC ();
    
    assert (pak);
    
    PacketWrite1 (pak, 0x2a);
    PacketWrite1 (pak, pak->cmd = channel);
    PacketWrite4 (pak, 0);
    
    return pak;
}

void FlapSend (Connection *conn, Packet *pak)
{
    if (!conn->our_seq)
        conn->our_seq = rand () & 0x7fff;
    conn->our_seq++;
    conn->our_seq &= 0x7fff;

    PacketWriteAtB2 (pak, 2, pak->id = conn->our_seq);
    PacketWriteAtB2 (pak, 4, pak->len - 6);
    
    if (prG->verbose & DEB_PACK8)
    {
        M_printf ("%s " COLINDENT COLCLIENT "%s ", s_now, i18n (1903, "Outgoing v8 server packet:"));
        FlapPrint (pak);
        M_print (COLEXDENT "\r");
    }
    if (prG->verbose & DEB_PACK8SAVE)
        FlapSave (pak, FALSE);
    
    conn->stat_pak_sent++;
    conn->stat_real_pak_sent++;
    
    sockwrite (conn->sok, pak->data, pak->len);
    PacketD (pak);
}

/***********************************************/

static char *_encryptpw (const char *pw)
{
    char *cpw = strdup (pw), *p;
    const char *tb = "\xf3\x26\x81\xc4\x39\x86\xdb\x92"
                     "\x71\xa3\xb9\xe6\x53\x7a\x95\x7c";
    int i = 0;
    for (p = cpw; *p; p++, i++)
        *p ^= tb[i % 16];
    return cpw;
}

    
void FlapCliIdent (Connection *conn)
{
    Packet *pak;

    if (!conn->passwd || !strlen (conn->passwd))
    {
#ifdef __BEOS__
        M_print (i18n (2063, "You need to save your password in your ~/.micq/micqrc file.\n"));
#else
        char pwd[20];
        pwd[0] = '\0';
        M_printf ("%s ", i18n (1063, "Enter password:"));
        Echo_Off ();
        M_fdnreadln (stdin, pwd, sizeof (pwd));
        Echo_On ();
#ifdef ENABLE_UTF8
        conn->passwd = strdup (ConvToUTF8 (pwd, prG->enc_loc));
#else
        conn->passwd = strdup (pwd);
#endif
#endif
    }
    
    pak = FlapC (1);
    PacketWriteB4 (pak, CLI_HELLO);
    PacketWriteTLVStr  (pak, 1, s_sprintf ("%d", conn->uin));
    PacketWriteTLVData (pak, 2, _encryptpw (conn->passwd), strlen (conn->passwd));
    PacketWriteTLVStr  (pak, 3, "ICQ Inc. - Product of ICQ (TM).2002a.5.37.1.3728.85");
    PacketWriteTLV2    (pak, 22, 266);
    PacketWriteTLV2    (pak, 23, FLAP_VER_MAJOR);
    PacketWriteTLV2    (pak, 24, FLAP_VER_MINOR);
    PacketWriteTLV2    (pak, 25, FLAP_VER_LESSER);
    PacketWriteTLV2    (pak, 26, FLAP_VER_BUILD);
    PacketWriteTLV4    (pak, 20, FLAP_VER_SUBBUILD);
    PacketWriteTLVStr  (pak, 15, "de");  /* en */
    PacketWriteTLVStr  (pak, 14, "de");  /* en */
    FlapSend (conn, pak);
}

void FlapCliCookie (Connection *conn, const char *cookie, UWORD len)
{
    Packet *pak;

    pak = FlapC (1);
    PacketWriteB4 (pak, CLI_HELLO);
    PacketWriteTLVData (pak, 6, cookie, len);
    FlapSend (conn, pak);
}

void FlapCliHello (Connection *conn)
{
    Packet *pak;
    
    pak = FlapC (1);
    PacketWriteB4 (pak, CLI_HELLO);
    FlapSend (conn, pak);
}

void FlapCliGoodbye (Connection *conn)
{
    Packet *pak;
    
    if (!(conn->connect & CONNECT_MASK))
        return;
    
    pak = FlapC (4);
    FlapSend (conn, pak);
    
    sockclose (conn->sok);
    conn->sok = -1;
    conn->connect = 0;
}

void FlapCliKeepalive (Connection *conn)
{
    FlapSend (conn, FlapC (5));
}
