/*
 * Handles incoming and creates outgoing SNAC packets
 * for the family 3 (contact) commands.
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
#include "oscar_tlv.h"
#include "oscar_snac.h"
#include "oscar_contact.h"
#include "packet.h"
#include "contact.h"
#include "connection.h"
#include "icq_response.h"

/*
 * SRV_CONTACTERR - SNAC(3,1)
 */
JUMP_SNAC_F(SnacSrvContacterr)
{
    Connection *serv = event->conn;
    UWORD err, cnt;
    Contact *cont;
    char first = 0, empty = 0;
    const char *errtxt;
    
    err = PacketReadB2 (event->pak);
    cnt = PacketReadB2 (event->pak);
    
    switch (err)
    {
        case 0x0e: errtxt = i18n (2329, "syntax error"); break;
        case 0x14: errtxt = i18n (2330, "removing non-contact"); break;
        default:   errtxt = i18n (2331, "unknown");
    }

    rl_printf (i18n (2332, "Contact error %d (%s) for %d contacts: "), err, errtxt, cnt);

    while (empty < 3)
    {
        if ((cont = PacketReadCont (event->pak, serv)))
        {
            if (first)
                rl_print (", ");
            if (cont)
                rl_printf ("%s (%ld)", cont->nick, cont->uin);
            first = 1;
        }
        else
            empty++;
    }
    rl_print ("\n");
}

/*
 * CLI_REQBUDDY - SNAC(3,2)
 */

/* implemented as macro */

/*
 * SRV_REPLYBUDDY - SNAC(3,3)
 */
JUMP_SNAC_F(SnacSrvReplybuddy)
{
    /* ignore all data, do nothing */
}

/*
 * CLI_ADDCONTACT - SNAC(3,4)
 */
void SnacCliAddcontact (Connection *serv, Contact *cont, ContactGroup *cg)
{
    Packet *pak;
    int i;
    
    if (cont && ContactPrefVal (cont, CO_LOCAL))
        return;
    
    pak = SnacC (serv, 3, 4, 0, 0);
    if (cont)
        PacketWriteCont (pak, cont);
    else
    {
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
        {
            if (ContactPrefVal (cont, CO_LOCAL))
                continue;
            if (i && !(i % 256))
            {
                SnacSend (serv, pak);
                pak = SnacC (serv, 3, 4, 0, 0);
            }
            PacketWriteCont (pak, cont);
            
        }
    }
    SnacSend (serv, pak);
}

/*
 * CLI_REMCONTACT - SNAC(3,5)
 */
void SnacCliRemcontact (Connection *serv, Contact *cont, ContactGroup *cg)
{
    Packet *pak;
    int i;
    
    pak = SnacC (serv, 3, 5, 0, 0);
    if (cont)
        PacketWriteCont (pak, cont);
    else
    {
        for (i = 0; (cont = ContactIndex (cg, i)); i++)
        {
            if (i && !(i % 256))
            {
                SnacSend (serv, pak);
                pak = SnacC (serv, 3, 5, 0, 0);
            }
            PacketWriteCont (pak, cont);
            
        }
    }
    SnacSend (serv, pak);
}

/*
 * SRV_REFUSE - SNAC(3,A)
 */
JUMP_SNAC_F(SnacSrvContrefused)
{
    Connection *serv = event->conn;
    Contact *cont;
    
    cont = PacketReadCont (event->pak, serv);
    if (cont)
        rl_printf (i18n (2315, "Cannot watch status of %s - too many watchers.\n"), cont->nick);
}

/*
 * SRV_USERONLINE - SNAC(3,B)
 */
JUMP_SNAC_F(SnacSrvUseronline)
{
    Connection *serv = event->conn;
    Contact *cont;
    Packet *p, *pak;
    TLV *tlv;
    
    pak = event->pak;
    cont = PacketReadCont (pak, serv);

    PacketReadB2 (pak);
    PacketReadB2 (pak);
    tlv = TLVRead (pak, PacketReadLeft (pak));
    if (tlv[10].str.len && CONTACT_DC (cont))
        cont->dc->ip_rem = tlv[10].nr;
    
    if (tlv[12].str.len && CONTACT_DC (cont))
    {
        UDWORD ip;
        p = PacketCreate (&tlv[12].str);
        
                       ip = PacketReadB4 (p);
        cont->dc->port    = PacketReadB4 (p);
        cont->dc->type    = PacketRead1  (p);
        cont->dc->version = PacketReadB2 (p);
        cont->dc->cookie  = PacketReadB4 (p);
                            PacketReadB4 (p);
                            PacketReadB4 (p);
        cont->dc->id1     = PacketReadB4 (p);
        cont->dc->id2     = PacketReadB4 (p);
        cont->dc->id3     = PacketReadB4 (p);
        /* remainder ignored */
        PacketD (p);
        if (ip && ~ip)
            cont->dc->ip_loc = ip;
    }
    /* TLV 1, d, f, 2, 3 ignored */
    if (!~cont->status)
        cont->caps = 0;

    if (tlv[13].str.len)
    {
        p = PacketCreate (&tlv[13].str);
        
        while (PacketReadLeft (p))
            ContactSetCap (cont, PacketReadCap (p));
        PacketD (p);
    }
    
    OptSetVal (&cont->copts, CO_TIMEONLINE, time (NULL) - tlv[15].nr);
    ContactSetVersion (cont);
    IMOnline (cont, serv, tlv[6].str.len ? tlv[6].nr : 0);
    TLVD (tlv);
}

/*
 * SRV_USEROFFLINE - SNAC(3,C)
 */
JUMP_SNAC_F(SnacSrvUseroffline)
{
    Connection *serv = event->conn;
    Contact *cont;
    Packet *pak;
    
    pak = event->pak;
    cont = PacketReadCont (pak, serv);

    IMOffline (cont, serv);
}

