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
                M_print ("\n");
                break;
            }
            if ((sess->connect & CONNECT_MASK) < 3)
                FlapCliIdent (sess);
            else
            {
                FlapCliCookie (sess, tlv[6].str, tlv[6].len);
                TLVD (tlv);
                tlv = NULL;
            }
            break;
        default:
            M_print (i18n (883, "FLAP channel 1 unknown command %d.\n"), i);
    }
}

void FlapChannel4 (Session *sess, Packet *pak)
{
    int rc, flags;
    struct sockaddr_in sin;
    struct hostent *host;
    
    tlv = TLVRead (pak);
    if (tlv[8].len)
    {
        Time_Stamp ();
        M_print (" " ESC "«");
        if (!(sess->connect & CONNECT_OK))
            M_print (i18n (895, "Login failed:\n"));
        else
            M_print (i18n (896, "Server closed connection:\n"));
        M_print (i18n (859, "Error code: %d\n"), tlv[8].nr);
        if (tlv[1].len && tlv[1].nr != sess->uin)
            M_print (i18n (861, "UIN: %d\n"), tlv[1].nr);
        if (tlv[4].len)
            M_print (i18n (862, "URL: %s\n"), tlv[4].str);
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
        assert ((sess->connect & CONNECT_MASK) == 2);
        assert (tlv[5].len);
        assert (strchr (tlv[5].str, ':'));

        FlapCliGoodbye (sess);

        if (prG->verbose)
            M_print (i18n (898, "Redirect to server %s...\n"), tlv[5].str);

        sess->server_port = atoi (strchr (tlv[5].str, ':') + 1);
        *strchr (tlv[5].str, ':') = '\0';
        sess->server = strdup (tlv[5].str);

        sess->sok = socket (AF_INET, SOCK_STREAM, 0);
        if (sess->sok <= 0)
        {
            sess->sok = -1;
            sess->connect = 0;
            rc = errno;
            M_print (i18n (872, "failed: %s (%d)\n"), strerror (rc), rc);
            return;
        }

        flags = fcntl (sess->sok, F_GETFL, 0);
        if (flags != -1)
            flags = fcntl (sess->sok, F_SETFL, flags | O_NONBLOCK);
        if (flags == -1 && prG->verbose)
        {
            rc = errno;
            M_print (i18n (872, "failed: %s (%d)\n"), strerror (rc), rc);
            sockclose (sess->sok);
            sess->sok = -1;
            sess->connect = 0;
            return;
        }

        sin.sin_family = AF_INET;
        sin.sin_port = htons (sess->server_port);

        sin.sin_addr.s_addr = inet_addr (sess->server);
        if (sin.sin_addr.s_addr == -1)
        {
            host = gethostbyname (sess->server);
            if (!host)
            {
#ifdef HAVE_HSTRERROR
                M_print (i18n (874, "failed: can't find hostname %s: %s."), sess->server, hstrerror (h_errno));
#else
                M_print (i18n (875, "failed: can't find hostname %s."), sess->server);
#endif
                M_print ("\n");
                sockclose (sess->sok);
                sess->sok = -1;
                sess->connect = 0;
                return;
            }
            sin.sin_addr = *((struct in_addr *) host->h_addr);
        }
        
        sess->connect = 3 | CONNECT_SELECT_R;

        rc = connect (sess->sok, (struct sockaddr *) &sin, sizeof (struct sockaddr));
        if (rc >= 0)
        {
            M_print (i18n (873, "ok.\n"));
            return;
        }

        if ((rc = errno) == EINPROGRESS)
        {
            M_print ("\n");
            return;
        }
        M_print (i18n (872, "failed: %s (%d)\n"), strerror (rc), rc);
        sess->connect = 0;
        sockclose (sess->sok);
        sess->sok = -1;
    }
}

/***********************************************/

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
        M_print (" " ESC "«" COLCLIENT "");
        M_print (i18n (886, "Outgoing v8 server packet (FLAP): channel %d seq %08x length %d" COLNONE "\n"),
                 pak->cmd, pak->id, pak->len);
        if (pak->len > 6)
        {
            if (pak->cmd == 2 && pak->len >= 16)
                SnacPrint (pak);
            else
                Hex_Dump (pak->data + 6, pak->len - 6);
        }
        M_print (ESC "»\n");
    }
    
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

void FlapCliGoodbye (Session *sess)
{
    Packet *pak;
    
    if (!(sess->connect & (CONNECT_MASK | CONNECT_OK)))
        return;
    
    pak = FlapC (4);
    FlapSend (sess, pak);
    
    sockclose (sess->sok);
    sess->sok = -1;
    sess->connect = 0;
}


