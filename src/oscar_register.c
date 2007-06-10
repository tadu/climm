/*
 * Handles incoming and creates outgoing SNAC packets
 * for the family 23 (register) commands.
 *
 * mICQ Copyright (C) © 2001-2007 Rüdiger Kuhlmann
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
 * In addition, as a special exception permission is granted to link the
 * code of this release of mICQ with the OpenSSL project's "OpenSSL"
 * library, and distribute the linked executables.  You must obey the GNU
 * General Public License in all respects for all of the code used other
 * than "OpenSSL".  If you modify this file, you may extend this exception
 * to your version of the file, but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your version
 * of this file.
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
#include "oscar_base.h"
#include "oscar_snac.h"
#include "oscar_contact.h"
#include "packet.h"
#include "contact.h"
#include "connection.h"
#include "conv.h"
#include "file_util.h"
#include "util_ssl.h"

/*
 * SRV_REGREFUSED - SNAC(17,1)
 */
JUMP_SNAC_F(SnacSrvRegrefused)
{
    Connection *serv = event->conn;
    rl_print (i18n (1920, "Registration of new UIN refused.\n"));
    if (serv->flags & CONN_WIZARD)
    {
        rl_print (i18n (1792, "I'm sorry, AOL doesn't want to give us a new UIN, probably because of too many new UIN requests from this IP. Please try again later.\n"));
        exit (0);
    }
    if (~serv->flags & CONN_CONFIGURED)
    {
#ifdef ENABLE_PEER2PEER
        ConnectionD (serv->assoc);
#endif
        ConnectionD (serv);
    }
}

/*
 * CLI_MD5LOGIN - SNAC(17,2)
 */
void SnacCliMd5login (Connection *serv, const char *hash)
{
    Packet *pak;

    /* send packet */
    pak = SnacC (serv, 23, 2, 0, 0);
    PacketWriteTLVStr  (pak, 1, serv->screen);
    PacketWriteTLVStr  (pak, 3, "ICQ Inc. - Product of ICQ (TM).2003b.5.37.1.3728.85");
    PacketWriteTLVData (pak, 37, hash, 16);
    PacketWriteTLV2    (pak, 22, 266);
    PacketWriteTLV2    (pak, 23, FLAP_VER_MAJOR);
    PacketWriteTLV2    (pak, 24, FLAP_VER_MINOR);
    PacketWriteTLV2    (pak, 25, FLAP_VER_LESSER);
    PacketWriteTLV2    (pak, 26, FLAP_VER_BUILD);
    PacketWriteTLV4    (pak, 20, FLAP_VER_SUBBUILD);
    PacketWriteTLVStr  (pak, 15, "de");  /* en */
    PacketWriteTLVStr  (pak, 14, "DE");  /* en */
    /* SSI use flag: 1 - only SSI, 0 - family 3 snacs? */
    PacketWriteTLV (pak, 74);
    PacketWrite1 (pak, 0);
    PacketWriteTLVDone (pak);
    SnacSend(serv, pak);
}

/*
 * SRV_REPLYLOGIN - SNAC(17,3)
 */
JUMP_SNAC_F(SnacSrvReplylogin)
{
    Connection *serv = event->conn;

    /* delegate to old login method */
    FlapChannel4 (serv, event->pak);
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
void SnacCliRegisteruser (Connection *serv)
{
    Packet *pak;
    
    assert (serv->passwd);
    assert (*serv->passwd);
    
    pak = SnacC (serv, 23, 4, 0, 0);
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
    PacketWriteLNTS (pak, c_out (serv->passwd));
    PacketWriteB4 (pak, REG_X2);
    PacketWriteB4 (pak, REG_X3);
    PacketWriteTLVDone (pak);
    SnacSend (serv, pak);
}

/*
 * SRV_NEWUIN - SNAC(17,5)
 */
JUMP_SNAC_F(SnacSrvNewuin)
{
    Connection *serv = event->conn;
    Contact *cont;
    UDWORD uin;

    uin = PacketReadAt4 (event->pak, 6 + 10 + 46);
    cont = ContactUIN (serv, uin);
    serv->uin = cont->uin;
    s_repl (&serv->screen, cont->screen);
    rl_print ("\n");
    rl_printf (i18n (2608, "Your new UIN is: %s.\n"), cont->screen);
    serv->flags |= CONN_CONFIGURED;
    if (serv->flags & CONN_WIZARD)
    {
        assert (serv->open);
#ifdef ENABLE_PEER2PEER
        assert (serv->assoc);
        assert (serv->assoc->open);
        serv->assoc->flags |= CONN_AUTOLOGIN;
#endif
        serv->flags |= CONN_AUTOLOGIN;
        serv->flags |= CONN_INITWP;

        s_repl (&serv->contacts->name, s_sprintf ("contacts-icq8-%s", cont->screen));
        rl_print (i18n (1790, "Setup wizard finished. Congratulations to your new UIN!\n"));
        rl_printf ("\n%s", COLERROR);
        rl_print (i18n (2381, "I'll add the author of mICQ to your contact list for your convenience. Don't abuse this opportunity - please use the help command and make a serious attempt to read the man pages and the FAQ before asking questions.\n"));
        rl_printf ("%s\n", COLNONE);

        if (Save_RC () == -1)
            rl_print (i18n (1679, "Sorry saving your personal reply messages went wrong!\n"));
#ifdef ENABLE_PEER2PEER
        serv->assoc->open (serv->assoc);
#endif
        serv->open (serv);
    }
    else
        rl_print (i18n (2518, "You need to 'save' to write your new UIN to disc.\n"));
}

/*
 * CLI_REQLOGIN - SNAC(17,6)
 */
void SnacCliReqlogin (Connection *serv)
{
    Packet *pak;

    pak = SnacC (serv, 23, 6, 0, 0);
    PacketWriteTLVStr (pak, 1, serv->screen);
    PacketWriteTLV (pak, 0x4B); /* unknown */
    PacketWriteTLVDone (pak);
    PacketWriteTLV (pak, 0x5A); /* unknown */
    PacketWriteTLVDone (pak);
    SnacSend (serv, pak);
}

/*
 * SRV_SRV_LOGINKEY - SNAC(17,7)
 */
JUMP_SNAC_F(SnacSrvLoginkey)
{
    Connection *serv = event->conn;
    char hash[16];
    ssl_md5ctx_t *ctx;
    int rc;
    size_t len = strlen (serv->passwd);
    strc_t key;
    
    assert (serv->passwd);
    assert (*serv->passwd);

    key = PacketReadB2Str (event->pak, NULL);
    if (!key->len)
        return;

    /* compute md5 hash */
#define AIM_MD5_STRING "AOL Instant Messenger (SM)"

    ctx = ssl_md5_init ();
    if (!ctx)
        return;
    ssl_md5_write (ctx, key->txt, key->len);
    ssl_md5_write (ctx, serv->passwd, len > 8 ? 8 : len);
    ssl_md5_write (ctx, AIM_MD5_STRING, strlen (AIM_MD5_STRING));
    rc = ssl_md5_final (ctx, hash);
    if (rc)
        return;

    SnacCliMd5login (serv, hash);
}
