/*
 * Handles incoming and creates outgoing SNAC packets
 * for the family 9 (bos) commands.
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
#include "oscar_base.h"
#include "oscar_snac.h"
#include "oscar_bos.h"
#include "packet.h"
#include "contact.h"
#include "connection.h"

/*
 * CLI_REQBOS - SNAC(9,2)
 */

/* implemented as macro */

/*
 * SRV_REPLYBOS - SNAC(9,3)
 */
JUMP_SNAC_F(SnacSrvReplybos)
{
    /* ignore */
}

/*
 * CLI_ADDVISIBLE - SNAC(9,5)
 */
void SnacCliAddvisible (Connection *serv, Contact *cont)
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
void SnacCliRemvisible (Connection *serv, Contact *cont)
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
void SnacCliAddinvis (Connection *serv, Contact *cont)
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
void SnacCliReminvis (Connection *serv, Contact *cont)
{
    Packet *pak;
    
    if (!cont)
        return;
    pak = SnacC (serv, 9, 8, 0, 0);
    PacketWriteCont (pak, cont);
    SnacSend (serv, pak);
}
