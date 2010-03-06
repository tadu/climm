/*
 * Handles incoming and creates outgoing SNAC packets
 * for the family 1 (service) commands.
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
#include "util_ui.h"
#include "util_rl.h"
#include <assert.h>

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
    { 19,  4, NULL, NULL}, /* 5 */
    { 21,  1, NULL, NULL},
    { 34,  0, NULL, NULL},
    { 36,  0, NULL, NULL},
    { 37,  0, NULL, NULL},
    {  0,  0, NULL, NULL}
};


/*
 * Keeps track of sending a keep alive every 30 seconds.
 */
static void SrvCallBackKeepalive (Event *event)
{
    if (!event || !event->conn)
    {
        EventD (event);
        return;
    }
    ASSERT_SERVER_CONN (event->conn);
    if (event->conn->connect & CONNECT_OK)
    {
        FlapCliKeepalive (event->conn->serv);
        event->due = time (NULL) + 30;
        QueueEnqueue (event);
        return;
    }
    EventD (event);
}

void CliFinishLogin (Server *serv)
{
    Event *event;

    /* Step 4: (13,6)=(19,6) received */
    serv->conn->connect += 16;
    /* SnacCliAddcontact (serv, NULL, serv->contacts); */
    SnacCliSetstatus (serv, serv->status, 3);
    SnacCliReady (serv);
    SnacCliReqofflinemsgs (serv);
    if (prG->chat > 0)
        SnacCliSetrandom (serv, prG->chat);
    serv->conn->connect = CONNECT_OK | CONNECT_SELECT_R;
    QueueEnqueueData (serv->conn, QUEUE_SRV_KEEPALIVE, 0, time (NULL) + 30,
        NULL, serv->conn->cont, NULL, &SrvCallBackKeepalive);
    if ((event = QueueDequeue2 (serv->conn, QUEUE_DEP_WAITLOGIN, 0, NULL)))
    {
        event->due = time (NULL) + 5;
        QueueEnqueue (event);
    }
}

/*
 * SRV_SERVICEERR - SNAC(1,1)
 */
JUMP_SNAC_F (SnacSrvServiceerr)
{
    UWORD err;
    TLV *tlv;
    
    err = PacketReadB2 (event->pak);
    tlv = TLVRead (event->pak, PacketReadLeft (event->pak), -1);
    
    if (tlv[8].str.len)
        DebugH (DEB_PROTOCOL, "Server returned error code %d, sub code %ld for service family.", err, UD2UL (tlv[8].nr));
    else
        DebugH (DEB_PROTOCOL, "Server returned error %d for service family.", err);
    TLVD (tlv);
}

/*
 * CLI_READY - SNAC(1,2)
 */
void SnacCliReady (Server *serv)
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
    Server *serv = event->conn->serv;
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
    if (serv->conn->connect & CONNECT_OK)
        return;
    /* Step 1: (1,3) received) */
    serv->conn->connect += 16;
    SnacCliFamilies (serv);
    SnacCliRatesrequest (serv); /* triggers step 2 */
}

/*
 * SRV_RATES - SNAC(1,7)
 * CLI_ACKRATES - SNAC(1,8)
 */
JUMP_SNAC_F(SnacSrvRates)
{
    Server *serv = event->conn->serv;
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
}


/*
 * SRV_RATEEXCEEDED - SNAC(1,10)
 */
JUMP_SNAC_F(SnacSrvRateexceeded)
{
    Packet *pak = event->pak;
    UWORD code = PacketReadB2 (pak);
    if (code != 1)
        rl_print (i18n (2188, "You're sending data too fast - stop typing now, or the server will disconnect!\n"));
}

/*
 * SRV_SERVERPAUSE - SNAC(1,11)
 * SRV_MIGRATIONREQ - SNAC(1,18)
 */
JUMP_SNAC_F(SnacServerpause)
{
    Server *serv = event->conn->serv;
    ContactGroup *cg = serv->contacts;
    Contact *cont;
    int i;

#ifdef WIP
    rl_printf ("%s WIP: reconnecting because of serverpause.\n", s_time (NULL));
#endif
    OscarLogin (serv);
    for (i = 0; (cont = ContactIndex (cg, i)); i++)
        cont->status = ims_offline;
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
    Server *serv = event->conn->serv;
    Contact *cont;
    Packet *pak;
    TLV *tlv;
    UDWORD ostat, tlvc;
    status_t status;
    
    pak = event->pak;
    cont = PacketReadCont (pak, serv);
    
    if (strcmp (cont->screen, serv->screen))
        rl_printf (i18n (2609, "Warning: Server thinks our UIN is %s, when it is %s.\n"),
                  cont->screen, serv->screen);
    PacketReadB2 (pak);
    tlvc = PacketReadB2 (pak);
    tlv = TLVRead (pak, PacketReadLeft (pak), tlvc);
    if (tlv[10].str.len)
    {
        serv->conn->our_outside_ip = tlv[10].nr;
        if (prG->verbose)
            rl_printf (i18n (1915, "Server says we're at %s.\n"), s_ip (serv->conn->our_outside_ip));
    }
    if (tlv[6].str.len)
    {
        ostat = tlv[6].nr;
        status = IcqToStatus (ostat);
        if (status != serv->status)
        {
            serv->status = status;
            serv->nativestatus = ostat;
            ReadLinePromptReset ();
            rl_printf ("%s %s\n", s_now, s_status (serv->status, ostat));
        }
    }
    /* TLV 1 c f 2 3 ignored */
    TLVD (tlv);
}

/*
 * CLI_FAMILIES - SNAC(1,17)
 */
void SnacCliFamilies (Server *serv)
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
    Server *serv = event->conn->serv;
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

    if (serv->conn->connect & CONNECT_OK)
        return;
    /* Step 2: (1,24)=(1,18) received */
    serv->conn->connect += 16;
    SnacCliReqlocation  (serv);
    SnacCliReqbuddy     (serv);
    SnacCliReqicbm      (serv);
    SnacCliReqinfo      (serv);
    SnacCliReqbos       (serv); /* triggers step 3 */
}

/*
 * CLI_SETSTATUS - SNAC(1,1E)
 *
 * action: 1 = send status 2 = send connection info (3 = both)
 */
void SnacCliSetstatus (Server *serv, status_t status, UWORD action)
{
    Packet *pak;
    UDWORD ostat = IcqFromStatus (status);
    statusflag_t flags = imf_none;
    
    if (ServerPrefVal (serv, CO_WEBAWARE))
        flags |= imf_web;
    if (ServerPrefVal (serv, CO_DCAUTH))
        flags |= imf_dcauth;
    if (ServerPrefVal (serv, CO_DCCONT))
        flags |= imf_dccont;
    
    ostat |= IcqFromFlags (flags);
    
    if (serv->oscar_privacy_value != 1 && serv->oscar_privacy_value != 2 && serv->oscar_privacy_value != 5)
    {
        if (ContactIsInv (status) && serv->oscar_privacy_value != 3)
            SnacCliSetvisibility (serv, 3, 0);
        else if (!ContactIsInv (status) && serv->oscar_privacy_value != 4)
            SnacCliSetvisibility (serv, 4, 0);
    }
    
    pak = SnacC (serv, 1, 0x1e, 0, 0);
    if ((action & 1) && ContactIsInv (status))
        SnacCliAddvisible (serv, 0);
    if (action & 1)
        PacketWriteTLV4 (pak, 6, ostat);
    if (action & 2)
    {
        PacketWriteB2 (pak, 0x0c); /* TLV 0C */
        PacketWriteB2 (pak, 0x25);
        PacketWriteB4 (pak, ServerPrefVal (serv, CO_HIDEIP) ? 0 : serv->conn->our_local_ip);
        if (serv->oscar_dc && serv->oscar_dc->connect & CONNECT_OK)
        {
            PacketWriteB4 (pak, serv->oscar_dc->port);
            PacketWrite1  (pak, ServerPrefVal (serv, CO_OSCAR_DC_MODE) & 15);
            PacketWriteB2 (pak, serv->oscar_dc->version);
            PacketWriteB4 (pak, serv->oscar_dc->oscar_our_session);
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
        PacketWriteB4 (pak, BUILD_CLIMM);
        PacketWriteB4 (pak, BuildVersionNum);
        PacketWriteB4 (pak, BuildPlatformID);
        PacketWriteB2 (pak, 0);
        PacketWriteTLV2 (pak, 8, 0);
    }
    SnacSend (serv, pak);
    if ((action & 1) && !ContactIsInv (status))
        SnacCliAddinvis (serv, 0);
}
