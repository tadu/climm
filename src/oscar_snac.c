/*
 * Handles incoming and creates outgoing SNAC packets.
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
#include <assert.h>
#include "util_ui.h"
#include "util_syntax.h"
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
#include "oscar_register.h"
#include "contact.h"
#include "packet.h"
#include "connection.h"
#include "preferences.h"

jump_snac_f SnacSrvUnknown, SnacSrvIgnore, SnacSrvSetinterval;

static SNAC SNACS[] = {
    {  1,  1, "SRV_SERVICEERR",      SnacSrvServiceerr},
    {  1,  3, "SRV_FAMILIES",        SnacSrvFamilies},
    {  1,  7, "SRV_RATES",           SnacSrvRates},
    {  1, 10, "SRV_RATEEXCEEDED",    SnacSrvRateexceeded},
    {  1, 11, "SRV_SERVERPAUSE",     SnacServerpause},
    {  1, 15, "SRV_REPLYINFO",       SnacSrvReplyinfo},
    {  1, 18, "SRV_MIGRATIONREQ",    SnacServerpause},
    {  1, 19, "SRV_MOTD",            SnacSrvMotd},
    {  1, 21, "SRV_UNKNOWN",         SnacSrvIgnore },
    {  1, 24, "SRV_FAMILIES2",       SnacSrvFamilies2},
    {  1, 33, "SRV_BUDDYICON",       SnacSrvIgnore },
    {  2,  1, "SRV_LOCATIONERR",     SnacSrvLocationerr},
    {  2,  3, "SRV_REPLYLOCATION",   SnacSrvReplylocation},
    {  2,  6, "SRV_USERINFO",        SnacSrvUserinfo},
    {  3,  1, "SRV_CONTACTERR",      SnacSrvContacterr},
    {  3,  3, "SRV_REPLYBUDDY",      SnacSrvReplybuddy},
    {  3, 11, "SRV_USERONLINE",      SnacSrvUseronline},
    {  3, 10, "SRV_REFUSE",          SnacSrvContrefused},
    {  3, 12, "SRV_USEROFFLINE",     SnacSrvUseroffline},
    {  4,  1, "SRV_ICBMERR",         SnacSrvIcbmerr},
    {  4,  5, "SRV_REPLYICBM",       SnacSrvReplyicbm},
    {  4,  7, "SRV_RECVMSG",         SnacSrvRecvmsg},
    {  4, 10, "SRV_MISSEDICBM",      NULL },
    {  4, 11, "SRV/CLI_ACKMSG",      SnacSrvAckmsg},
    {  4, 12, "SRV_SRVACKMSG",       SnacSrvSrvackmsg},
    {  9,  1, "SRV_BOSERR",          SnacSrvBoserr},
    {  9,  3, "SRV_REPLYBOS",        SnacSrvReplybos},
    { 11,  2, "SRV_SETINTERVAL",     SnacSrvSetinterval},
    { 19,  3, "SRV_REPLYLISTS",      SnacSrvReplylists},
    { 19,  6, "SRV_REPLYROSTER",     SnacSrvReplyroster},
    { 19,  8, "SRV_ROSTERADD",       SnacSrvRosteradd},
    { 19,  9, "SRV_ROSTERUPDATE",    SnacSrvRosterupdate},
    { 19, 10, "SRV_ROSTERDELETE",    SnacSrvRosterdelete},
    { 19, 14, "SRV_UPDATEACK",       SnacSrvUpdateack},
    { 19, 15, "SRV_REPLYROSTEROK",   SnacSrvRosterok},
    { 19, 17, "SRV_ADDSTART",        SnacSrvAddstart},
    { 19, 18, "SRV_ADDEND",          SnacSrvAddend},
    { 19, 25, "SRV_AUTHREQ",         SnacSrvAuthreq},
    { 19, 27, "SRV_AUTHREPLY",       SnacSrvAuthreply},
    { 19, 28, "SRV_ADDEDYOU",        SnacSrvAddedyou},
    { 21,  1, "SRV_TOICQERR",        SnacSrvToicqerr},
    { 21,  3, "SRV_FROMICQSRV",      SnacSrvFromicqsrv},
    { 23,  1, "SRV_REGREFUSED",      SnacSrvRegrefused},
    { 23,  3, "SRV_REPLYLOGIN",      SnacSrvReplylogin},
    { 23,  5, "SRV_NEWUIN",          SnacSrvNewuin},
    { 23,  7, "SRV_LOGINKEY",        SnacSrvLoginkey},
    {  1,  2, "CLI_READY",           NULL},
    {  1,  6, "CLI_RATESREQUEST",    NULL},
    {  1,  8, "CLI_ACKRATES",        NULL},
    {  1, 12, "CLI_ACKSERVERPAUSE",  NULL},
    {  1, 14, "CLI_REQINFO",         NULL},
    {  1, 23, "CLI_FAMILIES",        NULL},
    {  1, 30, "CLI_SETSTATUS",       NULL},
    {  2,  2, "CLI_REQLOCATION",     NULL},
    {  2,  4, "CLI_SETUSERINFO",     NULL},
    {  2,  5, "CLI_REQUSERINFO",     NULL},
    {  3,  2, "CLI_REQBUDDY",        NULL},
    {  3,  4, "CLI_ADDCONTACT",      NULL},
    {  3,  5, "CLI_REMCONTACT",      NULL},
    {  4,  2, "CLI_SETICBM",         NULL},
    {  4,  4, "CLI_REQICBM",         NULL},
    {  4,  6, "CLI_SENDMSG",         NULL},
    {  9,  2, "CLI_REQBOS",          NULL},
    {  9,  5, "CLI_ADDVISIBLE",      NULL},
    {  9,  6, "CLI_REMVISIBLE",      NULL},
    {  9,  7, "CLI_ADDINVISIBLE",    NULL},
    {  9,  8, "CLI_REMINVISIBLE",    NULL},
    { 19,  2, "CLI_REQLISTS",        NULL},
    { 19,  4, "CLI_REQROSTER",       NULL},
    { 19,  5, "CLI_CHECKROSTER",     NULL},
    { 19,  7, "CLI_ROSTERACK",       NULL},
    { 19,  8, "CLI_ROSTERADD",       NULL},
    { 19,  9, "CLI_ROSTERUPDATE",    NULL},
    { 19, 10, "CLI_ROSTERDELETE",    NULL},
    { 19, 17, "CLI_ADDSTART",        NULL},
    { 19, 18, "CLI_ADDEND",          NULL},
    { 19, 20, "CLI_GRANTAUTH?",      NULL},
    { 19, 24, "CLI_REQAUTH",         NULL},
    { 19, 26, "CLI_AUTHORIZE",       NULL},
    { 21,  2, "CLI_TOICQSRV",        NULL},
    { 23,  2, "CLI_MD5LOGIN",        NULL},
    { 23,  4, "CLI_REGISTERUSER",    NULL},
    { 23,  6, "CLI_REQLOGIN",        NULL},
    {  0,  0, "unknown",             NULL}
};

/*
 * Interprets the given SNAC.
 */
void SnacCallback (Event *event)
{
    Packet  *pak  = event->pak;
    SNAC *s;
    UWORD family;
    Event *refevent;
    
    ASSERT_SERVER_CONN (event->conn);
    
    family     = PacketReadB2 (pak);
    pak->cmd   = PacketReadB2 (pak);
    pak->flags = PacketReadB2 (pak);
    pak->ref   = PacketReadB4 (pak);
    
    if (pak->flags & 0x8000)
        PacketReadData (pak, NULL, PacketReadB2 (pak));
    
    refevent = QueueDequeue (event->conn, QUEUE_OSCAR_REF, pak->ref);
    if (refevent)
    {
        UDWORD seq = pak->ref;
        pak->ref = family;
        refevent->pak = event->pak;
        event->pak = NULL;
        refevent->callback (refevent);
        event->pak = refevent->pak;
        refevent->pak = NULL;
        EventD (refevent);
        if (!event->pak)
        {
            EventD (event);
            return;
        }
        pak->ref = seq;
    }
    
    for (s = SNACS; s->fam; s++)
        if (s->fam == family && s->cmd == pak->cmd)
            break;
    
    if (s->f)
        s->f (event);
    else
        SnacSrvUnknown (event);

    EventD (event);
}

/*
 * Returns the name of the SNAC, or "unknown".
 */
const char *SnacName (UWORD fam, UWORD cmd)
{
    SNAC *s;
    
    for (s = SNACS; s->fam; s++)
        if (s->fam == fam && s->cmd == cmd)
            return s->name;
    return s->name;
}

/*
 * Creates a new SNAC.
 */
Packet *SnacC (Server *serv, UWORD fam, UWORD cmd, UWORD flags, UDWORD ref)
{
    Packet *pak;
    
    if (!serv->oscar_snac_seq)
        serv->oscar_snac_seq = rand () & 0x7fff;
    if (!ref)
        ref = rand () & 0x7fff;
    
    pak = FlapC (2);
    PacketWriteB2 (pak, fam);
    PacketWriteB2 (pak, cmd);
    PacketWriteB2 (pak, flags);
    PacketWriteB4 (pak, pak->ref = ref);
    return pak;
}

/*
 * Print a given SNAC.
 */
void SnacPrint (Packet *pak)
{
    UWORD fam, cmd, flag, opos = pak->rpos;
    UDWORD ref, len;

    pak->rpos = 6;
    fam  = PacketReadB2 (pak);
    cmd  = PacketReadB2 (pak);
    flag = PacketReadB2 (pak);
    ref  = PacketReadB4 (pak);
    len  = PacketReadAtB2 (pak, pak->rpos);

    rl_printf ("%s %sSNAC     (%x,%x) [%s] flags %x ref %lx",
             s_dumpnd (pak->data + 6, flag & 0x8000 ? 10 + len + 2 : 10),
             COLDEBUG, fam, cmd, SnacName (fam, cmd), flag, UD2UL (ref));
    if (flag & 0x8000)
    {
        rl_printf (" extra (%ld)", UD2UL (len));
        pak->rpos += len + 2;
    }
    rl_printf ("%s\n", COLNONE);

    if (prG->verbose & DEB_PACK8DATA || ~prG->verbose & DEB_PACK8)
    {
        if (fam != 3 || cmd != 11 || prG->verbose & DEB_PACK8DATAONLINE)
        {
            char *f, *syn = strdup (s_sprintf ("gs%dx%ds", fam, cmd));
            rl_print (f = PacketDump (pak, syn, COLDEBUG, COLNONE));
            free (f);
            free (syn);
        }
    }

    pak->rpos = opos;
}

/*
 * STFU.
 */
JUMP_SNAC_F(SnacSrvIgnore)
{
}


/*
 * Any unknown SNAC.
 */
JUMP_SNAC_F(SnacSrvUnknown)
{
    if (!(prG->verbose & DEB_PACK8))
    {
        rl_printf ("%s " COLINDENT "%s%s ", s_now, COLSERVER, i18n (1033, "Incoming v8 server packet:"));
        FlapPrint (event->pak);
        rl_print (COLEXDENT "\n");
    }
}

/*
 * SRV_SETINTERVAL - SNAC(b,2)
 */
JUMP_SNAC_F(SnacSrvSetinterval)
{
    Packet *pak;
    UWORD interval;

    pak = event->pak;
    interval = PacketReadB2 (pak);
    if (prG->verbose & DEB_PROTOCOL)
        rl_printf (i18n (1918, "Ignored server request for a minimum report interval of %d.\n"), 
            interval);
}

static JUMP_SNAC_F(cb_SnacRefCancel)
{
    EventD (event);
}

void SnacSendR (Server *serv, Packet *pak, jump_snac_f *f, void *data)
{
    QueueEnqueueData2 (serv->conn, QUEUE_OSCAR_REF, pak->ref, 600, data, f, &cb_SnacRefCancel);
    SnacSend (serv, pak);
}
