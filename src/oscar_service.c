/*
 * Handles incoming and creates outgoing SNAC packets
 * for the family 1 (service) commands.
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
#include "oscar_service.h"
#include "oscar_location.h"
#include "oscar_contact.h"
#include "oscar_icbm.h"
#include "oscar_bos.h"
#include "oscar_roster.h"
#include "oscar_oldicq.h"
#include "packet.h"
#include "contact.h"
#include "connection.h"
#include "preferences.h"
#include "buildmark.h"

static void SrvCallBackKeepalive (Event *event);

static SNAC SNACv[] = {
    {  1,  4, NULL, NULL},
    {  2,  1, NULL, NULL},
    {  3,  1, NULL, NULL},
    {  4,  1, NULL, NULL},
    {  6,  1, NULL, NULL},
    {  8,  0, NULL, NULL},
    {  9,  1, NULL, NULL},
    { 10,  1, NULL, NULL},
    { 11,  1, NULL, NULL},
    { 12,  1, NULL, NULL},
    { 19,  4, NULL, NULL},
    { 21,  1, NULL, NULL},
    { 34,  0, NULL, NULL},
    {  0,  0, NULL, NULL}
};

/*
 * Keeps track of sending a keep alive every 30 seconds.
 */
static void SrvCallBackKeepalive (Event *event)
{
    if (event->conn && event->conn->connect & CONNECT_OK && event->conn->type == TYPE_SERVER)
    {
        FlapCliKeepalive (event->conn);
        event->due = time (NULL) + 30;
        QueueEnqueue (event);
        return;
    }
    EventD (event);
}

/*
 * CLI_READY - SNAC(1,2)
 */
void SnacCliReady (Connection *serv)
{
    Packet *pak;
    SNAC *s;

    pak = SnacC (serv, 1, 2, 0, 0);
    
    for (s = SNACv; s->fam; s++)
    {
        if (!s->cmd || s->fam == 12)
            continue;

        PacketWriteB2 (pak, s->fam);
        PacketWriteB2 (pak, s->cmd);
        PacketWriteB4 (pak, s->fam == 2 ? 0x0101047B : 0x0110047B);
    }
    SnacSend (serv, pak);
}

/*
 * SRV_FAMILIES - SNAC(1,3)
 */
JUMP_SNAC_F(SnacSrvFamilies)
{
    Connection *serv = event->conn;
    Packet *pak;
    SNAC *s;
    UWORD fam;

    pak = event->pak;
    while (PacketReadLeft (pak) >= 2)
    {
        fam = PacketReadB2 (pak);
        
        for (s = SNACv; s->fam; s++)
            if (s->fam == fam && !s->f)
                break;
        if (!s->fam)
        {
            rl_printf (i18n (1899, "Unknown family requested: %d\n"), fam);
            continue;
        }
    }
    SnacCliFamilies (serv);
}

/*
 * SRV_RATES - SNAC(1,7)
 * CLI_ACKRATES - SNAC(1,8)
 */
JUMP_SNAC_F(SnacSrvRates)
{
    Connection *serv = event->conn;
    UWORD nr, grp;
    Packet *pak;
    
    pak = SnacC (serv, 1, 8, 0, 0);
    nr = PacketReadB2 (event->pak); /* ignore the remainder */
    while (nr--)
    {
        grp = PacketReadB2 (event->pak);
        event->pak->rpos += 33;
        PacketWriteB2 (pak, grp);
    }
    SnacSend (serv, pak);

    if (!(serv->connect & CONNECT_OK))
        serv->connect++;

    SnacCliReqlocation  (serv);
    SnacCliReqbuddy     (serv);
    SnacCliReqicbm      (serv);
    SnacCliReqbos       (serv);
    SnacCliReqlists     (serv);
}


/*
 * SRV_RATEEXCEEDED - SNAC(1,10)
 */
JUMP_SNAC_F(SnacSrvRateexceeded)
{
    rl_print (i18n (2188, "You're sending data too fast - stop typing now, or the server will disconnect!\n"));
}

/*
 * SRV_SERVERPAUSE - SNAC(1,11)
 * SRV_MIGRATIONREQ - SNAC(1,18)
 */
JUMP_SNAC_F(SnacServerpause)
{
    Connection *serv = event->conn;
    ContactGroup *cg = serv->contacts;
    Contact *cont;
    int i;

#ifdef WIP
    rl_printf ("%s WIP: reconnecting because of serverpause.\n", s_time (NULL));
#endif
    ConnectionInitServer(serv);
    for (i = 0; (cont = ContactIndex (cg, i)); i++)
        cont->status = STATUS_OFFLINE;
}

/*
 * SRV_MOTD - SNAC(1,13)
 */
JUMP_SNAC_F(SnacSrvMotd)
{
    /* ignore */
}

/*
 * SRV_REPLYINFO - SNAC(1,15)
 */
JUMP_SNAC_F(SnacSrvReplyinfo)
{
    Connection *serv = event->conn;
    Contact *cont;
    Packet *pak;
    TLV *tlv;
    UDWORD status;
    
    pak = event->pak;
    cont = PacketReadCont (pak, serv);
    
    if (cont->uin != serv->uin)
        rl_printf (i18n (1907, "Warning: Server thinks our UIN is %ld, when it is %ld.\n"),
                  cont->uin, serv->uin);
    PacketReadB2 (pak);
    PacketReadB2 (pak);
    tlv = TLVRead (pak, PacketReadLeft (pak));
    if (tlv[10].str.len)
    {
        serv->our_outside_ip = tlv[10].nr;
        if (prG->verbose)
            rl_printf (i18n (1915, "Server says we're at %s.\n"), s_ip (serv->our_outside_ip));
        if (serv->assoc)
            serv->assoc->our_outside_ip = serv->our_outside_ip;
    }
    if (tlv[6].str.len)
    {
        status = tlv[6].nr;
        if (status != serv->status)
        {
            serv->status = status;
            rl_printf ("%s %s\n", s_now, s_status (serv->status));
        }
    }
    /* TLV 1 c f 2 3 ignored */
    TLVD (tlv);
    
    if (~serv->connect & CONNECT_OK)
    {
        SnacCliSetrandom (serv, prG->chat);
        serv->connect = CONNECT_OK | CONNECT_SELECT_R;
        QueueEnqueueData (serv, QUEUE_SRV_KEEPALIVE, 0, time (NULL) + 30,
                          NULL, event->cont, NULL, &SrvCallBackKeepalive);
        if ((event = QueueDequeue2 (serv, QUEUE_DEP_OSCARLOGIN, 0, NULL)))
        {
            event->due = time (NULL) + 5;
            QueueEnqueue (event);
        }
    }
}

/*
 * CLI_FAMILIES - SNAC(1,17)
 */
void SnacCliFamilies (Connection *serv)
{
    Packet *pak;
    SNAC *s;

    pak = SnacC (serv, 1, 0x17, 0, 0);
    
    for (s = SNACv; s->fam; s++)
    {
        if (!s->cmd || s->fam == 12)
            continue;

        PacketWriteB2 (pak, s->fam);
        PacketWriteB2 (pak, s->cmd);
    }
    SnacSend (serv, pak);
}

/*
 * SRV_FAMILIES2 - SNAC(1,18)
 */
JUMP_SNAC_F(SnacSrvFamilies2)
{
    Connection *serv = event->conn;
    Packet *pak;
    SNAC *s;
    UWORD fam, ver;
    
    pak = event->pak;
    while (PacketReadLeft (pak) >= 4)
    {
        fam = PacketReadB2 (pak);
        ver = PacketReadB2 (pak);
        for (s = SNACv; s->fam; s++)
            if (s->fam == fam)
                break;
        if (!s->fam)
            continue;
        if (s->cmd > ver)
            rl_printf (i18n (1904, "Server doesn't understand ver %d (only %d) for family %d!\n"), s->cmd, ver, fam);
    }

    if (!(serv->connect & CONNECT_OK))
        serv->connect++;
    SnacCliRatesrequest (serv);
}

/*
 * CLI_SETSTATUS - SNAC(1,1E)
 *
 * action: 1 = send status 2 = send connection info (3 = both)
 */
void SnacCliSetstatus (Connection *serv, UDWORD status, UWORD action)
{
    Packet *pak;
    
    if (ConnectionPrefVal (serv, CO_WEBAWARE))
        status |= STATUSF_WEBAWARE;
    if (ConnectionPrefVal (serv, CO_DCAUTH))
        status |= STATUSF_DC_AUTH;
    if (ConnectionPrefVal (serv, CO_DCCONT))
        status |= STATUSF_DC_CONT;
    
    pak = SnacC (serv, 1, 0x1e, 0, 0);
    if ((action & 1) && (status & STATUSF_INV))
        SnacCliAddvisible (serv, 0);
    if (action & 1)
        PacketWriteTLV4 (pak, 6, status);
    if (action & 2)
    {
        PacketWriteB2 (pak, 0x0c); /* TLV 0C */
        PacketWriteB2 (pak, 0x25);
        PacketWriteB4 (pak, ConnectionPrefVal (serv, CO_HIDEIP) ? 0 : serv->our_local_ip);
        if (serv->assoc && serv->assoc->connect & CONNECT_OK)
        {
            PacketWriteB4 (pak, serv->assoc->port);
            PacketWrite1  (pak, serv->assoc->status);
            PacketWriteB2 (pak, serv->assoc->version);
            PacketWriteB4 (pak, serv->assoc->our_session);
        }
        else
        {
            PacketWriteB4 (pak, 0);
            PacketWrite1  (pak, 1);
            PacketWriteB2 (pak, 8);
            PacketWriteB4 (pak, 0);
        }
        PacketWriteB2 (pak, 0);
        PacketWriteB2 (pak, 80);
        PacketWriteB2 (pak, 0);
        PacketWriteB2 (pak, 3);
        PacketWriteB4 (pak, BUILD_MICQ);
        PacketWriteB4 (pak, BuildVersionNum);
        PacketWriteB4 (pak, BuildPlatformID);
        PacketWriteB2 (pak, 0);
        PacketWriteTLV2 (pak, 8, 0);
    }
    SnacSend (serv, pak);
    if ((action & 1) && (~status & STATUSF_INV))
        SnacCliAddinvis (serv, 0);
}

