/*
 * Initialization and basic support for v8 of the ICQ protocol.
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
#include "util.h"
#include "util_ui.h"
#include "util_io.h"
#include "buildmark.h"
#include "icq_response.h"
#include "conv.h"
#include "contact.h"
#include "session.h"
#include "packet.h"
#include "preferences.h"
#include "cmd_pkt_v8_flap.h"
#include "cmd_pkt_v8_snac.h"
#include "cmd_pkt_v8.h"
#include "util_ssl.h"
#include "tcp.h"
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#if HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#include <assert.h>

jump_conn_f SrvCallBackReceive;
static jump_conn_f SrvCallBackReconn;
static void SrvCallBackTimeout (Event *event);
static void SrvCallBackDoReconn (Event *event);

int reconn = 0;

void ConnectionInitServer (Connection *conn)
{
    if (!conn->server || !*conn->server || !conn->port)
        return;

    conn->cont = ContactUIN (conn, conn->uin);
    conn->our_seq  = rand () & 0x7fff;
    conn->connect  = 0;
    conn->dispatch = &SrvCallBackReceive;
    conn->reconnect= &SrvCallBackReconn;
    conn->close    = &FlapCliGoodbye;
    s_repl (&conn->server, conn->pref_server);
    if (conn->status == STATUS_OFFLINE)
        conn->status = conn->pref_status;
    QueueEnqueueData (conn, conn->connect, conn->our_seq,
                      time (NULL) + 10,
                      NULL, conn->cont, NULL, &SrvCallBackTimeout);

    M_printf (i18n (9999, "Opening v8 connection to %s:%s%ld%s... "),
              s_wordquote (conn->server), COLQUOTE, conn->port, COLNONE);

    UtilIOConnectTCP (conn);
}

static void SrvCallBackReconn (Connection *conn)
{
    ContactGroup *cg = conn->contacts;
    Contact *cont;
    int i;

    if (!(cont = conn->cont))
        return;

    M_printf ("%s %s%*s%s ", s_now, COLCONTACT, uiG.nick_len + s_delta (cont->nick), cont->nick, COLNONE);
    conn->connect = 0;
    if (reconn < 5)
    {
        M_printf (i18n (2032, "Scheduling v8 reconnect in %d seconds.\n"), 10 << reconn);
        QueueEnqueueData (conn, /* FIXME: */ 0, 0, time (NULL) + (10 << reconn), NULL, cont, NULL, &SrvCallBackDoReconn);
        reconn++;
    }
    else
    {
        M_print (i18n (2031, "Connecting failed too often, giving up.\n"));
        reconn = 0;
    }
    for (i = 0; (cont = ContactIndex (cg, i)); i++)
        cont->status = STATUS_OFFLINE;
}

static void SrvCallBackDoReconn (Event *event)
{
    if (event->conn && event->conn->type == TYPE_SERVER)
        ConnectionInitServer (event->conn);
    EventD (event);
}

static void SrvCallBackTimeout (Event *event)
{
    Connection *conn = event->conn;
    
    if (!conn)
    {
        EventD (event);
        return;
    }
    ASSERT_SERVER (conn);
    
    if ((conn->connect & CONNECT_MASK) && !(conn->connect & CONNECT_OK))
    {
        if (conn->connect == event->type)
        {
            M_print (i18n (1885, "Connection v8 timed out.\n"));
            conn->connect = 0;
            sockclose (conn->sok);
            conn->sok = -1;
            SrvCallBackReconn (conn);
        }
        else
        {
            event->due = time (NULL) + 10;
            conn->connect |= CONNECT_SELECT_R;
            event->type = conn->connect;
            QueueEnqueue (event);
            return;
        }
    }
    EventD (event);
}

void SrvCallBackReceive (Connection *conn)
{
    Packet *pak;

    if (!(conn->connect & CONNECT_OK))
    {
        switch (conn->connect & 7)
        {
            case 0:
                if (conn->assoc && !(conn->assoc->connect & CONNECT_OK) && (conn->assoc->flags & CONN_AUTOLOGIN))
                {
                    M_printf ("FIXME: avoiding deadlock\n");
                    conn->connect &= ~CONNECT_SELECT_R;
                }
                else
            case 1:
            case 5:
                /* fall-through */
                    conn->connect |= 4 | CONNECT_SELECT_R;
                conn->connect &= ~CONNECT_SELECT_W & ~CONNECT_SELECT_X & ~3;
                return;
            case 2:
            case 6:
                conn->connect = 0;
                SrvCallBackReconn (conn);
                return;
            case 4:
                break;
            default:
                assert (0);
        }
    }

    pak = UtilIOReceiveTCP (conn);
    
    if (!pak)
        return;

    if (PacketRead1 (pak) != 0x2a)
    {
        Debug (DEB_PROTOCOL, i18n (1880, "Incoming packet is not a FLAP: id is %d.\n"), PacketRead1 (pak));
        return;
    }
    
    if (prG->verbose & DEB_PACK8)
    {
        M_printf ("%s " COLINDENT "%s%s ",
                 s_now, COLSERVER, i18n (1033, "Incoming v8 server packet:"));
        FlapPrint (pak);
        M_print (COLEXDENT "\r");
    }
    if (prG->verbose & DEB_PACK8SAVE)
        FlapSave (pak, TRUE);
    
    QueueEnqueueData (conn, QUEUE_FLAP, pak->id, time (NULL),
                      pak, 0, NULL, &SrvCallBackFlap);
    pak = NULL;
}

Connection *SrvRegisterUIN (Connection *conn, const char *pass)
{
    Connection *new;
#ifdef ENABLE_PEER2PEER
    Connection *newl;
#endif
    
    if (!(new = ConnectionC (TYPE_SERVER)))
        return NULL;

#ifdef ENABLE_PEER2PEER
    if (!(newl = ConnectionClone (new, TYPE_MSGLISTEN)))
    {
        ConnectionClose (new);
        return NULL;
    }
    new->assoc = newl;
    newl->open = &ConnectionInitPeer;
    if (conn && conn->assoc)
    {
        newl->version = conn->assoc->version;
        newl->status = newl->pref_status = conn->assoc->pref_status;
        newl->flags = conn->assoc->flags & ~CONN_CONFIGURED;
    }
    else
    {
        newl->version = 8;
        newl->status = newl->pref_status = prG->s5Use ? 2 : TCP_OK_FLAG;
        newl->flags |= CONN_AUTOLOGIN;
    }
#endif

    if (conn)
    {
        assert (conn->type == TYPE_SERVER);
        
        new->flags   = conn->flags & ~CONN_CONFIGURED;
        new->version = conn->version;
        new->uin     = 0;
        new->pref_status  = STATUS_ONLINE;
        new->pref_server  = strdup (conn->pref_server);
        new->pref_port    = conn->pref_port;
        new->pref_passwd  = strdup (pass);
    }
    else
    {
        new->version = 8;
        new->uin     = 0;
        new->pref_status  = STATUS_ONLINE;
        new->pref_server  = strdup ("login.icq.com");
        new->pref_port    = 5190;
        new->pref_passwd  = strdup (pass);
        new->flags |= CONN_AUTOLOGIN;
    }
    new->server = strdup (new->pref_server);
    new->port = new->pref_port;
    new->passwd = strdup (pass);
    new->open = &ConnectionInitServer;

    ConnectionInitServer (new);
    return new;
}

void SrvMsgAdvanced (Packet *pak, UDWORD seq, UWORD msgtype, UWORD status,
                     UDWORD deststatus, UWORD flags, const char *msg)
{
    if (msgtype == MSG_SSL_OPEN)       status = 0;
    else if (status == (UWORD)STATUS_OFFLINE) /* keep */ ;
    else if (status & STATUSF_DND)     status = STATUSF_DND  | (status & STATUSF_INV);
    else if (status & STATUSF_OCC)     status = STATUSF_OCC  | (status & STATUSF_INV);
    else if (status & STATUSF_NA)      status = STATUSF_NA   | (status & STATUSF_INV);
    else if (status & STATUSF_AWAY)    status = STATUSF_AWAY | (status & STATUSF_INV);
    else if (status & STATUSF_FFC)     status = STATUSF_FFC  | (status & STATUSF_INV);
    else                               status &= STATUSF_INV;
    
    if      (flags != (UWORD)-1)           /* keep */ ;
    else if (deststatus == (UWORD)STATUS_OFFLINE) /* keep */ ;
    else if (deststatus & STATUSF_DND)     flags = TCP_MSGF_CLIST;
    else if (deststatus & STATUSF_OCC)     flags = TCP_MSGF_CLIST;
    else if (deststatus & STATUSF_NA)      flags = TCP_MSGF_1;
    else if (deststatus & STATUSF_AWAY)    flags = TCP_MSGF_1;
    else if (deststatus & STATUSF_FFC)     flags = TCP_MSGF_LIST | TCP_MSGF_1;
    else                                   flags = TCP_MSGF_LIST | TCP_MSGF_1;

    PacketWriteLen    (pak);
     PacketWrite2      (pak, seq);       /* sequence number */
     PacketWrite4      (pak, 0);
     PacketWrite4      (pak, 0);
     PacketWrite4      (pak, 0);
    PacketWriteLenDone (pak);
    PacketWrite2      (pak, msgtype);    /* message type    */
    PacketWrite2      (pak, status);     /* status          */
    PacketWrite2      (pak, flags);      /* flags           */
    PacketWriteLNTS   (pak, msg);        /* the message     */
}

/*
 * Append the "geeting" part to an advanced message packet.
 */
void SrvMsgGreet (Packet *pak, UWORD cmd, const char *reason, UWORD port, UDWORD len, const char *msg)
{
    PacketWrite2     (pak, cmd);
    switch (cmd)
    {
        case 0x2d:
        default:
            PacketWriteB4  (pak, 0xbff720b2);
            PacketWriteB4  (pak, 0x378ed411);
            PacketWriteB4  (pak, 0xbd280004);
            PacketWriteB4  (pak, 0xac96d905);
            break;
        case 0x29:
        case 0x32:
            PacketWriteB4  (pak, 0xf02d12d9);
            PacketWriteB4  (pak, 0x3091d311);
            PacketWriteB4  (pak, 0x8dd70010);
            PacketWriteB4  (pak, 0x4b06462e);
    }
    PacketWrite2     (pak, 0);
    switch (cmd)
    {
        case 0x29:  PacketWriteDLStr (pak, "File");          break;
        case 0x2d:  PacketWriteDLStr (pak, "ICQ Chat");      break;
        case 0x32:  PacketWriteDLStr (pak, "File Transfer"); break;
        default:    PacketWriteDLStr (pak, "");
    }
    PacketWrite2     (pak, 0);
    PacketWrite1     (pak, 1);
    switch (cmd)
    {
        case 0x29:
        case 0x2d:  PacketWriteB4    (pak, 0x00000100);      break;
        case 0x32:  PacketWriteB4    (pak, 0x01000000);      break;
        default:    PacketWriteB4    (pak, 0);
    }
    PacketWriteB4    (pak, 0);
    PacketWriteB4    (pak, 0);
    PacketWriteLen4  (pak);
    PacketWriteDLStr (pak, c_out (reason));
    PacketWriteB2    (pak, port);
    PacketWriteB2    (pak, 0);
    PacketWriteLNTS  (pak, c_out (msg));
    PacketWrite4     (pak, len);
    if (cmd != 0x2d)
        PacketWrite4     (pak, port);
    PacketWriteLen4Done (pak);
}

/*
 * Process an advanced message.
 *
 * Note: swallows only acknowledge event/packet
 */
void SrvReceiveAdvanced (Connection *serv, Event *inc_event, Packet *inc_pak, Event *ack_event)
{
    Connection *flist;
    Contact *cont = inc_event->cont;
    ContactOptions *opt = inc_event->opt, *opt2;
    Packet *ack_pak = ack_event->pak;
    Event *e1, *e2;
    const char *txt, *ack_msg;
    strc_t text, cname, ctext, reason, cctmp;
    char *name;
    UDWORD tmp, cmd, flen, opt_acc;
    UWORD unk, seq, msgtype, unk2, pri;
    UWORD ack_flags, ack_status, accept;

    unk     = PacketRead2    (inc_pak);  PacketWrite2 (ack_pak, unk);
    seq     = PacketRead2    (inc_pak);  PacketWrite2 (ack_pak, seq);
    tmp     = PacketRead4    (inc_pak);  PacketWrite4 (ack_pak, tmp);
    tmp     = PacketRead4    (inc_pak);  PacketWrite4 (ack_pak, tmp);
    tmp     = PacketRead4    (inc_pak);  PacketWrite4 (ack_pak, tmp);
    msgtype = PacketRead2    (inc_pak);  PacketWrite2 (ack_pak, msgtype);

    unk2    = PacketRead2    (inc_pak);
    pri     = PacketRead2    (inc_pak);
    text    = PacketReadL2Str (inc_pak, NULL);
    
#ifdef WIP
    if (prG->verbose)
    M_printf ("FIXMEWIP: Starting advanced message: events %p, %p; type %d, seq %x.\n",
              inc_event, ack_event, msgtype, seq);
#endif
 
    ContactOptionsSetVal (opt, CO_MSGTYPE, msgtype);
    ContactOptionsSetStr (opt, CO_MSGTEXT, msgtype == MSG_NORM ? ConvFromCont (text, cont) : c_in_to_split (text, cont));
    
    accept = FALSE;

    if      (serv->status & STATUSF_DND)
        ack_msg = ContactPrefStr (cont, CO_AUTODND);
    else if (serv->status & STATUSF_OCC)
        ack_msg = ContactPrefStr (cont, CO_AUTOOCC);
    else if (serv->status & STATUSF_NA)
        ack_msg = ContactPrefStr (cont, CO_AUTONA);
    else if (serv->status & STATUSF_AWAY)
        ack_msg = ContactPrefStr (cont, CO_AUTOAWAY);
    else
        ack_msg = "";

/*  if (serv->status & STATUSF_DND)  ack_status  = pri & 4 ? TCP_ACK_ONLINE : TCP_ACK_DND; else
 *
 * Don't refuse until we have sensible preferences for that
 */
    if (serv->status & STATUSF_DND)  ack_status  = TCP_ACK_ONLINE; else
    if (serv->status & STATUSF_OCC)  ack_status  = TCP_ACK_ONLINE; else
    if (serv->status & STATUSF_NA)   ack_status  = TCP_ACK_NA;     else
    if (serv->status & STATUSF_AWAY) ack_status  = TCP_ACK_AWAY;
    else                             ack_status  = TCP_ACK_ONLINE;

    ack_flags = 0;
    if (serv->status & STATUSF_INV)  ack_flags |= TCP_MSGF_INV;

    switch (msgtype & ~MSGF_MASS)
    {
        /* Requests for auto-response message */
        do  {
        case MSGF_GETAUTO | MSG_GET_AWAY:  ack_msg = ContactPrefStr (cont, CO_AUTOAWAY); break;
        case MSGF_GETAUTO | MSG_GET_OCC:   ack_msg = ContactPrefStr (cont, CO_AUTOOCC);  break;
        case MSGF_GETAUTO | MSG_GET_NA:    ack_msg = ContactPrefStr (cont, CO_AUTONA);   break;
        case MSGF_GETAUTO | MSG_GET_DND:   ack_msg = ContactPrefStr (cont, CO_AUTODND);  break;
        case MSGF_GETAUTO | MSG_GET_FFC:   ack_msg = ContactPrefStr (cont, CO_AUTOFFC);  break;
        case MSGF_GETAUTO | MSG_GET_VER:   ack_msg = BuildVersionText;
            } while (0);
#ifdef WIP
            M_printf ("%s %s%*s%s ", s_now, COLCONTACT, uiG.nick_len + s_delta (cont->nick), cont->nick, COLNONE);
            M_printf (i18n (1814, "Sent auto-response message to %s%s%s.\n"),
                     COLCONTACT, cont->nick, COLNONE);
#endif
            accept = TRUE;
            ack_flags = pri;
            ack_status = 0;
            break;

        case MSG_FILE:
            cmd     = PacketRead4 (inc_pak);
            cname   = PacketReadL2Str (inc_pak, NULL);
            flen    = PacketRead4 (inc_pak);
            msgtype = PacketRead4 (inc_pak);
            
            name = strdup (ConvFromCont (cname, cont));
            
            flist = PeerFileCreate (serv);
            if (!ContactOptionsGetVal (opt, CO_FILEACCEPT, &opt_acc) && flen)
            {
                ContactOptionsSetVal (opt, CO_FILEACCEPT, 0);
                ContactOptionsSetVal (opt, CO_BYTES, flen);
                ContactOptionsSetStr (opt, CO_MSGTEXT, name);
                ContactOptionsSetVal (opt, CO_REF, ack_event->seq);
                opt2 = ContactOptionsC ();
                ContactOptionsSetVal (opt2, CO_BYTES, flen);
                ContactOptionsSetStr (opt2, CO_MSGTEXT, name);
                ContactOptionsSetVal (opt2, CO_REF, ack_event->seq);
                ContactOptionsSetVal (opt2, CO_MSGTYPE, msgtype);
                IMSrvMsg (cont, serv, NOW, opt2);
                e1 = QueueEnqueueData (serv, QUEUE_ACKNOWLEDGE, ack_event->seq, time (NULL) + 120,
                                       NULL, inc_event->cont, NULL, NULL);
                e2 = QueueEnqueueData (inc_event->conn, inc_event->type, ack_event->seq,
                                       time (NULL) + 122, inc_event->pak, inc_event->cont, opt, inc_event->callback);
                e1->rel = e2;
                e2->rel = e1;
                inc_event->pak->rpos = inc_event->pak->tpos;
                inc_event->opt = NULL;
                inc_event->pak = NULL;
                ack_event->due = 0;
                ack_event->callback = NULL;
                free (name);
#ifdef WIP
                M_printf ("FIXMEWIP: Delaying advanced message: events %p, %p.\n", inc_event, ack_event);
#endif
                return;
            }
            free (name);
            if (PeerFileIncAccept (serv->assoc, inc_event))
            {
                PacketWrite2    (ack_pak, ack_status);
                PacketWrite2    (ack_pak, ack_flags);
                PacketWriteLNTS (ack_pak, c_out (ack_msg));
                PacketWriteB2   (ack_pak, flist->port);
                PacketWrite2    (ack_pak, 0);
                PacketWriteStr  (ack_pak, "");
                PacketWrite4    (ack_pak, 0);
                if (serv->assoc->version > 6)
                    PacketWrite4 (ack_pak, 0x20726f66);
                PacketWrite4    (ack_pak, flist->port);
            }
            else
            {
                if (!ContactOptionsGetStr (opt, CO_REFUSE, &txt))
                    txt = "";
                PacketWrite2    (ack_pak, TCP_ACK_REFUSE);
                PacketWrite2    (ack_pak, ack_flags);
                PacketWriteLNTS (ack_pak, c_out (ack_msg));
                PacketWriteB2   (ack_pak, 0);
                PacketWrite2    (ack_pak, 0);
                PacketWriteStr  (ack_pak, txt);
                PacketWrite4    (ack_pak, 0);
                if (serv->assoc->version > 6)
                    PacketWrite4 (ack_pak, 0x20726f66);
                PacketWrite4    (ack_pak, 0);
            }
            accept = -1;
            break;
        case MSG_EXTENDED:
            {
                /* UWORD port, port2, pad; */
                char *gtext;
                char id[20];
                str_s t = { 0, 0, 0 };
                t.txt = id;
                
                cmd    = PacketRead2 (inc_pak);
                         PacketReadData (inc_pak, &t, 16);
                         PacketRead2 (inc_pak);
                ctext  = PacketReadL4Str (inc_pak, NULL);
                         PacketReadData (inc_pak, NULL, 15);
                         PacketRead4 (inc_pak);
                reason = PacketReadL4Str (inc_pak, NULL);
                /*port=*/PacketReadB2 (inc_pak);
                /*pad=*/ PacketRead2 (inc_pak);
                cname  = PacketReadL2Str (inc_pak, NULL);
                flen   = PacketRead4 (inc_pak);
                /*port2*/PacketRead4 (inc_pak);
                
                gtext = strdup (ConvFromCont (ctext, cont));
                name = strdup (ConvFromCont (cname, cont));
                
                switch (cmd)
                {
                    case 0x0029:
                        flist = PeerFileCreate (serv);
                        if (!ContactOptionsGetVal (opt, CO_FILEACCEPT, &opt_acc) && flen)
                        {
                            ContactOptionsSetVal (opt, CO_FILEACCEPT, 0);
                            ContactOptionsSetVal (opt, CO_BYTES, flen);
                            ContactOptionsSetStr (opt, CO_MSGTEXT, name);
                            ContactOptionsSetVal (opt, CO_REF, ack_event->seq);
                            opt2 = ContactOptionsC ();
                            ContactOptionsSetVal (opt2, CO_BYTES, flen);
                            ContactOptionsSetStr (opt2, CO_MSGTEXT, name);
                            ContactOptionsSetVal (opt2, CO_REF, ack_event->seq);
                            ContactOptionsSetVal (opt2, CO_MSGTYPE, msgtype);
                            IMSrvMsg (cont, serv, NOW, opt2);
                            e1 = QueueEnqueueData (serv, QUEUE_ACKNOWLEDGE, ack_event->seq, time (NULL) + 120,
                                                   NULL, inc_event->cont, NULL, NULL);
                            e2 = QueueEnqueueData (inc_event->conn, inc_event->type, ack_event->seq,
                                                   time (NULL) + 122, inc_event->pak, inc_event->cont, opt, inc_event->callback);
                            e1->rel = e2;
                            e2->rel = e1;
                            inc_event->pak->rpos = inc_event->pak->tpos;
                            inc_event->opt = NULL;
                            inc_event->pak = NULL;
                            ack_event->callback = NULL;
                            ack_event->due = 0;
                            free (name);
                            free (gtext);
#ifdef WIP
                            M_printf ("FIXMEWIP: Delaying advanced message: events %p, %p.\n", inc_event, ack_event);
#endif
                            return;
                        }
                        if (PeerFileIncAccept (serv->assoc, inc_event))
                        {
                            PacketWrite2    (ack_pak, ack_status);
                            PacketWrite2    (ack_pak, ack_flags);
                            PacketWriteLNTS (ack_pak, "");
                            SrvMsgGreet     (ack_pak, cmd, "", flist->port, 0, "");
                        }
                        else
                        {
                            if (!ContactOptionsGetStr (opt, CO_REFUSE, &txt))
                                txt = "";
                            PacketWrite2    (ack_pak, TCP_ACK_REFUSE);
                            PacketWrite2    (ack_pak, ack_flags);
                            PacketWriteLNTS (ack_pak, txt);
                            SrvMsgGreet     (ack_pak, cmd, "", 0, 0, "");
                        }
                        break;

                    case 0x002d:
                        if (id[0] == (char)0xbf)
                        {
                            opt2 = ContactOptionsC ();
                            ContactOptionsSetVal (opt2, CO_MSGTYPE, msgtype);
                            ContactOptionsSetStr (opt2, CO_MSGTEXT, c_in_to_split (text, cont));
                            IMSrvMsg (cont, serv, NOW, opt2);
                            opt2 = ContactOptionsC ();
                            ContactOptionsSetVal (opt2, CO_MSGTYPE, MSG_CHAT);
                            ContactOptionsSetStr (opt2, CO_MSGTEXT, name);
                            IMSrvMsg (cont, serv, NOW, opt2);
                            opt2 = ContactOptionsC ();
                            ContactOptionsSetVal (opt2, CO_MSGTYPE, MSG_CHAT);
                            ContactOptionsSetStr (opt2, CO_MSGTEXT, reason->txt);
                            PacketWrite2    (ack_pak, TCP_ACK_REFUSE);
                            PacketWrite2    (ack_pak, ack_flags);
                            PacketWriteLNTS (ack_pak, "");
                            SrvMsgGreet     (ack_pak, cmd, "", 0, 0, "");
                            break;
                        }
                        else if (id[0] == (char)0x2a)
                        {
                            opt2 = ContactOptionsC ();
                            ContactOptionsSetVal (opt2, CO_MSGTYPE, MSG_CONTACT);
                            ContactOptionsSetStr (opt2, CO_MSGTEXT, c_in_to_split (reason, cont));
                            IMSrvMsg (cont, serv, NOW, opt2);
                            PacketWrite2    (ack_pak, ack_status);
                            PacketWrite2    (ack_pak, ack_flags);
                            PacketWriteLNTS (ack_pak, "");
                            SrvMsgGreet     (ack_pak, cmd, "", 0, 0, "");
                            break;
                        }

                    case 0x0032:
                    default:
                        if (prG->verbose & DEB_PROTOCOL)
                            M_printf (i18n (2065, "Unknown TCP_MSG_GREET_ command %04x.\n"), msgtype);
                        PacketWrite2    (ack_pak, TCP_ACK_REFUSE);
                        PacketWrite2    (ack_pak, ack_flags);
                        PacketWriteLNTS (ack_pak, "");
                        SrvMsgGreet     (ack_pak, cmd, "", 0, 0, "");
                        break;
                    
                }
                free (name);
                free (gtext);
                accept = -1;
            }
            break;
#ifdef ENABLE_SSL            
        case MSG_SSL_OPEN:  /* Licq compatible SSL handshake */
            if (!unk2)
            {
                PacketWrite2     (ack_pak, ack_status);
                PacketWrite2     (ack_pak, ack_flags);
                PacketWriteLNTS  (ack_pak, c_out ("1"));
                accept = -1;
                ack_event->conn->ssl_status = SSL_STATUS_INIT;
            }
            break;
        case MSG_SSL_CLOSE:
            if (!unk2)
            {
                PacketWrite2     (ack_pak, ack_status);
                PacketWrite2     (ack_pak, ack_flags);
                PacketWriteLNTS  (ack_pak, c_out (""));
                accept = -1;
                ack_event->conn->ssl_status = SSL_STATUS_CLOSE;
            }
            break;
#endif
        default:
            if (prG->verbose & DEB_PROTOCOL)
                M_printf (i18n (2066, "Unknown TCP_MSG_ command %04x.\n"), msgtype);
            /* fall-through */
        case MSG_CHAT:
            /* chat ist not implemented, so reject chat requests */
            accept = FALSE;
            break;

        /* Regular messages */
        case MSG_AUTO:
        case MSG_NORM:
        case MSG_URL:
        case MSG_AUTH_REQ:
        case MSG_AUTH_DENY:
        case MSG_AUTH_GRANT:
        case MSG_AUTH_ADDED:
        case MSG_WEB:
        case MSG_EMAIL:
        case MSG_CONTACT:
            /**/    PacketRead4 (inc_pak);
            /**/    PacketRead4 (inc_pak);
            cctmp = PacketReadL4Str (inc_pak, NULL);
            if (!strcmp (cctmp->txt, CAP_GID_UTF8))
                ContactOptionsSetStr (opt, CO_MSGTEXT, text->txt);
            if (*text->txt)
                IMSrvMsg (cont, serv, NOW, opt);
            inc_event->opt = NULL;
            PacketWrite2     (ack_pak, ack_status);
            PacketWrite2     (ack_pak, ack_flags);
            PacketWriteLNTS  (ack_pak, c_out (ack_msg));
            if (msgtype == MSG_NORM)
            {
                PacketWrite4 (ack_pak, TCP_COL_FG);
                PacketWrite4 (ack_pak, TCP_COL_BG);
            }
            accept = -1;
            break;
    }
    switch (accept)
    {
        case FALSE:
            PacketWrite2      (ack_pak, TCP_ACK_REFUSE);
            PacketWrite2      (ack_pak, ack_flags);
            PacketWriteLNTS   (ack_pak, c_out_to (ack_msg, cont));
            break;

        case TRUE:
            PacketWrite2      (ack_pak, ack_status);
            PacketWrite2      (ack_pak, ack_flags);
            PacketWriteLNTS   (ack_pak, c_out_to (ack_msg, cont));
    }
#ifdef WIP
    if (prG->verbose)
    M_printf ("FIXMEWIP: Finishing advanced message: events %p, %p.\n", inc_event, ack_event);
#endif
    QueueDequeueEvent (ack_event);
    ack_event->callback (ack_event);
}

