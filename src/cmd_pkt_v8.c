
/*
 * Initialization and basic support for v8 of the ICQ protocol.
 *
 * This file is Copyright © Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 */

#include "micq.h"
#include "util.h"
#include "util_ui.h"
#include "util_io.h"
#include "util_extra.h"
#include "buildmark.h"
#include "icq_response.h"
#include "conv.h"
#include "contact.h"
#include "session.h"
#include "packet.h"
#include "preferences.h"
#include "cmd_pkt_v8_flap.h"
#include "cmd_pkt_v8_snac.h"
#include "util_str.h"
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

    M_printf (i18n (1871, "Opening v8 connection to %s:%ld... "), conn->server, conn->port);

    conn->our_seq  = rand () & 0x7fff;
    conn->connect  = 0;
    conn->dispatch = &SrvCallBackReceive;
    conn->reconnect= &SrvCallBackReconn;
    conn->close    = &FlapCliGoodbye;
    s_repl (&conn->server, conn->spref->server);
    conn->type     = TYPE_SERVER;
    if (conn->status == STATUS_OFFLINE)
        conn->status = conn->spref->status;
    QueueEnqueueData (conn, conn->connect, conn->our_seq,
                      time (NULL) + 10,
                      NULL, conn->uin, NULL, &SrvCallBackTimeout);
    UtilIOConnectTCP (conn);
}

static void SrvCallBackReconn (Connection *conn)
{
    ContactGroup *cg = conn->contacts;
    Contact *cont;
    int i;

    cont = ContactUIN (conn, conn->uin);
    if (!cont)
        return;

    M_printf ("%s %s%*s%s ", s_now, COLCONTACT, uiG.nick_len + s_delta (cont->nick), cont->nick, COLNONE);
    conn->connect = 0;
    if (reconn < 5)
    {
        M_printf (i18n (2032, "Scheduling v8 reconnect in %d seconds.\n"), 10 << reconn);
        QueueEnqueueData (conn, /* FIXME: */ 0, 0, time (NULL) + (10 << reconn), NULL, conn->uin, NULL, &SrvCallBackDoReconn);
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
    if (event->conn)
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
    
    pak->cmd = PacketRead1 (pak);
    pak->id =  PacketReadB2 (pak);
               PacketReadB2 (pak);
    
    if (prG->verbose & DEB_PACK8)
    {
        M_printf ("%s " COLINDENT COLSERVER "%s ",
                 s_now, i18n (1033, "Incoming v8 server packet:"));
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
    
    new = ConnectionC ();
    if (!new)
        return NULL;
    new->spref = PreferencesConnectionC ();
    if (!new->spref)
        return NULL;
    if (conn)
    {
        assert (conn->spref->type == TYPE_SERVER);
        
        memcpy (new->spref, conn->spref, sizeof (*new->spref));
        new->spref->server = strdup (new->spref->server);
        new->spref->uin = 0;
    }
    else
    {
        new->spref->type = TYPE_SERVER;
        new->spref->flags = 0;
        new->spref->version = 8;
        new->spref->server = strdup ("login.icq.com");
        new->spref->port = 5190;
    }
    new->spref->passwd = strdup (pass);
    new->type    = TYPE_SERVER;
    new->flags   = 0;
    new->ver  = new->spref->version;
    new->server = strdup (new->spref->server);
    new->port = new->spref->port;
    new->passwd = strdup (pass);
    
    ConnectionInitServer (new);
    return new;
}

void SrvMsgAdvanced (Packet *pak, UDWORD seq, UWORD type, UWORD flags, UWORD status, const char *msg)
{
    PacketWriteLen    (pak);
     PacketWrite2      (pak, seq);        /* sequence number            */
     PacketWrite4      (pak, 0);
     PacketWrite4      (pak, 0);
     PacketWrite4      (pak, 0);
    PacketWriteLenDone (pak);
    PacketWrite2      (pak, type);       /* message type               */
    PacketWrite2      (pak, status);     /* flags                      */
    PacketWrite2      (pak, flags);      /* status                     */
    PacketWriteLNTS   (pak, msg);        /* the message                */
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
    Contact *cont = ContactUIN (serv, inc_event->uin);
    Extra *extra = inc_event->extra;
    Packet *ack_pak = ack_event->pak;
    Event *e1, *e2;
    const char *txt, *ack_msg;
    char *text, *ctext, *cname, *name, *cctmp;
    UDWORD tmp, cmd, flen;
    UWORD unk, seq, msgtype, /*unk2,*/ pri;
    UWORD ack_flags, ack_status, accept;

    unk     = PacketRead2    (inc_pak);  PacketWrite2 (ack_pak, unk);
    seq     = PacketRead2    (inc_pak);  PacketWrite2 (ack_pak, seq);
    tmp     = PacketRead4    (inc_pak);  PacketWrite4 (ack_pak, tmp);
    tmp     = PacketRead4    (inc_pak);  PacketWrite4 (ack_pak, tmp);
    tmp     = PacketRead4    (inc_pak);  PacketWrite4 (ack_pak, tmp);
    msgtype = PacketRead2    (inc_pak);  PacketWrite2 (ack_pak, msgtype);

    /*unk2=*/ PacketRead2    (inc_pak);
    pri     = PacketRead2    (inc_pak);
    text    = PacketReadLNTS (inc_pak);
    
#ifdef WIP
    if (prG->verbose)
    M_printf ("FIXME: Starting advanced message: events %p, %p; type %d, seq %x.\n",
              inc_event, ack_event, msgtype, seq);
#endif
 
    ExtraSet (extra, EXTRA_MESSAGE, msgtype, c_in_to (text, cont));

    accept = FALSE;

    if      (serv->status & STATUSF_DND)
        ack_msg = prG->auto_dnd;
    else if (serv->status & STATUSF_OCC)
        ack_msg = prG->auto_occ;
    else if (serv->status & STATUSF_NA)
        ack_msg = prG->auto_na;
    else if (serv->status & STATUSF_AWAY)
        ack_msg = prG->auto_away;
    else
        ack_msg = "";

    if (pri & 4)                     ack_status  = TCP_STAT_ONLINE; else
    if (serv->status & STATUSF_DND)  ack_status  = TCP_STAT_DND;    else
    if (serv->status & STATUSF_OCC)  ack_status  = TCP_STAT_OCC;    else
    if (serv->status & STATUSF_NA)   ack_status  = TCP_STAT_NA;     else
    if (serv->status & STATUSF_AWAY) ack_status  = TCP_STAT_AWAY;
    else                             ack_status  = TCP_STAT_ONLINE;

    ack_flags = 0;
    if (serv->status & STATUSF_INV)  ack_flags |= TCP_MSGF_INV;
/*    ack_flags ^= TCP_MSGF_LIST;   */

    switch (msgtype & ~MSGF_MASS)
    {
        /* Requests for auto-response message */
        do  {
        case MSGF_GETAUTO | MSG_GET_AWAY:  ack_msg = prG->auto_away; break;
        case MSGF_GETAUTO | MSG_GET_OCC:   ack_msg = prG->auto_occ;  break;
        case MSGF_GETAUTO | MSG_GET_NA:    ack_msg = prG->auto_na;   break;
        case MSGF_GETAUTO | MSG_GET_DND:   ack_msg = prG->auto_dnd;  break;
        case MSGF_GETAUTO | MSG_GET_FFC:   ack_msg = prG->auto_ffc;  break;
        case MSGF_GETAUTO | MSG_GET_VER:   ack_msg = BuildVersionText;
            } while (0);
#ifdef WIP
            M_printf ("%s " COLCONTACT "%*s" COLNONE " ", s_now, uiG.nick_len + s_delta (cont->nick), cont->nick);
            M_printf (i18n (1814, "Sent auto-response message to %s%s%s.\n"),
                     COLCONTACT, cont->nick, COLNONE);
#endif
            accept = TRUE;
            ack_flags = pri;
            ack_status = 0;
            break;

        case TCP_MSG_FILE:
            cmd     = PacketRead4 (inc_pak);
            cname   = PacketReadLNTS (inc_pak);
            flen    = PacketRead4 (inc_pak);
            msgtype = PacketRead4 (inc_pak);
            
            name = strdup (c_in_to (cname, cont));
            free (cname);
            
            flist = PeerFileCreate (serv);
            if (!ExtraGet (extra, EXTRA_FILETRANS) && flen)
            {
                ExtraSet (extra, EXTRA_FILETRANS, flen, name);
                ExtraSet (extra, EXTRA_REF, ack_event->seq, NULL);
                IMSrvMsg (cont, serv, NOW, ExtraClone (extra));
                e1 = QueueEnqueueData (serv, QUEUE_ACKNOWLEDGE, ack_event->seq, time (NULL) + 60,
                                       NULL, inc_event->uin, NULL, NULL);
                e2 = QueueEnqueueData (inc_event->conn, inc_event->type, ack_event->seq,
                                       time (NULL) + 62, inc_pak, inc_event->uin, extra, inc_event->callback);
                e1->rel = e2;
                e2->rel = e1;
                inc_pak->rpos = inc_pak->tpos;
                inc_event->extra = NULL;
                inc_event->pak = NULL;
                ack_event->due = 0;
                ack_event->callback = NULL;
                free (text);
                free (name);
#ifdef WIP
                M_printf ("FIXME: Delaying advanced message: events %p, %p.\n", inc_event, ack_event);
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
                if (serv->assoc->ver > 6)
                    PacketWrite4 (ack_pak, 0x20726f66);
                PacketWrite4    (ack_pak, flist->port);
            }
            else
            {
                txt = ExtraGetS (extra, EXTRA_REF);
                PacketWrite2    (ack_pak, TCP_STAT_REFUSE);
                PacketWrite2    (ack_pak, ack_flags);
                PacketWriteLNTS (ack_pak, c_out (ack_msg));
                PacketWriteB2   (ack_pak, 0);
                PacketWrite2    (ack_pak, 0);
                PacketWriteStr  (ack_pak, txt ? txt : "");
                PacketWrite4    (ack_pak, 0);
                if (serv->assoc->ver > 6)
                    PacketWrite4 (ack_pak, 0x20726f66);
                PacketWrite4    (ack_pak, 0);
            }
            accept = -1;
            break;
        case TCP_MSG_GREETING:
            {
                /* UWORD port, port2, pad; */
                char *gtext, *reason;
                
                cmd    = PacketRead2 (inc_pak);
                         PacketReadData (inc_pak, NULL, 16);
                         PacketRead2 (inc_pak);
                ctext  = PacketReadDLStr (inc_pak);
                         PacketReadData (inc_pak, NULL, 15);
                         PacketRead4 (inc_pak);
                reason = PacketReadDLStr (inc_pak);
                /*port=*/PacketReadB2 (inc_pak);
                /*pad=*/ PacketRead2 (inc_pak);
                cname  = PacketReadLNTS (inc_pak);
                flen   = PacketRead4 (inc_pak);
                /*port2*/PacketRead4 (inc_pak);
                
                gtext = strdup (c_in_to (ctext, cont));
                free (ctext);
                name = strdup (c_in_to (cname, cont));
                free (cname);
                
                switch (cmd)
                {
                    case 0x0029:
                        flist = PeerFileCreate (serv);
                        if (!ExtraGet (extra, EXTRA_FILETRANS) && flen)
                        {
                            ExtraSet (extra, EXTRA_FILETRANS, flen, name);
                            ExtraSet (extra, EXTRA_REF, ack_event->seq, NULL);
                            ExtraSet (extra, EXTRA_MESSAGE, TCP_MSG_FILE, name);
                            IMSrvMsg (cont, serv, NOW, ExtraClone (extra));
                            e1 = QueueEnqueueData (serv, QUEUE_ACKNOWLEDGE, ack_event->seq, time (NULL) + 60,
                                                   NULL, inc_event->uin, NULL, NULL);
                            e2 = QueueEnqueueData (inc_event->conn, inc_event->type, ack_event->seq,
                                                   time (NULL) + 62, inc_pak, inc_event->uin, extra, inc_event->callback);
                            e1->rel = e2;
                            e2->rel = e1;
                            inc_pak->rpos = inc_pak->tpos;
                            inc_event->extra = NULL;
                            inc_event->pak = NULL;
                            ack_event->callback = NULL;
                            ack_event->due = 0;
                            free (text);
                            free (name);
                            free (gtext);
                            free (reason);
#ifdef WIP
                            M_printf ("FIXME: Delaying advanced message: events %p, %p.\n", inc_event, ack_event);
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
                            PacketWrite2    (ack_pak, TCP_STAT_REFUSE);
                            PacketWrite2    (ack_pak, ack_flags);
                            PacketWriteLNTS (ack_pak, "");
                            SrvMsgGreet     (ack_pak, cmd, "", 0, 0, "");
                        }
                        break;

                    case 0x002d:
                        IMSrvMsg (cont, serv->assoc, NOW, ExtraClone (extra));
                        IMSrvMsg (cont, serv->assoc, NOW, ExtraSet (ExtraClone (extra),
                                  EXTRA_MESSAGE, TCP_MSG_CHAT, name));
                        IMSrvMsg (cont, serv->assoc, NOW, ExtraSet (ExtraClone (extra),
                                  EXTRA_MESSAGE, TCP_MSG_CHAT, reason));
                        PacketWrite2    (ack_pak, TCP_STAT_REFUSE);
                        PacketWrite2    (ack_pak, ack_flags);
                        PacketWriteLNTS (ack_pak, "");
                        SrvMsgGreet     (ack_pak, cmd, "", 0, 0, "");
                        break;

                    case 0x0032:
                    default:
                        if (prG->verbose & DEB_PROTOCOL)
                            M_printf (i18n (2065, "Unknown TCP_MSG_GREET_ command %04x.\n"), msgtype);
                        PacketWrite2    (ack_pak, TCP_STAT_REFUSE);
                        PacketWrite2    (ack_pak, ack_flags);
                        PacketWriteLNTS (ack_pak, "");
                        SrvMsgGreet     (ack_pak, cmd, "", 0, 0, "");
                        break;
                    
                }
                free (name);
                free (gtext);
                free (reason);
                accept = -1;
            }
            break;
        default:
            if (prG->verbose & DEB_PROTOCOL)
                M_printf (i18n (2066, "Unknown TCP_MSG_ command %04x.\n"), msgtype);
            /* fall-through */
        case TCP_MSG_CHAT:
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
#ifdef ENABLE_UTF8
            /**/    PacketRead4 (inc_pak);
            /**/    PacketRead4 (inc_pak);
            cctmp = PacketReadDLStr (inc_pak);
            if (!strcmp (cctmp, CAP_GID_UTF8))
                ExtraSet (extra, EXTRA_MESSAGE, msgtype, text);
            free (cctmp);
#endif
            if (*text)
            IMSrvMsg (cont, serv, NOW, ExtraClone (extra));
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
            PacketWrite2      (ack_pak, TCP_STAT_REFUSE);
            PacketWrite2      (ack_pak, ack_flags);
            PacketWriteLNTS   (ack_pak, c_out_to (ack_msg, cont));
            break;

        case TRUE:
            PacketWrite2      (ack_pak, ack_status);
            PacketWrite2      (ack_pak, ack_flags);
            PacketWriteLNTS   (ack_pak, c_out_to (ack_msg, cont));
    }
    free (text);
#ifdef WIP
    if (prG->verbose)
    M_printf ("FIXME: Finishing advanced message: events %p, %p.\n", inc_event, ack_event);
#endif
    QueueDequeueEvent (ack_event);
    ack_event->callback (ack_event);
}

