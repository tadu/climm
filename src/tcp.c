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
#include "util_extra.h"
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
#include "util_str.h"
#include "util_syntax.h"
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

#ifdef ENABLE_PEER2PEER

static void        TCPDispatchPeer    (Connection *peer);
static void        PeerDispatchClose  (Connection *conn);

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

/*********************************************/

/*
 * "Logs in" TCP connection by opening listening socket.
 */
void ConnectionInitPeer (Connection *list)
{
    assert (list);
    
    if (list->ver < 6 || list->ver > 8)
    {
        M_printf (i18n (2024, "Unknown protocol version %d for ICQ peer-to-peer protocol.\n"), list->ver);
        return;
    }

    if (list->ver == 6)
        M_print (i18n (2046, "You may want to use protocol version 8 for the ICQ peer-to-peer protocol instead.\n"));

    M_printf (i18n (1777, "Opening peer-to-peer connection at localhost:%ld... "), list->port);

    list->connect     = 0;
    list->our_seq     = -1;
    list->type        = TYPE_MSGLISTEN;
    list->flags       = 0;
    list->dispatch    = &TCPDispatchMain;
    list->reconnect   = &TCPDispatchReconn;
    list->close       = &PeerDispatchClose;
    list->our_session = 0;
    list->ip          = 0;
    s_repl (&list->server, NULL);
    list->port        = list->spref->port;
    list->uin         = list->parent ? list->parent->uin : -1;

    UtilIOConnectTCP (list);
}

/*
 *  Starts establishing a TCP connection to given contact.
 */
BOOL TCPDirectOpen (Connection *list, Contact *cont)
{
    Connection *peer;

    ASSERT_MSGLISTEN (list);

    if (!cont || !cont->dc || cont->dc->version < 6)
        return FALSE;
    if (cont->uin == list->parent->uin)
        return FALSE;

    if ((peer = ConnectionFind (TYPE_MSGDIRECT, cont->uin, list)))
    {
        if (peer->connect & CONNECT_MASK)
            return TRUE;
    }
    else
        peer = ConnectionClone (list, TYPE_MSGDIRECT);
    
    if (!peer)
        return FALSE;
    
    peer->port   = 0;
    peer->uin    = cont->uin;
    peer->flags  = 0;
    peer->spref  = NULL;
    peer->parent = list;
    peer->assoc  = NULL;
    peer->ver    = list->ver <= cont->dc->version ? list->ver : cont->dc->version;
    peer->close  = &PeerDispatchClose;

    TCPDispatchConn (peer);

    return TRUE;
}

/*
 * Closes TCP message/file connection(s) to given contact.
 */
void TCPDirectClose (Connection *list, Contact *cont)
{
    Connection *peer;
    int i;

    ASSERT_MSGLISTEN (list);
    if (!cont)
        return;
    
    for (i = 0; (peer = ConnectionNr (i)); i++)
        if (peer->uin == cont->uin && peer->parent == list)
            if (peer->type == TYPE_MSGDIRECT || peer->type == TYPE_FILEDIRECT)
                TCPClose (peer);
}

/*
 * Switchs off TCP for a given uin.
 */
void TCPDirectOff (Connection *list, Contact *cont)
{
    Connection *peer;
    
    ASSERT_MSGLISTEN (list);
    if (!cont)
        return;
    peer = ConnectionFind (TYPE_MSGDIRECT, cont->uin, list);
    
    if (!peer)
        peer = ConnectionC ();
    if (!peer)
        return;
    
    peer->uin     = cont->uin;
    peer->connect = CONNECT_FAIL;
    peer->type    = TYPE_MSGDIRECT;
    peer->flags   = 0;
    peer->parent  = list;

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
        Contact *cont;
        
        if (!(cont = ContactUIN (peer->parent->parent, peer->uin)))
            return;
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
                if (list->type == TYPE_MSGLISTEN && list->parent && list->parent->ver > 6
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

    if (list->ver == 6)
        M_print (i18n (2046, "You may want to use protocol version 8 for the ICQ peer-to-peer protocol instead.\n"));

    peer->flags = 0;
    peer->spref = NULL;
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
    peer->uin = 0;
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
        cont = ContactUIN (peer->parent->parent, peer->uin);
        if (!cont || !cont->dc)
        {
            TCPClose (peer);
            return;
        }
        
        Debug (DEB_TCP, "Conn: uin %ld nick %s state %x", peer->uin, cont->nick, peer->connect);

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
                    M_printf (i18n (2034, "Opening TCP connection at %s:%ld... "),
                             s_ip (peer->ip), peer->port);
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
                    M_printf (i18n (2034, "Opening TCP connection at %s:%ld... "),
                             s_ip (peer->ip), peer->port);
                }
                UtilIOConnectTCP (peer);
                return;
            case 5:
            {
                if (peer->parent && peer->parent->parent && peer->parent->parent->ver < 7)
                {
                    CmdPktCmdTCPRequest (peer->parent->parent, cont, cont->dc->port);
                    QueueEnqueueData (peer, QUEUE_TCP_TIMEOUT, peer->ip, time (NULL) + 30,
                                      NULL, cont->uin, NULL, &TCPCallBackTOConn);
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
                    M_printf (i18n (2034, "Opening TCP connection at %s:%ld... "),
                             s_ip (peer->ip), peer->port);
                    M_print (i18n (1785, "success.\n"));
                }
                QueueEnqueueData (peer, QUEUE_TCP_TIMEOUT, peer->ip, time (NULL) + 10,
                                  NULL, cont->uin, NULL, &TCPCallBackTimeout);
                peer->connect = 1 | CONNECT_SELECT_R;
                peer->dispatch = &TCPDispatchShake;
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
    {
        pak = TCPReceivePacket (peer);
        if (!pak)
            return;
    }

    while (1)
    {
        if (!peer)
            return;

        cont = ContactUIN (peer->parent->parent, peer->uin);
        if (!cont && (peer->connect & CONNECT_MASK) != 16)
        {
            TCPClose (peer);
            peer->connect = CONNECT_FAIL;
            if (pak)
                PacketD (pak);
            return;
        }
        
        Debug (DEB_TCP, "HS %d uin %ld nick %s state %d pak %p peer %p",
                        peer->sok, peer->uin, cont ? cont->nick : "<>", peer->connect, pak, peer);

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
                if (peer->ver > 6 && peer->type == TYPE_MSGDIRECT)
                {
                    TCPSendInit2 (peer);
                    return;
                }
                continue;
            case 6:
                peer->connect = 7 | CONNECT_SELECT_R;
                if (peer->ver > 6 && peer->type == TYPE_MSGDIRECT)
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
                if (peer->ver > 6 && peer->type == TYPE_MSGDIRECT)
                    return;
                pak = NULL;
                continue;
            case 20:
                peer->connect++;
                if (peer->ver > 6 && peer->type == TYPE_MSGDIRECT)
                {
                    peer = TCPReceiveInit2 (peer, pak);
                    PacketD (pak);
                    pak = NULL;
                }
                continue;
            case 21:
                peer->connect = 48 | CONNECT_SELECT_R;
                if (peer->ver > 6 && peer->type == TYPE_MSGDIRECT)
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
                    QueueRetry (peer, QUEUE_PEER_FILE, peer->uin);
                }
                else
                {
                    peer->dispatch = &TCPDispatchPeer;
                    QueueRetry (peer, QUEUE_TCP_RESEND, peer->uin);
                }
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
    
    cont = ContactUIN (peer->parent->parent, peer->uin);
    if (!cont)
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

        if (peer->ver > 6)
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
                    QueueEnqueueData (peer, QUEUE_TCP_RECEIVE, seq_in,
                                      0, pak, cont->uin, NULL, &TCPCallBackReceive);

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
        
        if ((cont = ContactUIN (peer->parent->parent, peer->uin)))
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
                M_printf ("%s " COLINDENT COLSERVER "", s_now);
                M_printf (i18n (1789, "Received malformed packet: (%d)"), peer->sok);
                M_print  (COLNONE "\n");
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
    
    pak = PacketC ();
    if (peer->type != TYPE_MSGDIRECT || peer->ver > 6 || cmd != PEER_MSG)
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
        if (PacketReadAt1 (pak, 0) == PEER_MSG || (!PacketReadAt1 (pak, 0) && peer->type == TYPE_MSGDIRECT && peer->ver == 6)) 
            Encrypt_Pak (peer, tpak);
    
    if (!UtilIOSendTCP (peer, tpak))
        TCPClose (peer);
}


/*
 * Sends a TCP initialization packet.
 */
void TCPSendInitv6 (Connection *peer)
{
    Packet *pak;
    
    ASSERT_ANY_DIRECT (peer);

    if (!(peer->connect & CONNECT_MASK))
        return;

    if (!(peer->our_session))
        peer->our_session = rand ();

    peer->stat_real_pak_sent++;

    pak = PeerPacketC (peer, PEER_INIT);
    PacketWrite2  (pak, 6);                                    /* TCP version      */
    PacketWrite2  (pak, 0);                                    /* TCP revision     */
    PacketWrite4  (pak, peer->uin);                            /* destination UIN  */
    PacketWrite2  (pak, 0);                                    /* unknown - zero   */
    PacketWrite4  (pak, peer->parent->port);                   /* our port         */
    PacketWrite4  (pak, peer->parent->parent->uin);            /* our UIN          */
    PacketWriteB4 (pak, peer->parent->parent->our_outside_ip); /* our (remote) IP  */
    PacketWriteB4 (pak, peer->parent->parent->our_local_ip);   /* our (local)  IP  */
    PacketWrite1  (pak, peer->parent->status);                 /* connection type  */
    PacketWrite4  (pak, peer->parent->port);                   /* our (other) port */
    PacketWrite4  (pak, peer->our_session);                    /* session id       */

    Debug (DEB_TCP, "HS %d uin %ld CONNECT pak %p peer %p",
                    peer->sok, peer->uin, pak, peer);

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

    if (peer->ver == 6)
    {
        TCPSendInitv6 (peer);
        return;
    }

    if (!(peer->connect & CONNECT_MASK))
        return;

    if (!peer->our_session)
    {
        Contact *cont;

        cont = ContactUIN (peer->parent->parent, peer->uin);
        if (!cont || !cont->dc)
        {
            TCPClose (peer);
            return;
        }
        peer->our_session = cont->dc->cookie;
    }

    peer->stat_real_pak_sent++;

    pak = PeerPacketC (peer, PEER_INIT);
    PacketWrite2  (pak, peer->ver);                    /* TCP version      */
    PacketWrite2  (pak, 43);                           /* length           */
    PacketWrite4  (pak, peer->uin);                    /* destination UIN  */
    PacketWrite2  (pak, 0);                            /* unknown - zero   */
    PacketWrite4  (pak, peer->parent->port);           /* our port         */
    PacketWrite4  (pak, peer->parent->parent->uin);             /* our UIN          */
    PacketWriteB4 (pak, peer->parent->parent->our_outside_ip);  /* our (remote) IP  */
    PacketWriteB4 (pak, peer->parent->parent->our_local_ip);    /* our (local)  IP  */
    PacketWrite1  (pak, peer->parent->status);                  /* connection type  */
    PacketWrite4  (pak, peer->parent->port);           /* our (other) port */
    PacketWrite4  (pak, peer->our_session);            /* session id       */
    PacketWrite4  (pak, 0x00000050);
    PacketWrite4  (pak, 0x00000003);
    PacketWrite4  (pak, 0);

    Debug (DEB_TCP, "HS %d uin %ld CONNECTv8 pak %p peer %p",
                    peer->sok, peer->uin, pak, peer);

    PeerPacketSend (peer, pak);
    PacketD (pak);
}

/*
 * Sends the initialization packet
 */
static void TCPSendInitAck (Connection *peer)
{
    Packet *pak;
    
    ASSERT_ANY_DIRECT (peer);

    if (!(peer->connect & CONNECT_MASK))
        return;

    peer->stat_real_pak_sent++;

    pak = PeerPacketC (peer, PEER_INITACK);
    PacketWrite1 (pak, 0);
    PacketWrite2 (pak, 0);

    Debug (DEB_TCP, "HS %d uin %ld INITACK pak %p peer %p",
                    peer->sok, peer->uin, pak, peer);

    PeerPacketSend (peer, pak);
    PacketD (pak);
}

static void TCPSendInit2 (Connection *peer)
{
    Packet *pak;
    
    ASSERT_ANY_DIRECT (peer);
    
    if (peer->ver == 6)
        return;
    
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
                    peer->sok, peer->uin, pak, peer);

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

        peer->ver = (peer->ver > nver ? nver : peer->ver);

        if (!peer->our_session)
            peer->our_session = peer->ver > 6 ? cont->dc->cookie : sid;
        if (sid  != peer->our_session)
            FAIL (8);

        if (ContactPrefVal (cont, CO_IGNORE))
            FAIL (9);

        /* okay, the connection seems not to be faked, so update using the following information. */

        peer->uin = uin;
        if (port)     cont->dc->port = port;
        if (oip)      cont->dc->ip_rem = oip;
        if (iip)      cont->dc->ip_loc = iip;
        if (tcpflag)  cont->dc->type = tcpflag;

        Debug (DEB_TCP, "HS %d uin %ld nick %s init pak %p peer %p: ver %04x:%04x port %ld uin %ld SID %08lx type %x",
                        peer->sok, peer->uin, cont->nick, pak, peer, peer->ver, len, port, uin, sid, peer->type);

        for (i = 0; (peer2 = ConnectionNr (i)); i++)
            if (     peer2->type == peer->type && peer2->parent == peer->parent
                  && peer2->uin  == peer->uin  && !peer->assoc && peer2 != peer)
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
        Debug (DEB_TCP | DEB_PROTOCOL, i18n (1841, "Received malformed initialization acknowledgement packet.\n"));
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
            Contact *cont = ContactUIN (peer->parent->parent, peer->uin);
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
    
    if (peer->type == TYPE_FILEDIRECT || !peer->uin)
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
    cont = ContactUIN (peer->parent->parent, peer->uin);

    M_printf ("%s " COLINDENT "%s", s_now, out ? COLCLIENT : COLSERVER);
    M_printf (out ? i18n (2078, "Outgoing TCP packet (%d - %s): %s")
                  : i18n (2079, "Incoming TCP packet (%d - %s): %s"),
              peer->sok, peer->uin && cont ? cont->nick : "", TCPCmdName (cmd));
    M_print (COLNONE "\n");

    if (peer->connect & CONNECT_OK && peer->type == TYPE_MSGDIRECT && peer->ver == 6)
    {
        cmd = 2;
        pak->rpos --;
    }

    if (prG->verbose & DEB_PACKTCPDATA)
        if (cmd != 6)
        {
            M_print (f = PacketDump (pak, peer->type == TYPE_MSGDIRECT ? "gpeer" : "gfile"));
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
BOOL TCPSendMsg (Connection *list, Contact *cont, const char *msg, UWORD type)
{
    Packet *pak;
    Connection *peer;

    if (!cont || !cont->dc || !cont->dc->port)
        return FALSE;
    if (!list || !list->parent)
        return FALSE;
    if (cont->uin == list->parent->uin)
        return FALSE;
    if (!(list->connect & CONNECT_MASK))
        return FALSE;
    if (!cont->dc->ip_loc && !cont->dc->ip_rem)
        return FALSE;

    ASSERT_MSGLISTEN(list);
    
    peer = ConnectionFind (TYPE_MSGDIRECT, cont->uin, list);
    if (peer)
    {
        if (peer->connect & CONNECT_FAIL)
            return FALSE;
        if (~peer->connect & CONNECT_OK)
            peer = NULL;
    }
    if (!peer)
    {
       TCPDirectOpen (list, cont);
       return FALSE;
    }

    ASSERT_MSGDIRECT(peer);

    pak = PacketTCPC (peer, TCP_CMD_MESSAGE);
    SrvMsgAdvanced   (pak, peer->our_seq, type, list->parent->status,
                      cont->status, -1, c_out_for (msg, cont, type));
    PacketWrite4 (pak, TCP_COL_FG);      /* foreground color           */
    PacketWrite4 (pak, TCP_COL_BG);      /* background color           */
    if (CONT_UTF8 (cont, type))
        PacketWriteDLStr (pak, CAP_GID_UTF8);

    peer->stat_real_pak_sent++;

    QueueEnqueueData (peer, QUEUE_TCP_RESEND, peer->our_seq--, time (NULL),
                      pak, cont->uin, ExtraSet (NULL, EXTRA_MESSAGE, 1, msg), &TCPCallBackResend);
    return 1;
}

/*
 * Sends a message via TCP.
 * Adds it to the resend queue until acked.
 */
UBYTE PeerSendMsg (Connection *list, Contact *cont, Extra *extra)
{
    Packet *pak;
    Connection *peer;
    const char *e_msg_text = NULL;
    UDWORD e_msg_type = 0;

    if (!list || !list->parent || !cont || !cont->dc || !cont->dc->port)
        return RET_DEFER;
    if (cont->uin == list->parent->uin || !(list->connect & CONNECT_MASK))
        return RET_DEFER;
    if (!cont->dc->ip_loc && !cont->dc->ip_rem)
        return RET_DEFER;
    
    e_msg_text = ExtraGetS (extra, EXTRA_MESSAGE);
    e_msg_type = ExtraGet  (extra, EXTRA_MESSAGE);

    switch (e_msg_type & 0xff)
    {
        case MSG_NORM:
        case MSG_URL:
        case MSG_GET_AWAY:
        case MSG_GET_OCC:
        case MSG_GET_NA:
        case MSG_GET_DND:
        case MSG_GET_FFC:
        case MSG_GET_VER:
            break;
        default:
            return RET_DEFER;
    }

    ASSERT_MSGLISTEN(list);
    
    peer = ConnectionFind (TYPE_MSGDIRECT, cont->uin, list);
    if (peer)
    {
        if (peer->connect & CONNECT_FAIL)
            return RET_DEFER;
        if (~peer->connect & CONNECT_OK)
            peer = NULL;
    }
    if (!peer)
    {
       TCPDirectOpen (list, cont);
       return RET_DEFER;
    }

    ASSERT_MSGDIRECT(peer);
    
    pak = PacketTCPC (peer, TCP_CMD_MESSAGE);
    SrvMsgAdvanced   (pak, peer->our_seq, e_msg_type, list->parent->status,
                      cont->status, -1, c_out_for (e_msg_text, cont, e_msg_type));
    PacketWrite4 (pak, TCP_COL_FG);      /* foreground color           */
    PacketWrite4 (pak, TCP_COL_BG);      /* background color           */
    if (CONT_UTF8 (cont, e_msg_type))
        PacketWriteDLStr (pak, CAP_GID_UTF8);

    peer->stat_real_pak_sent++;

    QueueEnqueueData (peer, QUEUE_TCP_RESEND, peer->our_seq--, time (NULL),
                      pak, cont->uin, extra, &TCPCallBackResend);
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

    if (!count)
        return TRUE;
    if (count < 0)
        return FALSE;
    if (!cont || !cont->dc || !cont->dc->port)
        return FALSE;
    
    if (!list || !list->parent)
        return FALSE;
    if (cont->uin == list->parent->uin)
        return FALSE;
    if (!(list->connect & CONNECT_MASK))
        return FALSE;
    if (!cont->dc->ip_loc && !cont->dc->ip_rem)
        return FALSE;

    ASSERT_MSGLISTEN(list);
    
    peer = ConnectionFind (TYPE_MSGDIRECT, cont->uin, list);
    if (peer)
    {
        if (peer->connect & CONNECT_FAIL)
            return FALSE;
    }
    else
    {
        if (!TCPDirectOpen (list, cont))
            return FALSE;
        peer = ConnectionFind (TYPE_MSGDIRECT, cont->uin, list);
        if (!peer)
            return FALSE;
    }

    ASSERT_MSGDIRECT(peer);
    
    flist = PeerFileCreate (peer->parent->parent);
    if (!flist)
        return FALSE;
    
    ASSERT_FILELISTEN(flist);

    if (ConnectionFind (TYPE_FILEDIRECT, cont->uin, flist))
        return FALSE;

    fpeer = ConnectionClone (flist, TYPE_FILEDIRECT);
    
    assert (fpeer);
    
    fpeer->uin     = cont->uin;
    fpeer->connect = 77;
        
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
            M_printf ("%s " COLCONTACT "%*s" COLNONE " ", s_now, uiG.nick_len + s_delta (cont->nick), cont->nick);
            M_printf (i18n (2091, "Queueing %s as %s for transfer.\n"), files[i], as[i]);
            sum++;
            sumlen += fstat.st_size;
            pak = PeerPacketC (fpeer, 2);
            PacketWrite1 (pak, 0);
            PacketWriteLNTS (pak, c_out_to (as[i], cont));
            PacketWriteLNTS (pak, "");
            PacketWrite4 (pak, fstat.st_size);
            PacketWrite4 (pak, 0);
            PacketWrite4 (pak, 64);
            QueueEnqueueData (fpeer, QUEUE_PEER_FILE, sum, now, pak, cont->uin,
                              ExtraSet (NULL, EXTRA_MESSAGE, -1, files[i]), &PeerFileResend);
        }
    }
    
    if (!sum)
    {
        ConnectionClose (fpeer);
        return FALSE;
    }
    
    pak = PeerPacketC (fpeer, 0);
    PacketWrite4 (pak, 0);
    PacketWrite4 (pak, sum);
    PacketWrite4 (pak, sumlen);
    PacketWrite4 (pak, 64);
    PacketWriteLNTS (pak, "Sender's nick");
    QueueEnqueueData (fpeer, QUEUE_PEER_FILE, 0, now, pak, cont->uin,
                      ExtraSet (NULL, EXTRA_MESSAGE, MSG_FILE, description), &PeerFileResend);
        
    if (peer->ver < 8)
    {
        pak = PacketTCPC (peer, TCP_CMD_MESSAGE);
        SrvMsgAdvanced   (pak, peer->our_seq, MSG_FILE, list->parent->status,
                          cont->status, -1, c_out_to (description, cont));
        PacketWrite2 (pak, 0);
        PacketWrite2 (pak, 0);
        PacketWriteLNTS (pak, "many, really many, files");
        PacketWrite4 (pak, sumlen);
        PacketWrite4 (pak, 0);
    }
    else
    {
        pak = PacketTCPC (peer, TCP_CMD_MESSAGE);
        SrvMsgAdvanced   (pak, peer->our_seq, MSG_EXTENDED, list->parent->status,
                          cont->status, -1, "");
        SrvMsgGreet (pak, 0x29, description, 0, sumlen, "many, really many files");
    }

    peer->stat_real_pak_sent++;

    QueueEnqueueData (peer, QUEUE_TCP_RESEND, peer->our_seq--, now, pak, cont->uin,
                      ExtraSet (NULL, EXTRA_MESSAGE, MSG_FILE, description), &TCPCallBackResend);
    return TRUE;
}

/*
 * Resends TCP packets if necessary
 */
static void TCPCallBackResend (Event *event)
{
    Contact *cont = ContactUIN (event->conn, event->uin);
    Connection *peer = event->conn;
    Packet *pak = event->pak;
    UWORD delta, e_trans;

    if (!peer || !cont)
    {
        if (!peer)
            M_printf (i18n (2092, "TCP message %s discarded - lost session.\n"), ExtraGetS (event->extra, EXTRA_MESSAGE));
        EventD (event);
        return;
    }

    ASSERT_MSGDIRECT (peer);
    assert (pak);
    
    e_trans = ExtraGet (event->extra, EXTRA_TRANS);
    
    if (event->attempts >= MAX_RETRY_P2P_ATTEMPTS)
    {
        if (ExtraGet (event->extra, EXTRA_MESSAGE) != MSG_FILE)
            TCPClose (peer);
    }
    else if (peer->connect & CONNECT_MASK)
    {
        if (peer->connect & CONNECT_OK)
        {
            if (event->attempts > 1)
                IMIntMsg (cont, peer, NOW, STATUS_OFFLINE, INT_MSGTRY_DC, ExtraGetS (event->extra, EXTRA_MESSAGE), NULL);

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

    peer->connect = CONNECT_FAIL;
    event->extra = ExtraSet (event->extra, EXTRA_TRANS, e_trans & ~EXTRA_TRANS_DC, NULL);
    delta = (peer->ver > 6 ? 1 : 0);
    if (PacketReadAt2 (pak, 4 + delta) == TCP_CMD_MESSAGE)
    {
        IMCliMsg (peer->parent->parent, cont, event->extra);
        event->extra = NULL;
    }
    else
        M_printf (i18n (1844, "TCP message %04x discarded after timeout.\n"), PacketReadAt2 (pak, 4 + delta));
    
    EventD (event);
}

static void PeerCallbackReceiveAdvanced (Event *event)
{
    Debug (DEB_TCP, "%p %p %p\n", event, event ? event->conn : NULL, event ? event->pak : NULL);
    if (event && event->conn && event->pak)
    {
        PeerPacketSend (event->conn, event->pak);
        event->pak = NULL;
    }
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
    const char *ctmp, *ctext, *cname, *reason;
    char *tmp, *text, *name;
    UWORD cmd, type, seq, port /*, unk*/;
    UDWORD /*len,*/ status /*, flags, xtmp1, xtmp2, xtmp3*/;
    const char *e_msg_text;
/*    UWORD e_msg_type;*/

    if (!(peer = event->conn))
    {
        EventD (event);
        return;
    }
    
    ASSERT_MSGDIRECT (peer);
    assert (event->pak);
    
    pak = event->pak;
    serv = peer->parent->parent;
    pak->tpos = pak->rpos;

    cont = ContactUIN (serv, event->uin);
    if (!cont)
        return;
    
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
            ctmp   = PacketReadL2Str (pak, NULL)->txt;
            /* fore/background color ignored */
            tmp = strdup (c_in_to (ctmp, cont));
            
            if (!(oldevent = QueueDequeue (peer, QUEUE_TCP_RESEND, seq)))
            {
                free (tmp);
                break;
            }
            
            e_msg_text = ExtraGetS (oldevent->extra, EXTRA_MESSAGE);
            /*e_msg_type = */ ExtraGet  (oldevent->extra, EXTRA_MESSAGE);

            switch (type)
            {
                case MSG_NORM:
                case MSG_URL:
                    IMIntMsg (cont, peer, NOW, STATUS_OFFLINE, INT_MSGACK_DC, e_msg_text, NULL);
                    if ((~cont->oldflags & CONT_SEENAUTO) && strlen (tmp))
                    {
                        IMSrvMsg (cont, peer, NOW, ExtraSet (ExtraSet (NULL,
                                  EXTRA_ORIGIN, EXTRA_ORIGIN_dc, NULL),
                                  EXTRA_MESSAGE, MSG_AUTO, tmp));
                        cont->oldflags |= CONT_SEENAUTO;
                    }
                    break;

                case MSGF_GETAUTO | MSG_GET_AWAY:
                case MSGF_GETAUTO | MSG_GET_OCC:
                case MSGF_GETAUTO | MSG_GET_NA:
                case MSGF_GETAUTO | MSG_GET_DND:
                case MSGF_GETAUTO | MSG_GET_FFC:
                case MSGF_GETAUTO | MSG_GET_VER:
                    IMSrvMsg (cont, peer, NOW, ExtraSet (ExtraSet (ExtraSet (NULL,
                              EXTRA_ORIGIN, EXTRA_ORIGIN_dc, NULL),
                              EXTRA_STATUS, status & ~MSGF_GETAUTO, NULL),
                              EXTRA_MESSAGE, type, tmp));
                    break;

                case MSG_FILE:
                    port = PacketReadB2 (pak);
                    if (PeerFileAccept (peer, status, port))
                        IMIntMsg (cont, peer, NOW, status, INT_FILE_ACKED, tmp, ExtraSet (NULL, 0, port, e_msg_text));
                    else
                        IMIntMsg (cont, peer, NOW, status, INT_FILE_REJED, tmp, ExtraSet (NULL, 0, port, e_msg_text));
                    break;

                case MSG_EXTENDED:
                    cmd    = PacketRead2 (pak); 
                             PacketReadB4 (pak);   /* ID */
                             PacketReadB4 (pak);
                             PacketReadB4 (pak);
                             PacketReadB4 (pak);
                             PacketRead2 (pak);    /* EMPTY */
                    ctext  = PacketReadL4Str (pak, NULL)->txt;
                             PacketReadB4 (pak);   /* UNKNOWN */
                             PacketReadB4 (pak);
                             PacketReadB4 (pak);
                             PacketReadB2 (pak);
                             PacketRead1 (pak);
                             PacketRead4 (pak);    /* LEN */
                    reason = PacketReadL4Str (pak, NULL)->txt;
                    port   = PacketReadB2 (pak);
                             PacketRead2 (pak);    /* PAD */
                    cname  = PacketReadL2Str (pak, NULL)->txt;
                    /*len=*/ PacketRead4 (pak);
                             /* PORT2 ignored */

                    text = strdup (c_in_to (ctext, cont));
                    name = strdup (c_in_to (cname, cont));

                    switch (cmd)
                    {
                        case 0x0029:
                            if (PeerFileAccept (peer, status, port))
                                IMIntMsg (cont, peer, NOW, status, INT_FILE_ACKED, tmp, ExtraSet (NULL, 0, port, e_msg_text));
                            else
                                IMIntMsg (cont, peer, NOW, status, INT_FILE_REJED, tmp, ExtraSet (NULL, 0, port, e_msg_text));
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
                                     peer->sok, peer->uin, cont->nick, oldevent->pak, peer, seq);
            }
            free (tmp);
            EventD (oldevent);
            break;

        case TCP_CMD_MESSAGE:
            ack_pak = PacketTCPC (peer, TCP_CMD_ACK);
            event->extra = ExtraSet (event->extra, EXTRA_ORIGIN, EXTRA_ORIGIN_dc, NULL);
            oldevent = QueueEnqueueData (event->conn, QUEUE_ACKNOWLEDGE, rand () % 0xff,
                         (time_t)-1, ack_pak, cont->uin, NULL, &PeerCallbackReceiveAdvanced);
            SrvReceiveAdvanced (event->conn->parent->parent, event, event->pak, oldevent);
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
    UDWORD B1, M1, check, size;
    int i;
    UBYTE X1, X2, X3, *p;
    UDWORD hex, key;

    p = pak->data + 2;
    size = pak->len - 2;
    
    if (peer->ver > 6)
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
    PacketWriteAt4 (pak, peer->ver > 6 ? 3 : 2, check);
}

static BOOL Decrypt_Pak (Connection *peer, Packet *pak)
{
    UDWORD hex, key, B1, M1, check;
    int i, size;
    UBYTE X1, X2, X3, *p;

    p = pak->data;
    size = pak->len;
    
    if (peer->ver > 6)
    {
        p++;
        size--;
    }

    /* Get checkcode */
    check = PacketReadAt4 (pak, peer->ver > 6 ? 1 : 0);

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
