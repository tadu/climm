/*
 * Handles incoming and creates outgoing SNAC packets
 * for the family 9 (bos) commands.
 *
 * climm Copyright (C) © 2001-2007 Rüdiger Kuhlmann
 *
 * climm is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 dated June, 1991.
 *
 * climm is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * In addition, as a special exception permission is granted to link the
 * code of this release of climm with the OpenSSL project's "OpenSSL"
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

#include "climm.h"
#include "oscar_base.h"
#include "oscar_snac.h"
#include "oscar_bos.h"
#include "oscar_tlv.h"
#include "packet.h"
#include "contact.h"
#include "connection.h"
#include "util_ui.h"
#include "oscar_roster.h"

/*
 * SRV_BOSERR - SNAC(1,1)
 */
JUMP_SNAC_F (SnacSrvBoserr)
{
    UWORD err;
    TLV *tlv;
    
    err = PacketReadB2 (event->pak);
    tlv = TLVRead (event->pak, PacketReadLeft (event->pak), -1);
    
    if (tlv[8].str.len)
        DebugH (DEB_PROTOCOL, "Server returned error code %d, sub code %ld for bos family.", err, UD2UL (tlv[8].nr));
    else
        DebugH (DEB_PROTOCOL, "Server returned error %d for bos family.", err);
    TLVD (tlv);
}

/*
 * CLI_REQBOS - SNAC(9,2)
 */

/* implemented as macro */

/*
 * SRV_REPLYBOS - SNAC(9,3)
 */
JUMP_SNAC_F(SnacSrvReplybos)
{
    Server *serv = event->conn->serv;
    if (serv->conn->connect & CONNECT_OK)
        return;
    /* Step 3: (9,3) received */
    serv->conn->connect += 16;
    SnacCliReqlists     (serv); /* triggers step 4 */
}

/*
 * CLI_ADDVISIBLE - SNAC(9,5)
 */
void SnacCliAddvisible (Server *serv, Contact *cont)
{
    ContactGroup *cg;
    Packet *pak;
    int i;
    
    cg = serv->contacts;
    pak = SnacC (serv, 9, 5, 0, 0);
    if (cont)
        PacketWriteCont (pak, cont);
    else
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
            if (ContactPrefVal (cont, CO_INTIMATE))
                PacketWriteCont (pak, cont);
    SnacSend (serv, pak);
}

/*
 * CLI_REMVISIBLE - SNAC(9,6)
 */
void SnacCliRemvisible (Server *serv, Contact *cont)
{
    Packet *pak;
    
    if (!cont)
        return;
    pak = SnacC (serv, 9, 6, 0, 0);
    PacketWriteCont (pak, cont);
    SnacSend (serv, pak);
}

/*
 * CLI_ADDINVIS - SNAC(9,7)
 */
void SnacCliAddinvis (Server *serv, Contact *cont)
{
    ContactGroup *cg;
    Packet *pak;
    int i;
    
    pak = SnacC (serv, 9, 7, 0, 0);
    cg = serv->contacts;
    if (cont)
        PacketWriteCont (pak, cont);
    else
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
            if (ContactPrefVal (cont, CO_HIDEFROM))
                PacketWriteCont (pak, cont);
    SnacSend (serv, pak);
}

/*
 * CLI_REMINVIS - SNAC(9,8)
 */
void SnacCliReminvis (Server *serv, Contact *cont)
{
    Packet *pak;
    
    if (!cont)
        return;
    pak = SnacC (serv, 9, 8, 0, 0);
    PacketWriteCont (pak, cont);
    SnacSend (serv, pak);
}
