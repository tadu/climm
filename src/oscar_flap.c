/*
 * Decodes and creates FLAPs.
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
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include "util_ui.h"
#include "util_io.h"
#include "util_syntax.h"
#include "oscar_tlv.h"
#include "oscar_flap.h"
#include "oscar_snac.h"
#include "oscar_register.h"
#include "preferences.h"
#include "connection.h"
#include "packet.h"
#include "contact.h"
#include "conv.h"

static void FlapChannel1 (Connection *conn, Packet *pak);
static void FlapChannel4 (Connection *conn, Packet *pak);

void SrvCallBackFlap (Event *event)
{
    Connection *conn;

    if (!event->conn)
    {
        EventD (event);
        return;
    }
    
    assert (event->type == QUEUE_FLAP);
    ASSERT_SERVER (event->conn);
    
    conn = event->conn;

    event->pak->tpos = event->pak->rpos;
    event->pak->cmd = PacketRead1 (event->pak);
    event->pak->id  = PacketReadB2 (event->pak);
                      PacketReadB2 (event->pak);
    
    conn->stat_pak_rcvd++;
    conn->stat_real_pak_rcvd++;
    switch (event->pak->cmd)
    {
        case 1: /* Client login */
            FlapChannel1 (conn, event->pak);
            break;
        case 2: /* SNAC packets */
            SnacCallback (event);
            return;
        case 3: /* Errors */
            break;
        case 4: /* Logoff */
            FlapChannel4 (conn, event->pak);
            break;
        case 5: /* Ping */
        default:
            M_printf (i18n (1884, "FLAP with unknown channel %ld received.\n"), event->pak->cmd);
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
                FlapCliCookie (conn, tlv[6].str.txt, tlv[6].str.len);
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
    if (!tlv[5].str.len)
    {
        M_printf ("%s " COLINDENT, s_now);
        if (!(conn->connect & CONNECT_OK))
            M_print (i18n (1895, "Login failed:\n"));
        else
            M_print (i18n (1896, "Server closed connection:\n"));
        M_printf (i18n (1048, "Error code: %ld\n"), tlv[9].nr ? tlv[9].nr : tlv[8].nr);
        if (tlv[1].str.len && (UDWORD)atoi (tlv[1].str.txt) != conn->uin)
            M_printf (i18n (2218, "UIN: %s\n"), tlv[1].str.txt);
        if (tlv[4].str.len)
            M_printf (i18n (1961, "URL: %s\n"), tlv[4].str.txt);
        M_print (COLEXDENT "\n");
        
        if (tlv[8].nr == 24)
            M_print (i18n (2328, "You logged in too frequently, please wait 30 minutes before trying again.\n"));

        if ((conn->connect & CONNECT_MASK) && conn->sok != -1)
            sockclose (conn->sok);
        conn->connect = 0;
        conn->sok = -1;
        TLVD (tlv);
        tlv = NULL;
    }
    else
    {
        assert (strchr (tlv[5].str.txt, ':'));
        FlapCliGoodbye (conn);

        conn->port = atoi (strchr (tlv[5].str.txt, ':') + 1);
        *strchr (tlv[5].str.txt, ':') = '\0';
        s_repl (&conn->server, tlv[5].str.txt);
        conn->ip = 0;

        M_printf (i18n (9999, "Redirect to server %s:%s%ld%s... "),
                  s_wordquote (conn->server), COLQUOTE, conn->port, COLNONE);

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

    M_printf (COLEXDENT "%s\n  " COLINDENT "%s %sFLAP  ch %d seq %08x length %04x%s\n",
              COLNONE, s_dumpnd (pak->data, 6), COLDEBUG, ch, seq, len, COLNONE);

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
    UDWORD oldrpos = pak->rpos;
    char buf[200];
    
    snprintf (buf, sizeof (buf), "%sdebug", PrefUserDir (prG));
    mkdir (buf, 0700);
    snprintf (buf, sizeof (buf), "%sdebug" _OS_PATHSEPSTR "packets", PrefUserDir (prG));
    if (!(logf = fopen (buf, "a")))
        return;

    fprintf (logf, "%s FLAP seq %08x length %04x channel %d\n",
             in ? "<<<" : ">>>", PacketReadAtB2 (pak, 2),
             pak->len - 6, PacketReadAt1 (pak, 1));
    if (PacketReadAt1 (pak, 1) != 2)
        pak->rpos = 6;
    else
    {
        fprintf (logf, "SNAC (%x,%x) [%s] flags %x ref %lx\n",
                 PacketReadAtB2 (pak, 6), PacketReadAtB2 (pak, 8),
                 SnacName (PacketReadAtB2 (pak, 6), PacketReadAtB2 (pak, 8)),
                 PacketReadAtB2 (pak, 10), PacketReadAtB4 (pak, 12));
        pak->rpos = 16;
    }
    fprintf (logf, PacketDump (pak, "", "", ""));
    pak->rpos = oldrpos;
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
        M_printf ("%s " COLINDENT "%s%s ", s_now, COLCLIENT, i18n (1903, "Outgoing v8 server packet:"));
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
    char *f;

    if (!conn->passwd || !*conn->passwd)
    {
        strc_t pwd;
#ifdef __BEOS__
        M_print (i18n (2063, "You need to save your password in your ~/.micq/micqrc file.\n"));
#else
        M_printf ("%s ", i18n (1063, "Enter password:"));
        Echo_Off ();
        pwd = UtilIOReadline (stdin);
        Echo_On ();
        conn->passwd = strdup (pwd ? ConvFrom (pwd, prG->enc_loc)->txt : "");
#endif
    }
    
    pak = FlapC (1);
    PacketWriteB4 (pak, CLI_HELLO);
    PacketWriteTLVStr  (pak, 1, s_sprintf ("%ld", conn->uin));
    PacketWriteTLVData (pak, 2, f = _encryptpw (conn->passwd), strlen (conn->passwd));
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
    free (f);
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
