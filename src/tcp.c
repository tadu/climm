/*
 * This file handles TCP client-to-client communications.
 *
 *  Author/Copyright: James Schofield (jschofield@ottawa.com) 22 Feb 2001
 *  Lots of changes from Rüdiger Kuhlmann. File transfer by Rüdiger Kuhlmann.
 *  This file may be distributed under version 2 of the GPL licence.
 *
 * $Id$
 */

#include "micq.h"
#include "util_ui.h"
#include "util_io.h"
#include "file_util.h"
#include "util.h"
#include "buildmark.h"
#include "network.h"
#include "conv.h"
#include "cmd_user.h"
#include "icq_response.h"
#include "preferences.h"
#include "server.h"
#include "contact.h"
#include "session.h"
#include "packet.h"
#include "tcp.h"
#include "util_syntax.h"
#include "util_ssl.h"
#include "cmd_pkt_cmd_v5.h"
#include "cmd_pkt_v8_snac.h"
#include "cmd_pkt_v8.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_WINSOCK2_H
#include <winsock2.h>
#endif

#ifdef ENABLE_SSL
#define CV_ORIGIN_dcssl (peer->ssl_status == SSL_STATUS_OK ? CV_ORIGIN_ssl : CV_ORIGIN_dc)
#else
#define CV_ORIGIN_dcssl CV_ORIGIN_dc
#endif

#ifdef ENABLE_PEER2PEER

static void        TCPDispatchPeer    (Connection *peer);
static void        PeerDispatchClose  (Connection *peer);

static Packet     *TCPReceivePacket   (Connection *peer);

static void        TCPSendInit        (Connection *peer);
static void        TCPSendInitAck     (Connection *peer);
static void        TCPSendInit2       (Connection *peer);
static Connection *TCPReceiveInit     (Connection *peer, Packet *pak);
static void        TCPReceiveInitAck  (Connection *peer, Packet *pak);
static Connection *TCPReceiveInit2    (Connection *peer, Packet *pak);


static Packet     *PacketTCPC         (Connection *peer, UDWORD cmd);

static void        TCPCallBackTimeout (Event *event);
static void        TCPCallBackTOConn  (Event *event);
static void        TCPCallBackResend  (Event *event);
static void        TCPCallBackReceive (Event *event);

static void        Encrypt_Pak        (Connection *peer, Packet *pak);
static BOOL        Decrypt_Pak        (Connection *peer, Packet *pak);

static void TCPSendInitv6 (Connection *peer);


/*********************************************/

/*
 * "Logs in" peer2peer connection by opening listening socket.
 */
Event *ConnectionInitPeer (Connection *list)
{
    ASSERT_MSGLISTEN (list);
    
    if (list->version < 6 || list->version > 8)
    {
        M_printf (i18n (2024, "Unknown protocol version %d for ICQ peer-to-peer protocol.\n"), list->version);
        return NULL;
    }

    if (list->version == 6)
        M_print (i18n (2046, "You may want to use protocol version 8 for the ICQ peer-to-peer protocol instead.\n"));

    M_printf (i18n (9999, "Opening peer-to-peer connection at %slocalhost%s:%s%ld%s... "),
              COLQUOTE, COLNONE, COLQUOTE, list->port, COLNONE);

    list->connect     = 0;
    list->our_seq     = -1;
    list->dispatch    = &TCPDispatchMain;
    list->our_session = 0;
    list->ip          = 0;
    s_repl (&list->server, NULL);
    list->port        = list->pref_port;
    list->cont        = list->parent->cont ? list->parent->cont : ContactUIN (list->parent, list->parent->uin);

    UtilIOConnectTCP (list);
    return NULL;
}

/*
 *  Starts establishing a TCP connection to given contact.
 */
BOOL TCPDirectOpen (Connection *list, Contact *cont)
{
    Connection *peer;

    ASSERT_MSGLISTEN (list);
    assert (cont);

    if (!cont || !cont->dc || cont->dc->version < 6)
        return FALSE;
    if (cont == list->cont)
        return FALSE;

    if (!(peer = ConnectionFind (TYPE_MSGDIRECT, cont, list)))
        if (!(peer = ConnectionClone (list, TYPE_MSGDIRECT)))
            return FALSE;

    ASSERT_MSGDIRECT (peer);

    if (peer->connect & CONNECT_MASK)
        return TRUE;
    
    peer->version   = list->version <= cont->dc->version ? list->version : cont->dc->version;
    peer->port      = 0;
    peer->cont      = cont;
    peer->assoc     = NULL;
    peer->close     = &PeerDispatchClose;
    peer->reconnect = &TCPDispatchReconn;

    TCPDispatchConn (peer);

    return TRUE;
}

/*
 * Closes TCP message/file connection(s) to given contact.
 */
void TCPDirectClose (Connection *list, Contact *cont)
{
    Connection *peer;

    ASSERT_MSGLISTEN (list);
    assert (cont);
    
    while ((peer = ConnectionFind (TYPEF_ANY_DIRECT, list->cont, list)))
        TCPClose (peer);
}

/*
 * Switchs off TCP for a given uin.
 */
void TCPDirectOff (Connection *list, Contact *cont)
{
    Connection *peer;
    
    ASSERT_MSGLISTEN (list);
    assert (cont);

    if (!(peer = ConnectionFind (TYPE_MSGDIRECT, cont, list)))
        if (!(peer = ConnectionClone (list, TYPE_MSGDIRECT)))
            return;
    
    peer->cont     = cont;
    peer->connect  = CONNECT_FAIL;

    ASSERT_MSGDIRECT(peer);

    if (peer->incoming)
    {
        PacketD (peer->incoming);
        peer->incoming = NULL;
    }
}

/**************************************************/

/*
 * Reconnect hook. Actually, just inform the user.
 */
void TCPDispatchReconn (Connection *peer)
{
    ASSERT_ANY_DIRECT(peer);

    if (prG->verbose)
    {
        Contact *cont = peer->cont;

        M_printf ("%s %s%*s%s ", s_now, COLCONTACT, uiG.nick_len + s_delta (cont->nick), cont->nick, COLNONE);
        M_print  (i18n (2023, "Direct connection closed by peer.\n"));
    }
    if (peer->close)
        peer->close (peer);
    else
        TCPClose (peer);
}

/*
 * Accepts a new direct connection.
 */
void TCPDispatchMain (Connection *list)
{
    struct sockaddr_in sin;
    Connection *peer;
    socklen_t length;
    int rc;
 
    ASSERT_ANY_LISTEN(list);

    if (list->connect & CONNECT_OK)
    {
        if ((rc = UtilIOError (list)))
        {
#ifndef __BEOS__
            M_printf (i18n (2051, "Error on socket: %s (%d)\n"), strerror (rc), rc);
            if (list->sok > 0)
                sockclose (list->sok);
            list->sok = -1;
            list->connect = 0;
            return;
#endif
        }
    }
    else
    {
        switch (list->connect & 3)
        {
            case 1:
                list->connect |= CONNECT_OK | CONNECT_SELECT_R | CONNECT_SELECT_X;
                list->connect &= ~CONNECT_SELECT_W; /* & ~CONNECT_SELECT_X; */
                if (list->type == TYPE_MSGLISTEN && list->parent && list->parent->type == TYPE_SERVER
                    && (list->parent->connect & CONNECT_OK))
                    SnacCliSetstatus (list->parent, 0, 2);
                break;
            case 2:
                list->connect = 0;
                break;
            default:
                assert (0);
        }
        return;
    }

    peer = ConnectionClone (list, list->type == TYPE_MSGLISTEN ? TYPE_MSGDIRECT : TYPE_FILEDIRECT);
    
    if (!peer)
    {
        M_print (i18n (1914, "Can't allocate connection structure.\n"));
        list->connect = 0;
        if (list->sok)
            sockclose (list->sok);
        return;
    }

    if (list->version == 6)
        M_print (i18n (2046, "You may want to use protocol version 8 for the ICQ peer-to-peer protocol instead.\n"));

    peer->our_session = 0;
    peer->dispatch    = &TCPDispatchShake;

    if (prG->s5Use)
    {
        peer->sok = list->sok;
        list->sok = -1;
        list->connect = 0;
        UtilIOSocksAccept (peer);
        UtilIOConnectTCP (list);
    }
    else
    {
        length = sizeof (sin);
        peer->sok  = accept (list->sok, (struct sockaddr *)&sin, &length);
        
        if (peer->sok <= 0)
        {
            ConnectionClose (peer);
            return;
        }
    }

    peer->connect = 16 | CONNECT_SELECT_R;
    peer->cont = NULL;
}

/*
 * Continues connecting.
 */
void TCPDispatchConn (Connection *peer)
{
    Contact *cont;
    
    ASSERT_ANY_DIRECT (peer);
    
    peer->dispatch = &TCPDispatchConn;

    while (1)
    {
        if (!(cont = peer->cont) || !cont->dc)
        {
            TCPClose (peer);
            return;
        }
        
        Debug (DEB_TCP, "Conn: uin %ld nick %s state %x", cont->uin, cont->nick, peer->connect);

        switch (peer->connect & CONNECT_MASK)
        {
            case 0:
                if (!cont->dc->ip_rem || !cont->dc->port)
                {
                    peer->connect = 3;
                    break;
                }
                if (peer->type == TYPE_MSGDIRECT)
                    peer->port = cont->dc->port;
                s_repl (&peer->server, NULL);
                peer->ip      = cont->dc->ip_rem;
                peer->connect = 1;
                
                if (prG->verbose)
                {
                    M_printf ("%s %s%*s%s ", s_now, COLCONTACT, uiG.nick_len + s_delta (cont->nick), cont->nick, COLNONE);
                    M_printf (i18n (9999, "Opening TCP connection at %s:%s%ld%s... "),
                              s_wordquote (s_ip (peer->ip)), COLQUOTE, peer->port, COLNONE);
                }
                UtilIOConnectTCP (peer);
                return;
            case 3:
                if (!cont->dc->ip_loc || !cont->dc->port)
                {
                    peer->connect = CONNECT_FAIL;
                    return;
                }
                if (peer->type == TYPE_MSGDIRECT)
                    peer->port = cont->dc->port;
                s_repl (&peer->server, NULL);
                peer->ip      = cont->dc->ip_loc;
                peer->connect = 3;
                
                if (prG->verbose)
                {
                    M_printf ("%s %s%*s%s ", s_now, COLCONTACT, uiG.nick_len + s_delta (cont->nick), cont->nick, COLNONE);
                    M_printf (i18n (9999, "Opening TCP connection at %s:%s%ld%s... "),
                              s_wordquote (s_ip (peer->ip)), COLQUOTE, peer->port, COLNONE);
                }
                UtilIOConnectTCP (peer);
                return;
            case 5:
            {
                if (peer->parent && peer->parent->parent && peer->parent->parent->type == TYPE_SERVER_OLD)
                {
                    CmdPktCmdTCPRequest (peer->parent->parent, cont, cont->dc->port);
                    QueueEnqueueData (peer, QUEUE_TCP_TIMEOUT, peer->ip, time (NULL) + 30,
                                      NULL, cont, NULL, &TCPCallBackTOConn);
                    peer->connect = TCP_STATE_WAITING;
                }
                else
                    peer->connect = CONNECT_FAIL;
                sockclose (peer->sok);
                peer->sok = -1;
                return;
            }
            case 2:
            case 4:
                if (prG->verbose)
                {
                    M_printf ("%s %s%*s%s ", s_now, COLCONTACT, uiG.nick_len + s_delta (cont->nick), cont->nick, COLNONE);
                    M_printf (i18n (9999, "Opening TCP connection at %s:%s%ld%s... "),
                              s_wordquote (s_ip (peer->ip)), COLQUOTE, peer->port, COLNONE);
                    M_print (i18n (1785, "success.\n"));
                }
                QueueEnqueueData (peer, QUEUE_TCP_TIMEOUT, peer->ip, time (NULL) + 10,
                                  NULL, cont, NULL, &TCPCallBackTimeout);
                peer->connect = 1 | CONNECT_SELECT_R;
                peer->dispatch = &TCPDispatchShake;
#ifdef ENABLE_SSL
                /* We only set that status from here since we don't initialize
                 * SSL for incoming connections.
                 */
                peer->ssl_status = SSL_STATUS_REQUEST;
#endif
                TCPDispatchShake (peer);
                return;
            case TCP_STATE_WAITING:
            case TCP_STATE_WAITING + 2:
                if (prG->verbose)
                    M_printf (i18n (1855, "TCP connection to %s at %s:%ld failed.\n"),
                             cont->nick, s_ip (peer->ip), peer->port);
                peer->connect = CONNECT_FAIL;
                peer->sok = -1;
                return;
            case CONNECT_OK:
                return;
            default:
                assert (0);
        }
    }
}

/*
 * Shake hands with peer.
 */
void TCPDispatchShake (Connection *peer)
{
    Contact *cont;
    Packet *pak = NULL;
    
    ASSERT_ANY_DIRECT (peer);
    
    if ((peer->connect & CONNECT_MASK) != 1)
        if (!(pak = TCPReceivePacket (peer)))
            return;

    while (1)
    {
        if (!peer)
            return;

        if (!(cont = peer->cont) && (peer->connect & CONNECT_MASK) != 16)
        {
            TCPClose (peer);
            peer->connect = CONNECT_FAIL;
            if (pak)
                PacketD (pak);
            return;
        }
        
        Debug (DEB_TCP, "HS %d uin %ld nick %s state %d pak %p peer %p",
                        peer->sok, cont ? cont->uin : 0, cont ? cont->nick : "<>", peer->connect, pak, peer);

        switch (peer->connect & CONNECT_MASK)
        {
            case 1:
                peer->connect++;
                TCPSendInit (peer);
                return;
            case 2:
                peer->connect++;
                TCPReceiveInitAck (peer, pak);
                PacketD (pak);
                return;
            case 3:
                peer->connect++;
                peer = TCPReceiveInit (peer, pak);
                PacketD (pak);
                pak = NULL;
                continue;
            case 4:
                peer->connect++;
                TCPSendInitAck (peer);
                continue;
            case 5:
                peer->connect++;
                if (peer->version > 6 && peer->type == TYPE_MSGDIRECT)
                {
                    TCPSendInit2 (peer);
                    return;
                }
                continue;
            case 6:
                peer->connect = 7 | CONNECT_SELECT_R;
                if (peer->version > 6 && peer->type == TYPE_MSGDIRECT)
                {
                    peer = TCPReceiveInit2 (peer, pak);
                    PacketD (pak);
                    pak = NULL;
                }
                continue;
            case 7:
                peer->connect = 48 | CONNECT_SELECT_R;
                continue;
            case 16:
                peer->connect++;
                peer = TCPReceiveInit (peer, pak);
                PacketD (pak);
                pak = NULL;
                continue;
            case 17:
                peer->connect++;
                TCPSendInitAck (peer);
                continue;
            case 18:
                peer->connect++;
                TCPSendInit (peer);
                return;
            case 19:
                peer->connect++;
                TCPReceiveInitAck (peer, pak);
                PacketD (pak);
                if (peer->version > 6 && peer->type == TYPE_MSGDIRECT)
                    return;
                pak = NULL;
                continue;
            case 20:
                peer->connect++;
                if (peer->version > 6 && peer->type == TYPE_MSGDIRECT)
                {
                    peer = TCPReceiveInit2 (peer, pak);
                    PacketD (pak);
                    pak = NULL;
                }
                continue;
            case 21:
                peer->connect = 48 | CONNECT_SELECT_R;
                if (peer->version > 6 && peer->type == TYPE_MSGDIRECT)
                    TCPSendInit2 (peer);
                continue;
            case 48:
                EventD (QueueDequeue (peer, QUEUE_TCP_TIMEOUT, peer->ip));
                if (prG->verbose)
                {
                    M_printf ("%s %s%*s%s ", s_now, COLCONTACT, uiG.nick_len + s_delta (cont->nick), cont->nick, COLNONE);
                    M_print  (i18n (1833, "Peer to peer TCP connection established.\n"));
                }
                peer->connect = CONNECT_OK | CONNECT_SELECT_R;
                if (peer->type == TYPE_FILEDIRECT)
                {
                    peer->dispatch = &PeerFileDispatch;
                    QueueRetry (peer, QUEUE_PEER_FILE, cont);
                }
                else if (peer->type == TYPE_MSGDIRECT)
                {
                    peer->dispatch = &TCPDispatchPeer;
                    QueueRetry (peer, QUEUE_TCP_RESEND, cont);
                }
#ifdef ENABLE_SSL
                /* outgoing peer connection established */
                if (ssl_supported (peer) && peer->ssl_status == SSL_STATUS_REQUEST)
                    if (!TCPSendSSLReq (peer->parent, cont)) 
                        M_printf (i18n (2372, "Could not send SSL request to %s\n"), cont->nick);
#endif
                return;
            case 0:
                return;
            default:
                assert (0);
        }
    }
}

/*
 * Handles all incoming TCP traffic.
 * Queues incoming packets, ignoring resends
 * and deleting CANCEL requests from the queue.
 */
static void TCPDispatchPeer (Connection *peer)
{
    Contact *cont;
    Packet *pak;
    Event *event;
    int i = 0;
    UWORD seq_in = 0, seq, cmd;
    
    ASSERT_MSGDIRECT (peer);
    
    if (!(cont = peer->cont))
    {
        TCPClose (peer);
        return;
    }

    /* Recv all packets before doing anything else.
         The objective is to delete any packets CANCELLED by the remote user. */
    while (M_Is_Set (peer->sok) && i++ <= TCP_MSG_QUEUE)
    {
        if (!(pak = TCPReceivePacket (peer)))
            return;

        if (peer->version > 6)
            PacketRead1 (pak);
               PacketRead4 (pak);
        cmd  = PacketReadAt2 (pak, PacketReadPos (pak));
        seq  = PacketReadAt2 (pak, PacketReadPos (pak) + 4);
        
        /* Make sure this isn't a resend */
        if ((seq_in == 0) || (seq < seq_in))
        { 
            seq_in = seq;

            /* Deal with CANCEL packets now */
            switch (cmd)
            {
                case TCP_CMD_CANCEL:
                    event = QueueDequeue (peer, QUEUE_TCP_RECEIVE, seq_in);
                    if (!event)
                        break;
                    
                    if (prG->verbose)
                    {
                        M_printf (i18n (1807, "Cancelled incoming message (seq %04x) from %s\n"),
                                 seq_in, cont->nick);
                    }
                    EventD (event);
                    PacketD (pak);
                    break;

                default:
                    /* Store the event in the recv queue for handling later */            
                    QueueEnqueueData (peer, QUEUE_TCP_RECEIVE, seq_in, 0, pak, cont,
                                      ContactOptionsSetVals (NULL, CO_ORIGIN, CV_ORIGIN_dcssl, 0),
                                      &TCPCallBackReceive);
                    peer->our_seq--;
                break;
            }
        }

        M_select_init();
        M_set_timeout (0, 100000);
        M_Add_rsocket (peer->sok);
        M_select();
    }
}

/*********************************************/

/*
 * Handles timeout on TCP send/read/whatever
 */
static void TCPCallBackTimeout (Event *event)
{
    Connection *peer = event->conn;
    
    if (!peer)
    {
        EventD (event);
        return;
    }
    ASSERT_ANY_DIRECT (peer);
    assert (event->type == QUEUE_TCP_TIMEOUT);
    
    if ((peer->connect & CONNECT_MASK) && prG->verbose)
    {
        Contact *cont;
        
        if ((cont = peer->cont))
            M_printf (i18n (1850, "Timeout on connection with %s at %s:%ld\n"),
                      cont->nick, s_ip (peer->ip), peer->port);
        TCPClose (peer);
    }
    EventD (event);
}

/*
 * Handles timeout on TCP connect
 */
static void TCPCallBackTOConn (Event *event)
{
    if (!event->conn)
    {
        EventD (event);
        return;
    }
    ASSERT_ANY_DIRECT (event->conn);

    event->conn->connect += 2;
    TCPDispatchConn (event->conn);
    EventD (event);
}

/*
 *  Receives an incoming TCP packet.
 *  Resets socket on error. Paket must be freed.
 */
static Packet *TCPReceivePacket (Connection *peer)
{
    Packet *pak;

    ASSERT_ANY_DIRECT (peer);

    if (!(peer->connect & CONNECT_MASK))
        return NULL;

    pak = UtilIOReceiveTCP (peer);

    if (!peer->connect)
    {
        TCPClose (peer);
        if (pak)
            PacketD (pak);
        return NULL;
    }
    
    if (!pak)
        return NULL;

    peer->stat_pak_rcvd++;
    peer->stat_real_pak_rcvd++;

    if (peer->connect & CONNECT_OK && peer->type == TYPE_MSGDIRECT)
    {
        if (!Decrypt_Pak (peer, pak))
        {
            if (prG->verbose & DEB_TCP)
            {
                M_printf ("%s " COLINDENT "%s", s_now, COLSERVER);
                M_printf (i18n (1789, "Received malformed packet: (%d)"), peer->sok);
                M_printf ("%s\n", COLNONE);
                M_print  (s_dump (pak->data, pak->len));
                M_print  (COLEXDENT "\r");

            }
            TCPClose (peer);
            PacketD (pak);
            return NULL;
        }
    }

    if (prG->verbose & DEB_PACKTCP)
        TCPPrint (pak, peer, FALSE);

    pak->rpos = 0;
    pak->wpos = 0;
    
    return pak;
}

/*
 * Creates a peer-to-peer packet.
 */
Packet *PeerPacketC (Connection *peer, UBYTE cmd)
{
    Packet *pak;
    
    ASSERT_ANY_DIRECT (peer);
    
    pak = PacketC ();
    if (peer->type != TYPE_MSGDIRECT || peer->version > 6 || cmd != PEER_MSG)
        PacketWrite1 (pak, cmd);
    return pak;
}

/*
 * Encrypts and sends a TCP packet.
 * Resets socket on error.
 */
void PeerPacketSend (Connection *peer, Packet *pak)
{
    Packet *tpak;
    
    ASSERT_ANY_DIRECT (peer);
    assert (pak);
    
    if (!(peer->connect & CONNECT_MASK))
        return;

    if (prG->verbose & DEB_PACKTCP)
        TCPPrint (pak, peer, TRUE);

    tpak = PacketC ();
    assert (tpak);
    
    PacketWriteAt2 (tpak, 0, pak->len);
    memcpy (tpak->data + 2, pak->data, pak->len);
    tpak->len = pak->len + 2;
    
    if (peer->type == TYPE_MSGDIRECT)
        if (PacketReadAt1 (pak, 0) == PEER_MSG || (!PacketReadAt1 (pak, 0) && peer->type == TYPE_MSGDIRECT && peer->version == 6)) 
            Encrypt_Pak (peer, tpak);
    
    if (!UtilIOSendTCP (peer, tpak))
        TCPClose (peer);
}


/*
 * Sends a TCP initialization packet.
 */
static void TCPSendInitv6 (Connection *peer)
{
    Packet *pak;
    
    ASSERT_ANY_DIRECT (peer);
    assert (peer->cont);

    if (!(peer->connect & CONNECT_MASK))
        return;

    if (!(peer->our_session))
        peer->our_session = rand ();

    peer->stat_real_pak_sent++;

    pak = PeerPacketC (peer, PEER_INIT);
    PacketWrite2  (pak, 6);                                    /* TCP version      */
    PacketWrite2  (pak, 0);                                    /* TCP revision     */
    PacketWrite4  (pak, peer->cont->uin);                      /* destination UIN  */
    PacketWrite2  (pak, 0);                                    /* unknown - zero   */
    PacketWrite4  (pak, peer->parent->port);                   /* our port         */
    PacketWrite4  (pak, peer->parent->parent->uin);            /* our UIN          */
    PacketWriteB4 (pak, peer->parent->parent->our_outside_ip); /* our (remote) IP  */
    PacketWriteB4 (pak, peer->parent->parent->our_local_ip);   /* our (local)  IP  */
    PacketWrite1  (pak, peer->parent->status);                 /* connection type  */
    PacketWrite4  (pak, peer->parent->port);                   /* our (other) port */
    PacketWrite4  (pak, peer->our_session);                    /* session id       */

    Debug (DEB_TCP, "HS %d uin %ld CONNECT pak %p peer %p",
                    peer->sok, peer->cont->uin, pak, peer);

    PeerPacketSend (peer, pak);
    PacketD (pak);
}

/*
 * Sends a v7/v8 TCP initialization packet.
 */
static void TCPSendInit (Connection *peer)
{
    Packet *pak;
    
    ASSERT_ANY_DIRECT (peer);

    if (peer->version == 6)
    {
        TCPSendInitv6 (peer);
        return;
    }

    if (!(peer->connect & CONNECT_MASK))
        return;

    if (!peer->our_session)
    {
        Contact *cont;

        if (!(cont = peer->cont) || !cont->dc)
        {
            TCPClose (peer);
            return;
        }
        peer->our_session = cont->dc->cookie;
    }

    peer->stat_real_pak_sent++;

    pak = PeerPacketC (peer, PEER_INIT);
    PacketWrite2  (pak, peer->version);                        /* TCP version      */
    PacketWrite2  (pak, 43);                                   /* length           */
    PacketWrite4  (pak, peer->cont->uin);                      /* destination UIN  */
    PacketWrite2  (pak, 0);                                    /* unknown - zero   */
    PacketWrite4  (pak, peer->parent->port);                   /* our port         */
    PacketWrite4  (pak, peer->parent->parent->uin);            /* our UIN          */
    PacketWriteB4 (pak, peer->parent->parent->our_outside_ip); /* our (remote) IP  */
    PacketWriteB4 (pak, peer->parent->parent->our_local_ip);   /* our (local)  IP  */
    PacketWrite1  (pak, peer->parent->status);                 /* connection type  */
    PacketWrite4  (pak, peer->parent->port);                   /* our (other) port */
    PacketWrite4  (pak, peer->our_session);                    /* session id       */
    PacketWrite4  (pak, 0x00000050);
    PacketWrite4  (pak, 0x00000003);
    PacketWrite4  (pak, 0);

    Debug (DEB_TCP, "HS %d uin %ld CONNECTv8 pak %p peer %p",
                    peer->sok, peer->cont->uin, pak, peer);

    PeerPacketSend (peer, pak);
    PacketD (pak);
}

/*
 * Sends the initialization acknowledge packet
 */
static void TCPSendInitAck (Connection *peer)
{
    Packet *pak;
    
    ASSERT_ANY_DIRECT (peer);
    assert (peer->cont);

    if (!(peer->connect & CONNECT_MASK))
        return;

    peer->stat_real_pak_sent++;

    pak = PeerPacketC (peer, PEER_INITACK);
    PacketWrite1 (pak, 0);
    PacketWrite2 (pak, 0);

    Debug (DEB_TCP, "HS %d uin %ld INITACK pak %p peer %p",
                    peer->sok, peer->cont->uin, pak, peer);

    PeerPacketSend (peer, pak);
    PacketD (pak);
}

static void TCPSendInit2 (Connection *peer)
{
    Packet *pak;
    
    ASSERT_ANY_DIRECT (peer);
    assert (peer->cont);
    assert (peer->version > 6);
    
    if (!(peer->connect & CONNECT_MASK))
        return;
    
    peer->stat_real_pak_sent++;
    
    pak = PeerPacketC (peer, PEER_INIT2);
    PacketWrite4 (pak, 10);
    PacketWrite4 (pak, 1);
    PacketWrite4 (pak, (peer->connect & 16) ? 1 : 0);
    PacketWrite4 (pak, 0);
    PacketWrite4 (pak, 0);
    PacketWrite4 (pak, (peer->connect & 16) ? 0x40001 : 0);
    PacketWrite4 (pak, 0);
    PacketWrite4 (pak, (peer->connect & 16) ? 0 : 0x40001);

    Debug (DEB_TCP, "HS %d uin %ld INITMSG pak %p peer %p",
                    peer->sok, peer->cont->uin, pak, peer);

    PeerPacketSend (peer, pak);
    PacketD (pak);
}

#define FAIL(x) { err = x; break; }

static Connection *TCPReceiveInit (Connection *peer, Packet *pak)
{
    Contact *cont;
    UDWORD muin, uin, sid, port, port2, oip, iip;
    UWORD  cmd, len, len2, tcpflag, err, nver, i;
    Connection *peer2;

    ASSERT_ANY_DIRECT (peer);
    assert (pak);

    err = 0;
    while (1)
    {
        cmd       = PacketRead1 (pak);
        nver      = PacketRead2 (pak);
        len       = PacketRead2 (pak);
        len2      = PacketReadLeft (pak);
        muin      = PacketRead4 (pak);
                    PacketRead2 (pak);
        port      = PacketRead4 (pak);
        uin       = PacketRead4 (pak);
        oip       = PacketReadB4 (pak);
        iip       = PacketReadB4 (pak);
        tcpflag   = PacketRead1 (pak);
        port2     = PacketRead4 (pak);
        sid       = PacketRead4 (pak);
        
        /* check validity of this connection; fail silently */
        
        if (cmd != PEER_INIT)
            FAIL (1);
        
        if (nver < 6)
            FAIL (2);
        
        if (nver > 6 && len != len2)
            FAIL (3);
        
        if (nver == 6 && 23 > len2)
            FAIL (4);

        if (muin  != peer->parent->parent->uin)
            FAIL (5);
        
        if (uin  == peer->parent->parent->uin)
            FAIL (6);
        
        if (!(cont = ContactUIN (peer->parent->parent, uin)))
            FAIL (7);

        if (!CONTACT_DC (cont))
            FAIL (10);
        
        if (port && port2 && port != port2)
            FAIL (11);

        peer->version = (peer->parent->version > nver ? nver : peer->parent->version);

        if (!peer->our_session)
            peer->our_session = peer->version > 6 ? cont->dc->cookie : sid;
        if (sid  != peer->our_session)
            FAIL (8);

        if (ContactPrefVal (cont, CO_IGNORE))
            FAIL (9);

        /* okay, the connection seems not to be faked, so update using the following information. */

        peer->cont = cont;
        if (port)     cont->dc->port = port;
        if (oip)      cont->dc->ip_rem = oip;
        if (iip)      cont->dc->ip_loc = iip;
        if (tcpflag)  cont->dc->type = tcpflag;

        Debug (DEB_TCP, "HS %d uin %ld nick %s init pak %p peer %p: ver %04x:%04x port %ld uin %ld SID %08lx type %x",
                        peer->sok, cont->uin, cont->nick, pak, peer, peer->version, len, port, uin, sid, peer->type);

        for (i = 0; (peer2 = ConnectionNr (i)); i++)
            if (     peer2->type == peer->type && peer2->parent == peer->parent
                  && peer2->cont  == peer->cont  && !peer->assoc && peer2 != peer)
                break;

        if (peer2)
        {
            if (peer2->connect & CONNECT_OK)
            {
                TCPClose (peer);
                ConnectionClose (peer);
                return NULL;
            }
            if ((peer2->connect & CONNECT_MASK) == (UDWORD)TCP_STATE_WAITING)
                EventD (QueueDequeue (peer2, QUEUE_TCP_TIMEOUT, peer2->ip));
            if (peer2->sok == -1 && peer2->type == TYPE_FILEDIRECT)
            {
                peer2->sok = peer->sok;
                peer2->our_session = peer->our_session;
                peer2->version = peer->version;
                peer2->connect = peer->connect | CONNECT_SELECT_R;
                peer2->dispatch = peer->dispatch;
                peer->sok = -1;
                ConnectionClose (peer);
                return peer2;
            }
            if (peer2->sok != -1)
                TCPClose (peer2);
            peer->len = peer2->len;
            ConnectionClose (peer2);
        }
        return peer;
    }
    if ((prG->verbose & DEB_TCP) && err)
        M_printf ("%s %s: %d\n", s_now, i18n (2029, "Protocol error on peer-to-peer connection"), err);

    TCPClose (peer);
    return NULL;
}

/*
 * Receives the acknowledge packet for the initialization packet.
 */
static void TCPReceiveInitAck (Connection *peer, Packet *pak)
{
    ASSERT_ANY_DIRECT (peer);
    assert (pak);
    
    if (pak->len != 4 || PacketReadAt4 (pak, 0) != PEER_INITACK)
    {
        Debug (DEB_TCP, "Received malformed initialization acknowledgement packet.\n");
        TCPClose (peer);
    }
}

static Connection *TCPReceiveInit2 (Connection *peer, Packet *pak)
{
    UWORD  cmd, err;
    UDWORD ten, one;

    ASSERT_MSGDIRECT (peer);
    assert (pak);

    if (!pak)
        return peer;

    err = 0;
    while (1)
    {
        cmd     = PacketRead1 (pak);
        ten     = PacketRead4 (pak);
        one     = PacketRead4 (pak);
        
        if (cmd != PEER_INIT2)
            FAIL (201);
        
        if (ten != 10)
            FAIL (202);
        
        if (one != 1)
            FAIL (0);
        
        return peer;
    }

    if (err && (prG->verbose & (DEB_TCP | DEB_PROTOCOL)))
        M_printf ("%s %s: %d\n", s_now, i18n (2029, "Protocol error on peer-to-peer connection"), err);
    else
        peer->connect = 0;
    TCPClose (peer);
    return NULL;
}

/*
 * Quietly close connection.
 */
static void PeerDispatchClose (Connection *conn)
{
    conn->connect = 0;
#ifdef ENABLE_SSL
    ssl_close (conn);
#endif
    TCPClose (conn);
}

/*
 * Close socket and mark as inactive. If verbose, complain.
 */
void TCPClose (Connection *peer)
{
    assert (peer);
    
    if (peer->assoc && peer->assoc->type == TYPE_FILE)
        ConnectionClose (peer->assoc);
    
    if (peer->sok != -1)
    {
        if (peer->connect & CONNECT_MASK && prG->verbose)
        {
            Contact *cont = peer->cont;
            M_printf ("%s ", s_now);
            if (cont)
                M_printf (i18n (1842, "Closing socket %d to %s.\n"), peer->sok, cont->nick);
            else
                M_printf (i18n (1843, "Closing socket %d.\n"), peer->sok);
        }
        sockclose (peer->sok);
    }
    peer->sok     = -1;
    peer->connect = (peer->connect & CONNECT_MASK && !(peer->connect & CONNECT_OK)) ? CONNECT_FAIL : 0;
    peer->our_session = 0;
    if (peer->incoming)
    {
        PacketD (peer->incoming);
        peer->incoming = NULL;
    }
    if (peer->outgoing)
    {
        PacketD (peer->outgoing);
        peer->outgoing = NULL;
    }
    
    if (peer->type == TYPE_FILEDIRECT || !peer->cont)
    {
        peer->close = NULL;
        ConnectionClose (peer);
    }
}

static const char *TCPCmdName (UWORD cmd)
{
    static char buf[8];

    switch (cmd)
    {
        case PEER_INIT:       return "PEER_INIT";
        case PEER_INITACK:    return "PEER_INITACK";
        case PEER_MSG:        return "PEER_MSG";
        case PEER_INIT2:      return "PEER_INIT2";
        
        case TCP_CMD_CANCEL:  return "CANCEL";
        case TCP_CMD_ACK:     return "ACK";
        case TCP_CMD_MESSAGE: return "MSG";
        
        case 0x0029:          return "FILEREQ";
        case 0x002d:          return "CHATREQ";
        case 0x0032:          return "FILEACK";
    }
    snprintf (buf, sizeof (buf), "%04x", cmd);
    buf[7] = '\0';
    return buf;
}

/*
 * Output a TCP packet for debugging.
 */
void TCPPrint (Packet *pak, Connection *peer, BOOL out)
{
    UWORD cmd;
    Contact *cont;
    char *f;
    
    ASSERT_ANY_DIRECT(peer);
    
    pak->rpos = 0;
    cmd = *pak->data;
    cont = peer->cont;

    M_printf ("%s " COLINDENT "%s", s_now, out ? COLCLIENT : COLSERVER);
    M_printf (out ? i18n (2078, "Outgoing TCP packet (%d - %s): %s")
                  : i18n (2079, "Incoming TCP packet (%d - %s): %s"),
              peer->sok, cont ? cont->nick : "", TCPCmdName (cmd));
    M_printf ("%s\n", COLNONE);

    if (peer->connect & CONNECT_OK && peer->type == TYPE_MSGDIRECT && peer->version == 6)
    {
        cmd = 2;
        pak->rpos --;
    }

    if (prG->verbose & DEB_PACKTCPDATA)
        if (cmd != 6)
        {
            M_print (f = PacketDump (pak, peer->type == TYPE_MSGDIRECT ? "gpeer" : "gfile", COLDEBUG, COLNONE));
            free (f);
        }

    M_print (COLEXDENT "\r");
}

/*
 * Create and setup a TCP communication packet.
 */
static Packet *PacketTCPC (Connection *peer, UDWORD cmd)
{
    Packet *pak;
    
    ASSERT_ANY_DIRECT(peer);

    pak = PeerPacketC (peer, PEER_MSG);
    PacketWrite4      (pak, 0);          /* checksum - filled in later */
    PacketWrite2      (pak, cmd);        /* command                    */
    return pak;
}

/*
 * Sends a message via TCP.
 * Adds it to the resend queue until acked.
 */
UBYTE PeerSendMsg (Connection *list, Contact *cont, ContactOptions *opt)
{
    Packet *pak;
    Connection *peer;
    const char *opt_text = NULL;
    UDWORD opt_type = 0;

    ASSERT_MSGLISTEN(list);
    assert (cont);

    if (!cont->dc || !cont->dc->port)
        return RET_DEFER;
    if (cont->uin == list->parent->uin || !(list->connect & CONNECT_MASK))
        return RET_DEFER;
    if (!cont->dc->ip_loc && !cont->dc->ip_rem)
        return RET_DEFER;
    
    if (!ContactOptionsGetStr (opt, CO_MSGTEXT, &opt_text))
        return RET_FAIL;
    if (!ContactOptionsGetVal (opt, CO_MSGTYPE, &opt_type))
        opt_type = MSG_NORM;

    switch (opt_type & 0xff)
    {
        case MSG_NORM:
        case MSG_URL:
        case MSG_GET_AWAY:
        case MSG_GET_OCC:
        case MSG_GET_NA:
        case MSG_GET_DND:
        case MSG_GET_FFC:
        case MSG_SSL_OPEN:
        case MSG_SSL_CLOSE:
        case MSG_GET_VER:
            break;
        default:
            return RET_DEFER;
    }

    if ((peer = ConnectionFind (TYPE_MSGDIRECT, cont, list)))
        if (peer->connect & CONNECT_FAIL)
            return RET_DEFER;

    if (!peer || ~peer->connect & CONNECT_OK)
    {
       TCPDirectOpen (list, cont);
       return RET_DEFER;
    }

    ASSERT_MSGDIRECT(peer);
    
    pak = PacketTCPC (peer, TCP_CMD_MESSAGE);
    SrvMsgAdvanced   (pak, peer->our_seq, opt_type, list->parent->status,
                      cont->status, -1, c_out_for (opt_text, cont, opt_type));
    PacketWrite4 (pak, TCP_COL_FG);      /* foreground color           */
    PacketWrite4 (pak, TCP_COL_BG);      /* background color           */
    if (CONT_UTF8 (cont, opt_type))
        PacketWriteDLStr (pak, CAP_GID_UTF8);

    peer->stat_real_pak_sent++;

    QueueEnqueueData (peer, QUEUE_TCP_RESEND, peer->our_seq--, time (NULL),
                      pak, cont, opt, &TCPCallBackResend);
    return RET_INPR;
}

/*
 * Sends a message via TCP.
 * Adds it to the resend queue until acked.
 */
BOOL TCPSendFiles (Connection *list, Contact *cont, const char *description, const char **files, const char **as, int count)
{
    Packet *pak;
    Connection *peer, *flist, *fpeer;
    int i, rc, sumlen, sum;
    time_t now = time (NULL);
    str_s filenames = { NULL, 0, 0};

    ASSERT_MSGLISTEN(list);
    assert (cont);
    
    if (!count)
        return TRUE;
    if (count < 0)
        return FALSE;
    if (!cont->dc || !cont->dc->port)
        return FALSE;
    
    if (cont->uin == list->parent->uin)
        return FALSE;
    if (!(list->connect & CONNECT_MASK))
        return FALSE;
    if (!cont->dc->ip_loc && !cont->dc->ip_rem)
        return FALSE;

    if (!TCPDirectOpen (list, cont))
        return FALSE;
    if (!(peer = ConnectionFind (TYPE_MSGDIRECT, cont, list)))
        return FALSE;
    if (peer->connect & CONNECT_FAIL)
            return FALSE;

    ASSERT_MSGDIRECT(peer);
    
    if (!(flist = PeerFileCreate (peer->parent->parent)))
    
    ASSERT_FILELISTEN(flist);

    if (ConnectionFind (TYPE_FILEDIRECT, cont, flist))
        return FALSE;

    fpeer = ConnectionClone (flist, TYPE_FILEDIRECT);
    
    ASSERT_FILEDIRECT(fpeer);
    
    fpeer->cont    = cont;
    fpeer->connect = 77;
    fpeer->version = flist->version;
    
    s_init (&filenames, "", 0);
    for (sumlen = sum = i = 0; i < count; i++)
    {
        struct stat fstat;
        
        if (stat (files[i], &fstat))
        {
            rc = errno;
            M_printf (i18n (2071, "Couldn't stat file %s: %s (%d)\n"),
                     files[i], strerror (rc), rc);
        }
        else
        {
            M_printf ("%s %s%*s%s ", s_now, COLCONTACT, uiG.nick_len + s_delta (cont->nick),
                      cont->nick, COLNONE);
            M_printf (i18n (2091, "Queueing %s as %s for transfer.\n"), files[i], as[i]);
            if (sum)
                s_catn (&filenames, ", ", 2);
            s_cat (&filenames, as[i]);
            sum++;
            sumlen += fstat.st_size;
            pak = PeerPacketC (fpeer, 2);
            PacketWrite1 (pak, 0);
            PacketWriteLNTS (pak, c_out_to (as[i], cont));
            PacketWriteLNTS (pak, "");
            PacketWrite4 (pak, fstat.st_size);
            PacketWrite4 (pak, 0);
            PacketWrite4 (pak, 64);
            QueueEnqueueData (fpeer, QUEUE_PEER_FILE, sum, now, pak, cont,
                              ContactOptionsSetVals (NULL, CO_FILENAME, files[i], 0), &PeerFileResend);
        }
    }
    
    if (!sum)
    {
        ConnectionClose (fpeer);
        s_done (&filenames);
        return FALSE;
    }
    
    pak = PeerPacketC (fpeer, 0);
    PacketWrite4 (pak, 0);
    PacketWrite4 (pak, sum);
    PacketWrite4 (pak, sumlen);
    PacketWrite4 (pak, 64);
    PacketWriteLNTS (pak, cont->nick);
    QueueEnqueueData (fpeer, QUEUE_PEER_FILE, 0, now, pak, cont,
                      ContactOptionsSetVals (NULL, CO_MSGTYPE, MSG_FILE, CO_MSGTEXT, description, 0),
                      &PeerFileResend);
        
    if (peer->version < 8)
    {
        pak = PacketTCPC (peer, TCP_CMD_MESSAGE);
        SrvMsgAdvanced   (pak, peer->our_seq, MSG_FILE, list->parent->status,
                          cont->status, -1, c_out_to (description, cont));
        PacketWrite2 (pak, 0);
        PacketWrite2 (pak, 0);
        PacketWriteLNTS (pak, filenames.txt);
        PacketWrite4 (pak, sumlen);
        PacketWrite4 (pak, 0);
    }
    else
    {
        pak = PacketTCPC (peer, TCP_CMD_MESSAGE);
        SrvMsgAdvanced   (pak, peer->our_seq, MSG_EXTENDED, list->parent->status,
                          cont->status, -1, "");
        SrvMsgGreet (pak, 0x29, description, 0, sumlen, filenames.txt);
    }

    peer->stat_real_pak_sent++;

    QueueEnqueueData (peer, QUEUE_TCP_RESEND, peer->our_seq--, now, pak, cont,
                      ContactOptionsSetVals (NULL, CO_MSGTYPE, MSG_FILE, CO_MSGTEXT, description, 0),
                      &TCPCallBackResend);
    s_done (&filenames);
    return TRUE;
}

/*
 * Resends TCP packets if necessary
 */
static void TCPCallBackResend (Event *event)
{
    Contact *cont = event->cont;
    Connection *peer = event->conn;
    Packet *pak = event->pak;
    const char *opt_text;
    UWORD delta;
    UDWORD opt_type, opt_trans;
    char isfile;
    
    if (!ContactOptionsGetVal (event->opt, CO_MSGTYPE, &opt_type))
        opt_type = MSG_NORM;
    isfile = opt_type == MSG_FILE || opt_type == MSG_SSL_OPEN;
    if (!ContactOptionsGetStr (event->opt, CO_MSGTEXT, &opt_text))
        opt_text = "";

    if (!peer || !cont)
    {
        if (!peer)
            M_printf (i18n (2092, "TCP message %s discarded - lost session.\n"), opt_text);
        EventD (event);
        return;
    }

    ASSERT_MSGDIRECT (peer);
    assert (pak);
    
    if (!ContactOptionsGetVal (event->opt, CO_MSGTRANS, &opt_trans))
        opt_trans = CV_MSGTRANS_ANY;
    
    if (peer->connect & CONNECT_MASK && event->attempts < (isfile ? MAX_RETRY_P2PFILE_ATTEMPTS : MAX_RETRY_P2P_ATTEMPTS))
    {
        if (peer->connect & CONNECT_OK)
        {
            if (event->attempts > 1)
                IMIntMsg (cont, peer->parent->parent, NOW, STATUS_OFFLINE, INT_MSGTRY_DC, opt_text, NULL);

            if (event->attempts < 2)
                PeerPacketSend (peer, pak);
            event->attempts++;
            event->due = time (NULL) + QUEUE_P2P_RESEND_ACK;
        }
        else
            event->due = time (NULL) + 1;
        QueueEnqueue (event);
        return;
    }

    if (!isfile)
    {
        TCPClose (peer);
        peer->connect = CONNECT_FAIL;
        ContactOptionsSetVal (event->opt, CO_MSGTRANS, opt_trans & ~CV_MSGTRANS_DC);
        delta = (peer->version > 6 ? 1 : 0);
        if (PacketReadAt2 (pak, 4 + delta) == TCP_CMD_MESSAGE)
        {
            IMCliMsg (peer->parent->parent, cont, event->opt);
            event->opt = NULL;
        }
        else
            M_printf (i18n (1844, "TCP message %04x discarded after timeout.\n"), PacketReadAt2 (pak, 4 + delta));
    }
    EventD (event);
}

static void PeerCallbackReceiveAdvanced (Event *event)
{
    Debug (DEB_TCP, "%p %p %p\n", event, event ? event->conn : NULL, event ? event->pak : NULL);
    if (event && event->conn && event->pak && event->conn->type & TYPEF_ANY_DIRECT)
        PeerPacketSend (event->conn, event->pak);
#ifdef ENABLE_SSL
    switch (event->conn->ssl_status)
    {
        case SSL_STATUS_INIT:
            ssl_connect (event->conn, 0);
            break;
        case SSL_STATUS_CLOSE:
            /* Could not figure out how to say good bye to licq correctly.
             * That's why we do a simple close.
             */
            if (event->conn->close)
                event->conn->close(event->conn);
            break;
    }
#endif /* ENABLE_SSL */
    EventD (event);
}

/*
 * Handles a just received packet.
 */
static void TCPCallBackReceive (Event *event)
{
    Connection *serv, *peer;
    Event *oldevent;
    Contact *cont;
    Packet *pak, *ack_pak = NULL;
    strc_t ctmp, ctext, cname, creason;
    char *tmp, *text, *name;
    UWORD cmd, type, seq, port /*, unk*/;
    UDWORD /*len,*/ status /*, flags, xtmp1, xtmp2, xtmp3*/;
    const char *opt_text;
    UDWORD opt_origin, opt_type;
    
    if (!ContactOptionsGetVal (event->opt, CO_ORIGIN, &opt_origin))
        opt_origin = 0;

    if (!(peer = event->conn) || !(cont = event->cont))
    {
        EventD (event);
        return;
    }
    
    ASSERT_MSGDIRECT (peer);
    assert (event->pak);
    
    pak = event->pak;
    serv = peer->parent->parent;
    pak->tpos = pak->rpos;

    cmd    = PacketRead2 (pak);

    switch (cmd)
    {
        case TCP_CMD_ACK:
            /*unk=*/ PacketRead2 (pak);
            seq    = PacketRead2 (pak);
            /*xtmp1*/PacketRead4 (pak);
            /*xtmp2*/PacketRead4 (pak);
            /*xtmp3*/PacketRead4 (pak);
            type   = PacketRead2 (pak);
            status = PacketRead2 (pak);
            /*flags*/PacketRead2 (pak);
            ctmp   = PacketReadL2Str (pak, NULL);
            
            if (!(oldevent = QueueDequeue (peer, QUEUE_TCP_RESEND, seq)))
                break;
            
            if (!ContactOptionsGetStr (oldevent->opt, CO_MSGTEXT, &opt_text))
                opt_text = "";
            if (!ContactOptionsGetVal (oldevent->opt, CO_MSGTYPE, &opt_type))
                opt_type = type;
            
            if (opt_type != type && type != MSG_EXTENDED)
            {
                /* D'oh! */
                M_printf ("FIXME: message type mismatch: %d vs %ld\n", type, opt_type); /* FIXME */
                EventD (oldevent);
                break;
            }

            tmp = strdup (ConvFromCont (ctmp, cont));
            
            switch (type)
            {
                case MSG_NORM:
                case MSG_URL:
                    IMIntMsg (cont, peer, NOW, STATUS_OFFLINE, opt_origin == CV_ORIGIN_dc ? INT_MSGACK_DC : INT_MSGACK_SSL, opt_text, NULL);
                    if (~cont->oldflags & CONT_SEENAUTO && strlen (tmp) && strcmp (tmp, opt_text))
                    {
                        IMSrvMsg (cont, serv, NOW, ContactOptionsSetVals (NULL, CO_ORIGIN, opt_origin, CO_MSGTYPE, MSG_AUTO, CO_MSGTEXT, tmp, 0));
                        cont->oldflags |= CONT_SEENAUTO;
                    }
                    break;

                case MSGF_GETAUTO | MSG_GET_AWAY:
                case MSGF_GETAUTO | MSG_GET_OCC:
                case MSGF_GETAUTO | MSG_GET_NA:
                case MSGF_GETAUTO | MSG_GET_DND:
                case MSGF_GETAUTO | MSG_GET_FFC:
                case MSGF_GETAUTO | MSG_GET_VER:
                    IMSrvMsg (cont, serv, NOW, ContactOptionsSetVals (NULL, CO_ORIGIN, opt_origin, CO_MSGTYPE, opt_type,
                              CO_MSGTEXT, tmp, CO_STATUS, status & ~MSGF_GETAUTO, 0));
                    break;

                case MSG_FILE:
                    port = PacketReadB2 (pak);
                    if (PeerFileAccept (peer, status, port))
                        IMIntMsg (cont, serv, NOW, status, INT_FILE_ACKED, tmp,
                          ContactOptionsSetVals (NULL, CO_PORT, port, CO_MSGTEXT, opt_text, 0));
                    else
                        IMIntMsg (cont, serv, NOW, status, INT_FILE_REJED, tmp,
                          ContactOptionsSetVals (NULL, CO_MSGTEXT, opt_text, 0));
                    break;
#ifdef ENABLE_SSL                    
                /* We never stop SSL connections on our own. That's why 
                 * MSG_SSL_CLOSE is not handled here.
                 */
                case MSG_SSL_OPEN:
                    if (!status && !strcmp (tmp, "1"))
                        ssl_connect (peer, 1);
                    else
                    {
                        Debug (DEB_SSL, "%s (%ld) is not SSL capable", cont->nick, cont->uin);
#ifdef ENABLE_TCL
                        TCLEvent (cont, "ssl", "incapable");
#endif
                    }
                    break;
#endif
                case MSG_EXTENDED:
                    cmd    = PacketRead2 (pak); 
                             PacketReadB4 (pak);   /* ID */
                             PacketReadB4 (pak);
                             PacketReadB4 (pak);
                             PacketReadB4 (pak);
                             PacketRead2 (pak);    /* EMPTY */
                    ctext  = PacketReadL4Str (pak, NULL);
                             PacketReadB4 (pak);   /* UNKNOWN */
                             PacketReadB4 (pak);
                             PacketReadB4 (pak);
                             PacketReadB2 (pak);
                             PacketRead1 (pak);
                             PacketRead4 (pak);    /* LEN */
                    creason= PacketReadL4Str (pak, NULL);
                    port   = PacketReadB2 (pak);
                             PacketRead2 (pak);    /* PAD */
                    cname  = PacketReadL2Str (pak, NULL);
                    /*len=*/ PacketRead4 (pak);
                             /* PORT2 ignored */

                    text = strdup (ConvFromCont (ctext, cont));
                    name = strdup (ConvFromCont (cname, cont));

                    switch (cmd)
                    {
                        case 0x0029:
                            if (PeerFileAccept (peer, status, port))
                                IMIntMsg (cont, serv, NOW, status, INT_FILE_ACKED, tmp,
                                    ContactOptionsSetVals (NULL, CO_PORT, port, CO_MSGTEXT, opt_text, 0));
                            else
                                IMIntMsg (cont, serv, NOW, status, INT_FILE_REJED, tmp,
                                    ContactOptionsSetVals (NULL, CO_MSGTEXT, opt_text, 0));
                            break;
                            
                        default:
                            cmd = 0;
                    }
                    free (name);
                    free (text);
                    if (cmd)
                        break;

                    /* fall through */
                default:
                    Debug (DEB_TCP, "ACK %d uin %ld nick %s pak %p peer %p seq %04x",
                                     peer->sok, cont->uin, cont->nick, oldevent->pak, peer, seq);
            }
            free (tmp);
            EventD (oldevent);
            break;

        case TCP_CMD_MESSAGE:
            ack_pak = PacketTCPC (peer, TCP_CMD_ACK);
            event->opt = ContactOptionsSetVals (event->opt, CO_ORIGIN, opt_origin, 0);
            oldevent = QueueEnqueueData (event->conn, QUEUE_ACKNOWLEDGE, rand () % 0xff,
                         (time_t)-1, ack_pak, cont, NULL, &PeerCallbackReceiveAdvanced);
            SrvReceiveAdvanced (serv, event, event->pak, oldevent);
            break;
    }
    EventD (event);
}

/*
 *
 *  Encryption/Decryption
 *
 */

const UBYTE client_check_data[] = {
    "As part of this software beta version Mirabilis is "
    "granting a limited access to the ICQ network, "
    "servers, directories, listings, information and databases (\""
    "ICQ Services and Information\"). The "
    "ICQ Service and Information may databases (\""
    "ICQ Services and Information\"). The "
    "ICQ Service and Information may\0"
};

/*
 * Encrypts/Decrypts TCP packets
 * (leeched from licq) 
 */
static void Encrypt_Pak (Connection *peer, Packet *pak)
{
    UDWORD B1, M1, check, size, i;
    UBYTE X1, X2, X3, *p;
    UDWORD hex, key;

    p = pak->data + 2;
    size = pak->len - 2;
    
    if (peer->version > 6)
    {
        p++;
        size--;
    }

    /* calculate verification data */
    M1 = (rand() % ((size < 255 ? size : 255) - 10)) + 10;
    X1 = p[M1] ^ 0xff;
    X2 = rand() % 220;
    X3 = client_check_data[X2] ^ 0xff;
    B1 = (p[4] << 24) | (p[6] << 16) | (p[4] << 8) | p[6];

    /* calculate checkcode */
    check = (M1 << 24) | (X1 << 16) | (X2 << 8) | X3;
    check ^= B1;

    /* main XOR key */
    key = 0x67657268 * size + check;

    /* XORing the actual data */
    for (i = 0; i < (size + 3) / 4; )
    {
        hex = key + client_check_data[i & 0xff];
        p[i++] ^=  hex        & 0xff;
        p[i++] ^= (hex >>  8) & 0xff;
        p[i++] ^= (hex >> 16) & 0xff;
        p[i++] ^= (hex >> 24) & 0xff;
    }

    /* storing the checkcode */
    PacketWriteAt4 (pak, peer->version > 6 ? 3 : 2, check);
}

static BOOL Decrypt_Pak (Connection *peer, Packet *pak)
{
    UDWORD hex, key, B1, M1, check, i, size;
    UBYTE X1, X2, X3, *p;

    p = pak->data;
    size = pak->len;
    
    if (peer->version > 6)
    {
        p++;
        size--;
    }

    /* Get checkcode */
    check = PacketReadAt4 (pak, peer->version > 6 ? 1 : 0);

    /* primary decryption */
    key = 0x67657268 * size + check;
 
    for (i = 4; i < (size + 3) / 4; ) 
    {
        hex = key + client_check_data[i & 0xff];
        p[i++] ^=  hex        & 0xff;
        p[i++] ^= (hex >>  8) & 0xff;
        p[i++] ^= (hex >> 16) & 0xff;
        p[i++] ^= (hex >> 24) & 0xff;
    }

    B1 = (p[4]<<24) | (p[6]<<16) | (p[4]<<8) | (p[6]<<0);

    /* special decryption */
    B1 ^= check;

    /* validate packet */
    M1 = (B1 >> 24) & 0xFF;
    if (M1 < 10 || M1 >= size)
        return (-1);

    X1 = p[M1] ^ 0xFF;
    if (((B1 >> 16) & 0xFF) != X1) 
        return (-1);

    X2 = ((B1 >> 8) & 0xFF);
    if (X2 < 220)
    {
        X3 = client_check_data[X2] ^ 0xFF;
        if((B1 & 0xFF) != X3)
            return FALSE;
    }

    return TRUE;
}
#endif /* ENABLE_PEER2PEER */
