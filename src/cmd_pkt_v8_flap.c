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

static TLV *tlv = NULL;
static void FlapChannel1 (Session *sess, Packet *pak);
static void FlapChannel4 (Session *sess, Packet *pak);

void SrvCallBackFlap (Event *event)
{
    assert (event->type == QUEUE_TYPE_FLAC);
    
    event->sess->stat_pak_rcvd++;
    event->sess->stat_real_pak_rcvd++;
    switch (event->pak->cmd)
    {
        case 1: /* Client login */
            FlapChannel1 (event->sess, event->pak);
            break;
        case 2: /* SNAC packets */
            SrvCallBackSnac (event);
            return;
        case 3: /* Errors */
            break;
        case 4: /* Logoff */
            FlapChannel4 (event->sess, event->pak);
            break;
        case 5: /* Ping */
        default:
            M_print (i18n (1884, "FLAP with unknown channel %d received.\n"), event->pak->cmd);
    }
    PacketD (event->pak);
    free (event);
}

/***********************************************/

static void FlapChannel1 (Session *sess, Packet *pak)
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
                Hex_Dump (pak->data + pak->rpos, PacketReadLeft (pak));
                break;
            }
            if (!sess->uin)
            {
                FlapCliHello (sess);
                SnacCliRegisteruser (sess);
            }
            else if (sess->connect & 8)
            {
                assert (tlv);
                FlapCliCookie (sess, tlv[6].str, tlv[6].len);
                TLVD (tlv);
                tlv = NULL;
            }
            else
                FlapCliIdent (sess);
            break;
        default:
            M_print (i18n (1883, "FLAP channel 1 unknown command %d.\n"), i);
    }
}

static void FlapChannel4 (Session *sess, Packet *pak)
{
    tlv = TLVRead (pak, PacketReadLeft (pak));
    if (!tlv[5].len)
    {
        Time_Stamp ();
        M_print (" " ESC "«");
        if (!(sess->connect & CONNECT_OK))
            M_print (i18n (1895, "Login failed:\n"));
        else
            M_print (i18n (1896, "Server closed connection:\n"));
        M_print (i18n (1048, "Error code: %d\n"), tlv[8].nr);
        if (tlv[1].len && tlv[1].nr != sess->uin)
            M_print (i18n (1049, "UIN: %d\n"), tlv[1].nr);
        if (tlv[4].len)
            M_print (i18n (1961, "URL: %s\n"), tlv[4].str);
        M_print (ESC "»\n");

        if ((sess->connect & CONNECT_MASK) && sess->sok != -1)
            sockclose (sess->sok);
        sess->connect = 0;
        sess->sok = -1;
        TLVD (tlv);
        tlv = NULL;
    }
    else
    {
        assert (strchr (tlv[5].str, ':'));

        M_print (i18n (1898, "Redirect to server %s... "), tlv[5].str);

        FlapCliGoodbye (sess);

        sess->port = atoi (strchr (tlv[5].str, ':') + 1);
        *strchr (tlv[5].str, ':') = '\0';
        sess->server = strdup (tlv[5].str);
        sess->ip = 0;

        sess->connect = 8;
        UtilIOConnectTCP (sess);
    }
}

/***********************************************/

void FlapPrint (Packet *pak)
{
    M_print (i18n (1910, "FLAP seq %08x length %04x channel %d" COLNONE "\n"),
             PacketReadAtB2 (pak, 2), pak->len - 6, PacketReadAt1 (pak, 1));
    if (PacketReadAt1 (pak, 1) != 2)
    {
        if (prG->verbose & DEB_PACK8DATA)
            Hex_Dump (pak->data + 6, pak->len - 6);
    }
    else 
    {
        M_print (i18n (1905, "SNAC (%x,%x) [%s] flags %x ref %x\n"),
                 PacketReadAtB2 (pak, 6), PacketReadAtB2 (pak, 8),
                 SnacName (PacketReadAtB2 (pak, 6), PacketReadAtB2 (pak, 8)),
                 PacketReadAtB2 (pak, 10), PacketReadAtB4 (pak, 12));
        M_print (COLNONE);
        if (prG->verbose & DEB_PACK8DATA)
            Hex_Dump (pak->data + 16, pak->len - 16);
    }
}

void FlapSave (Packet *pak, BOOL in)
{
    FILE *logf;
    char buf[200];
    
    snprintf (buf, sizeof (buf), "%s/debug", PrefUserDir ());
    mkdir (buf, 0700);
    snprintf (buf, sizeof (buf), "%s/debug/packets", PrefUserDir ());
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

void FlapSend (Session *sess, Packet *pak)
{
    if (!sess->our_seq)
        sess->our_seq = rand () & 0x7fff;
    sess->our_seq++;
    sess->our_seq &= 0x7fff;

    PacketWriteAtB2 (pak, 2, pak->id = sess->our_seq);
    PacketWriteAtB2 (pak, 4, pak->len - 6);
    
    if (prG->verbose & DEB_PACK8)
    {
        Time_Stamp ();
        M_print (" " ESC "«" COLCLIENT "%s ", i18n (1903, "Outgoing v8 server packet:"));
        FlapPrint (pak);
        M_print (ESC "»\r");
    }
    if (prG->verbose & DEB_PACK8SAVE)
        FlapSave (pak, FALSE);
    
    sess->stat_pak_sent++;
    sess->stat_real_pak_sent++;
    
    sockwrite (sess->sok, pak->data, pak->len);
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

    
void FlapCliIdent (Session *sess)
{
    Packet *pak;
    UWORD flags = prG->flags;

    prG->flags &= ~FLAG_CONVRUSS & ~FLAG_CONVEUC;
    if (!sess->passwd || !strlen (sess->passwd))
    {
#ifdef __BEOS__
        M_print (i18n (2063, "You need to save your password in your ~/.micq/micqrc file.\n"));
#else
        char pwd[20];
        pwd[0] = '\0';
        M_print ("%s ", i18n (1063, "Enter password:"));
        Echo_Off ();
        M_fdnreadln (stdin, pwd, sizeof (pwd));
        Echo_On ();
        sess->passwd = strdup (pwd);
#endif
    }
    
    pak = FlapC (1);
    PacketWriteB4 (pak, CLI_HELLO);
    PacketWriteTLVStr (pak, 1, UtilFill ("%d", sess->uin));
    PacketWriteTLVStr (pak, 2, _encryptpw (sess->passwd));
    PacketWriteTLVStr (pak, 3, "ICQ Inc. - Product of ICQ (TM).2001b.5.15.1.3638.85");
    PacketWriteTLV2   (pak, 22, 266);
    PacketWriteTLV2   (pak, 23, FLAP_VER_MAJOR);
    PacketWriteTLV2   (pak, 24, FLAP_VER_MINOR);
    PacketWriteTLV2   (pak, 25, FLAP_VER_LESSER);
    PacketWriteTLV2   (pak, 26, FLAP_VER_BUILD);
    PacketWriteTLV4   (pak, 20, FLAP_VER_SUBBUILD);
    PacketWriteTLVStr (pak, 15, "de");  /* en */
    PacketWriteTLVStr (pak, 14, "de");  /* en */
    FlapSend (sess, pak);
    prG->flags = flags;
}

void FlapCliCookie (Session *sess, const char *cookie, UWORD len)
{
    Packet *pak;

    pak = FlapC (1);
    PacketWriteB4 (pak, CLI_HELLO);
    PacketWriteTLVData (pak, 6, cookie, len);
    FlapSend (sess, pak);
}

void FlapCliHello (Session *sess)
{
    Packet *pak;
    
    pak = FlapC (1);
    PacketWriteB4 (pak, CLI_HELLO);
    FlapSend (sess, pak);
}

void FlapCliGoodbye (Session *sess)
{
    Packet *pak;
    
    if (!(sess->connect & CONNECT_MASK))
        return;
    
    pak = FlapC (4);
    FlapSend (sess, pak);
    
    sockclose (sess->sok);
    sess->sok = -1;
    sess->connect = 0;
}

void FlapCliKeepalive (Session *sess)
{
    FlapSend (sess, FlapC (5));
}
