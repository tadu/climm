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
#include <arpa/inet.h>

static TLV *tlv = NULL;
static void FlapChannel1 (Session *sess, Packet *pak);
static void FlapChannel4 (Session *sess, Packet *pak);

void SrvCallBackFlap (struct Event *event)
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
            M_print (i18n (884, "FLAP with unknown channel %d received.\n"), event->pak->cmd);
    }
    PacketD (event->pak);
    free (event);
}

/***********************************************/

void FlapChannel1 (Session *sess, Packet *pak)
{
    int i;

    if (PacketReadLeft (pak) < 4)
    {
        M_print (i18n (881, "FLAP channel 1 out of data.\n"));
        return;
    }
    i = PacketReadB4 (pak);
    switch (i)
    {
        case 1:
            if (PacketReadLeft (pak))
            {
                M_print (i18n (882, "FLAP channel 1 cmd 1 extra data:\n"));
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
            M_print (i18n (883, "FLAP channel 1 unknown command %d.\n"), i);
    }
}

void FlapChannel4 (Session *sess, Packet *pak)
{
    tlv = TLVRead (pak);
    if (tlv[8].len)
    {
        Time_Stamp ();
        M_print (" " ESC "«");
        if (!(sess->connect & CONNECT_OK))
            M_print (i18n (895, "Login failed:\n"));
        else
            M_print (i18n (896, "Server closed connection:\n"));
        M_print (i18n (48, "Error code: %d\n"), tlv[8].nr);
        if (tlv[1].len && tlv[1].nr != sess->uin)
            M_print (i18n (49, "UIN: %d\n"), tlv[1].nr);
        if (tlv[4].len)
            M_print (i18n (51, "URL: %s\n"), tlv[4].str);
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
        if (tlv[5].len)
        {
            assert (strchr (tlv[5].str, ':'));

            M_print (i18n (898, "Redirect to server %s... "), tlv[5].str);

            sess->port = atoi (strchr (tlv[5].str, ':') + 1);
            *strchr (tlv[5].str, ':') = '\0';
            sess->server = strdup (tlv[5].str);
            sess->ip = 0;

            sess->connect = 8;
            UtilIOConnectTCP (sess);
        }
    }
}

/***********************************************/

void FlapPrint (Packet *pak)
{
    if (PacketReadAt1 (pak, 1) == 2)
    {
        M_print (i18n (910, "FLAP seq %08x length %04x channel %d" COLNONE "\n"),
                 PacketReadBAt2 (pak, 2), pak->len - 6, PacketReadAt1 (pak, 1));
        SnacPrint (pak);
    }
    else
    {
        M_print (i18n (910, "FLAP seq %08x length %04x channel %d" COLNONE "\n"),
                 PacketReadBAt2 (pak, 2), pak->len - 6, PacketReadAt1 (pak, 1));
        Hex_Dump (pak->data + 6, pak->len - 6);
    }
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

    PacketWriteBAt2 (pak, 2, pak->id = sess->our_seq);
    PacketWriteBAt2 (pak, 4, pak->len - 6);
    
    if (prG->verbose & 128)
    {
        Time_Stamp ();
        M_print (" " ESC "«" COLCLIENT "%s ", i18n (57, "Outgoing v8 server packet:"));
        FlapPrint (pak);
        M_print (ESC "»\r");
    }
    
    sess->stat_pak_sent++;
    sess->stat_real_pak_sent++;
    
    sockwrite (sess->sok, pak->data, pak->len);
    PacketD (pak);
}

/***********************************************/

void FlapCliIdent (Session *sess)
{
    Packet *pak;

    char *_encryptpw (const char *pw)
    {
        char *cpw = strdup (pw), *p;
        const char *tb = "\xf3\x26\x81\xc4\x39\x86\xdb\x92"
                         "\x71\xa3\xb9\xe6\x53\x7a\x95\x7c";
        int i = 0;
        for (p = cpw; *p; p++, i++)
            *p ^= tb[i % 16];
        return cpw;
    }
    
    if (!sess->passwd || !strlen (sess->passwd))
    {
        char pwd[20];
        pwd[0] = '\0';
        M_print ("%s ", i18n (63, "Enter password:"));
        Echo_Off ();
        M_fdnreadln (stdin, pwd, sizeof (pwd));
        Echo_On ();
        sess->passwd = strdup (pwd);
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
