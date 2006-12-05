/*
 * Handles incoming and creates outgoing SNAC packets
 * for the family 2 (location) commands.
 *
 * mICQ Copyright (C) © 2001-2005 Rüdiger Kuhlmann
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
#include "oscar_base.h"
#include "oscar_snac.h"
#include "oscar_location.h"
#include "conv.h"
#include "contact.h"
#include "packet.h"

#define PEEK_REFID  0x1eadbeef
#define PEEK_REFID2 0x2eadbeef


/*
 * SRV_LOCATIONERR - SNAC(2,1)
 */
JUMP_SNAC_F(SnacSrvLocationerr)
{
    UWORD err = PacketReadB2 (event->pak);

    if (event->pak->ref == PEEK_REFID2)
        ;
    else if (err == 4)
        rl_print (i18n (2022, "The user is offline.\n"));
    else if (err != 0xd)
        rl_printf (i18n (2583, "Location error: %d.\n"), err);
}


/*
 * CLI_REQLOCATION - SNAC(2,2)
 */

/* implemented as macro */

/*
 * SRV_REPLYLOCATION - SNAC(2,3)
 */
JUMP_SNAC_F(SnacSrvReplylocation)
{
    Connection *serv = event->conn;
    SnacCliSetuserinfo (serv);
}

/*
 * CLI_SETUSERINFO - SNAC(2,4)
 */
void SnacCliSetuserinfo (Connection *serv)
{
    Packet *pak;
    
    pak = SnacC (serv, 2, 4, 0, 0);
    PacketWriteTLV     (pak, 5);
    PacketWriteCapID   (pak, CAP_ISICQ);
    PacketWriteCapID   (pak, CAP_SRVRELAY);
    PacketWriteCapID   (pak, CAP_UTF8);
    PacketWriteCapID   (pak, CAP_AIM_INTER);
    PacketWriteCapID   (pak, CAP_MICQ);
    PacketWriteTLVDone (pak);
    SnacSend (serv, pak);
}

/*
 * SRV_USERINFO - SNAC(2,5)
 */
JUMP_SNAC_F(SnacSrvUserinfo)
{
    Connection *serv = event->conn;
    Contact *cont = PacketReadCont (event->pak, serv);
        
    if (event->pak->ref == PEEK_REFID || event->pak->ref == PEEK_REFID2)
    {
        rl_log_for (cont->nick, COLCONTACT);
        rl_print (i18n (2017, "The user is online, but possibly invisible.\n"));
    }
}

/*
 * CLI_REQUSERINFO - SNAC(2,5)
 */
void SnacCliRequserinfo (Connection *serv, Contact *victim, UWORD type)
{
    Packet *pak;
    
    pak = SnacC (serv, 2, 5, 0, type == (UWORD)-1 ? PEEK_REFID2 : PEEK_REFID);
    PacketWriteB2 (pak, type == (UWORD)-1 ? 2 : type);
    PacketWriteCont (pak, victim);
    SnacSend (serv, pak);
}

