/*
 * Handles incoming and creates outgoing SNAC packets
 * for the family 23 (register) commands.
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
#include "oscar_flap.h"
#include "oscar_snac.h"
#include "oscar_contact.h"
#include "packet.h"
#include "contact.h"
#include "connection.h"
#include "conv.h"
#include "file_util.h"

/*
 * SRV_REGREFUSED - SNAC(17,1)
 */
JUMP_SNAC_F(SnacSrvRegrefused)
{
    Connection *serv = event->conn;
    M_print (i18n (1920, "Registration of new UIN refused.\n"));
    if (serv->flags & CONN_WIZARD)
    {
        M_print (i18n (1792, "I'm sorry, AOL doesn't want to give us a new UIN, probably because of too many new UIN requests from this IP. Please try again later.\n"));
        exit (0);
    }
#ifdef ENABLE_PEER2PEER
    ConnectionD (serv->assoc);
#endif
    ConnectionD (serv);
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
    M_printf (i18n (1762, "Your new UIN is: %ld.\n"), cont->uin);
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

        s_repl (&serv->contacts->name, s_sprintf ("contacts-icq8-%ld", cont->uin));
        M_print (i18n (1790, "Setup wizard finished. Congratulations to your new UIN!\n"));

        if (Save_RC () == -1)
            M_print (i18n (1679, "Sorry saving your personal reply messages went wrong!\n"));
#ifdef ENABLE_PEER2PEER
        serv->assoc->open (serv->assoc);
#endif
        serv->open (serv);
    }
    else
        M_print (i18n (9999, "You need to 'save' to write your new UIN to disc.\n"));
}

