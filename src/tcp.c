/*
 * This file handles TCP client-to-client communications.
 *
 *  Author/Copyright: James Schofield (jschofield@ottawa.com) 22 Feb 2001
 *  Lots of changes from Rüdiger Kuhlmann. File transfer by Rüdiger Kuhlmann.
 *  This file may be distributed under version 2 of the GPL licence.
 */

#include "micq.h"
#include "util_ui.h"
#include "util_io.h"
#include "file_util.h"
#include "util.h"
#include "buildmark.h"
#include "network.h"
#include "cmd_user.h"
#include "icq_response.h"
#include "preferences.h"
#include "server.h"
#include "contact.h"
#include "session.h"
#include "packet.h"
#include "tcp.h"
#include "cmd_pkt_cmd_v5.h"
#include "cmd_pkt_v8_snac.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <sys/stat.h>

#ifdef ENABLE_PEER2PEER

static void       TCPDispatchPeer    (Session *peer);

static Packet    *TCPReceivePacket   (Session *peer);

static void       TCPSendInit        (Session *peer);
static void       TCPSendInitAck     (Session *peer);
static void       TCPSendInit2       (Session *peer);
static Session   *TCPReceiveInit     (Session *peer, Packet *pak);
static void       TCPReceiveInitAck  (Session *peer, Packet *pak);
static Session   *TCPReceiveInit2    (Session *peer, Packet *pak);


static Packet    *PacketTCPC         (Session *peer, UDWORD cmd, UDWORD seq,
                                      UWORD type, UWORD flags, UWORD status, const char *msg);
static void       TCPGreet           (Packet *pak, UWORD cmd, char *reason,
                                      UWORD port, UDWORD len, char *msg);

static void       TCPCallBackTimeout (Event *event);
static void       TCPCallBackTOConn  (Event *event);
static void       TCPCallBackResend  (Event *event);
static void       TCPCallBackReceive (Event *event);

static void       Encrypt_Pak        (Session *peer, Packet *pak);
static int        Decrypt_Pak        (UBYTE *pak, UDWORD size);
static int        TCPSendMsgAck      (Session *peer, UWORD seq, UWORD sub_cmd, BOOL accept);

/*********************************************/

/*
 * "Logs in" TCP connection by opening listening socket.
 */
void SessionInitPeer (Session *list)
{
    assert (list);
    
    if (list->ver < 6 || list->ver > 8)
    {
        M_print (i18n (2024, "Unknown protocol version %d for ICQ peer-to-peer protocol.\n"), list->ver);
        return;
    }

    if (list->ver == 6)
        M_print (i18n (2046, "You may want to use protocol version 8 for the ICQ peer-to-peer protocol instead.\n"));

    M_print (i18n (1777, "Opening peer-to-peer connection at localhost:%d... "), list->port);

    list->connect     = 0;
    list->our_seq     = -1;
    list->type        = TYPE_MSGLISTEN;
    list->flags       = 0;
    list->dispatch    = &TCPDispatchMain;
    list->reconnect   = &TCPDispatchReconn;
    list->our_session = 0;

    UtilIOConnectTCP (list);
}

/*
 *  Starts establishing a TCP connection to given contact.
 */
BOOL TCPDirectOpen (Session *list, UDWORD uin)
{
    Session *peer;
    Contact *cont;

    ASSERT_MSGLISTEN (list);

    if (uin == list->parent->uin)
        return FALSE;

    UtilCheckUIN (list->parent, uin);
    cont = ContactFind (uin);
    if (!cont || cont->TCP_version < 6)
        return FALSE;

    if ((peer = SessionFind (TYPE_MSGDIRECT, uin, list)))
    {
        if (peer->connect & CONNECT_MASK)
            return TRUE;
    }
    else
        peer = SessionClone (list, TYPE_MSGDIRECT);
    
    if (!peer)
        return FALSE;
    
    peer->port   = 0;
    peer->uin    = uin;
    peer->flags  = 0;
    peer->spref  = NULL;
    peer->parent = list;
    peer->assoc  = NULL;
    peer->ver    = list->ver <= cont->TCP_version ? list->ver : cont->TCP_version;

    TCPDispatchConn (peer);

    return TRUE;
}

/*
 * Closes TCP message/file connection(s) to given contact.
 */
void TCPDirectClose (Session *list, UDWORD uin)
{
    Session *peer;
    int i;

    ASSERT_MSGLISTEN (list);
    
    for (i = 0; (peer = SessionNr (i)); i++)
        if (peer->uin == uin && peer->parent == list)
            if (peer->type == TYPE_MSGDIRECT || peer->type == TYPE_FILEDIRECT)
                TCPClose (peer);
}

/*
 * Switchs off TCP for a given uin.
 */
void TCPDirectOff (Session *list, UDWORD uin)
{
    Contact *cont;
    Session *peer;
    
    ASSERT_MSGLISTEN (list);
    peer = SessionFind (TYPE_MSGDIRECT, uin, list);
    cont = ContactFind (uin);
    
    if (!peer && cont)
        peer = SessionC ();

    if (!peer)
        return;
    
    peer->uin     = cont->uin;
    peer->connect = CONNECT_FAIL;
    peer->type    = TYPE_MSGDIRECT;
    peer->flags   = 0;

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
void TCPDispatchReconn (Session *peer)
{
    ASSERT_ANY_DIRECT(peer);

    if (prG->verbose)
    {
        Time_Stamp ();
        M_print (" %s%10s%s ", COLCONTACT, ContactFindName (peer->uin), COLNONE);
        M_print (i18n (2023, "Direct connection closed by peer.\n"));
    }
    TCPClose (peer);
}

/*
 * Accepts a new direct connection.
 */
void TCPDispatchMain (Session *list)
{
    struct sockaddr_in sin;
    Session *peer;
    int tmp, rc;
 
    ASSERT_ANY_LISTEN(list);

    if (list->connect & CONNECT_OK)
    {
        if ((rc = UtilIOError (list)))
        {
            M_print (i18n (2051, "Error on socket: %s (%d)\n"), strerror (rc), rc);
            if (list->sok > 0)
                sockclose (list->sok);
            list->sok = -1;
            list->connect = 0;
            return;
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

    tmp = sizeof (sin);
    peer = SessionClone (list, list->type == TYPE_MSGLISTEN ? TYPE_MSGDIRECT : TYPE_FILEDIRECT);
    
    if (!peer)
    {
        M_print (i18n (1914, "Can't allocate connection structure.\n"));
        list->connect = 0;
        if (list->sok)
            sockclose (list->sok);
        return;
    }

#ifdef WIP
    M_print ("New incoming direct(?) (%d) %x,%x\n", list->sok, list->type, peer->type);
#endif

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
        peer->sok  = accept (list->sok, (struct sockaddr *)&sin, &tmp);
        
        if (peer->sok <= 0)
        {
            peer->connect = 0;
            peer->sok     = -1;
            return;
        }
    }

    peer->connect = 16 | CONNECT_SELECT_R;
    peer->uin = 0;
}

/*
 * Continues connecting.
 */
void TCPDispatchConn (Session *peer)
{
    Contact *cont;
    
    ASSERT_ANY_DIRECT (peer);
    
    peer->dispatch = &TCPDispatchConn;

    while (1)
    {
        cont = ContactFind (peer->uin);
        if (!cont)
        {
            TCPClose (peer);
            return;
        }
        
        Debug (DEB_TCP, "Conn: uin %d nick %s state %x", peer->uin, ContactFindName (peer->uin), peer->connect);

        switch (peer->connect & CONNECT_MASK)
        {
            case 0:
                if (!cont->outside_ip)
                {
                    peer->connect = 3;
                    break;
                }
                peer->server  = NULL;
                if (peer->type == TYPE_MSGDIRECT)
                {
                    peer->ip      = cont->outside_ip;
                    peer->port    = cont->port;
                }
                peer->connect = 1;
                
                if (prG->verbose)
                {
                    Time_Stamp ();
                    M_print (" %s%10s%s ", COLCONTACT, ContactFindName (peer->uin), COLNONE);
                    M_print (i18n (2034, "Opening TCP connection at %s:%d... "),
                              UtilIOIP (peer->ip), peer->port);
                }
                UtilIOConnectTCP (peer);
                return;
            case 3:
                if (peer->type == TYPE_FILEDIRECT)
                {
                    sockclose (peer->sok);
                    peer->sok = -1;
                    peer->connect = 0;
                    return;
                }
                if (!cont->local_ip || !cont->port)
                {
                    peer->connect = CONNECT_FAIL;
                    return;
                }
                peer->server  = NULL;
                peer->ip      = cont->local_ip;
                peer->port    = cont->port;
                peer->connect = 3;
                
                if (prG->verbose)
                {
                    Time_Stamp ();
                    M_print (" %s%10s%s ", COLCONTACT, ContactFindName (peer->uin), COLNONE);
                    M_print (i18n (2034, "Opening TCP connection at %s:%d... "),
                             UtilIOIP (peer->ip), peer->port);
                }
                UtilIOConnectTCP (peer);
                return;
            case 5:
            {
                if (peer->parent && peer->parent->parent && peer->parent->parent->ver < 7)
                {
                    CmdPktCmdTCPRequest (peer->parent->parent, cont->uin, cont->port);
                    QueueEnqueueData (peer, peer->ip, QUEUE_TCP_TIMEOUT,
                                      cont->uin, time (NULL) + 30,
                                      NULL, NULL, &TCPCallBackTOConn);
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
                    Time_Stamp ();
                    M_print (" %s%10s%s ", COLCONTACT, ContactFindName (cont->uin), COLNONE);
                    M_print (i18n (2034, "Opening TCP connection at %s:%d... "),
                             UtilIOIP (peer->ip), peer->port);
                    M_print (i18n (1785, "success.\n"));
                }
                QueueEnqueueData (peer, peer->ip, QUEUE_TCP_TIMEOUT,
                                  cont->uin, time (NULL) + 10,
                                  NULL, NULL, &TCPCallBackTimeout);
                peer->connect = 1 | CONNECT_SELECT_R;
                peer->dispatch = &TCPDispatchShake;
                TCPDispatchShake (peer);
                return;
            case TCP_STATE_WAITING:
            case TCP_STATE_WAITING + 2:
                if (prG->verbose)
                    M_print (i18n (1855, "TCP connection to %s at %s:%d failed.\n") , cont->nick, UtilIOIP (peer->ip), peer->port);
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
void TCPDispatchShake (Session *peer)
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

        Debug (DEB_TCP, "HS %d uin %d nick %s state %d pak %p peer %d",
                        peer->sok, peer->uin, ContactFindName (peer->uin), peer->connect, pak, peer);

        cont = ContactFind (peer->uin);
        if (!cont && (peer->connect & CONNECT_MASK) != 16)
        {
            TCPClose (peer);
            peer->connect = CONNECT_FAIL;
            if (pak)
                PacketD (pak);
            return;
        }
        
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
                    peer = TCPReceiveInit2 (peer, pak);
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
                QueueDequeue (peer->ip, QUEUE_TCP_TIMEOUT);
                if (prG->verbose)
                {
                    Time_Stamp ();
                    M_print (" %s%10s%s ", COLCONTACT, ContactFindName (peer->uin), COLNONE);
                    M_print (i18n (1833, "Peer to peer TCP connection established.\n"), cont->nick);
                }
                peer->connect = CONNECT_OK | CONNECT_SELECT_R;
                if (peer->type == TYPE_FILEDIRECT)
                {
                    peer->dispatch = &PeerFileDispatch;
                    QueueRetry (peer->uin, QUEUE_PEER_FILE);
                }
                else
                {
                    peer->dispatch = &TCPDispatchPeer;
                    QueueRetry (peer->uin, QUEUE_TCP_RESEND);
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
static void TCPDispatchPeer (Session *peer)
{
    Contact *cont;
    Packet *pak;
    Event *event;
    int i = 0;
    UWORD seq_in = 0, seq, cmd;
    
    ASSERT_MSGDIRECT (peer);
    
    cont = ContactFind (peer->uin);
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
                    event = QueueDequeue (seq_in, QUEUE_TCP_RECEIVE);
                    if (!event)
                        break;
                    
                    if (prG->verbose)
                    {
                        M_print (i18n (1807, "Cancelled incoming message (seq %04x) from %s\n"),
                                 seq_in, cont->nick);
                    }
                    PacketD (event->pak);
                    free (event);
                    PacketD (pak);
                    break;

                default:
                    /* Store the event in the recv queue for handling later */            
                    QueueEnqueueData (peer, seq_in, QUEUE_TCP_RECEIVE,
                                      0, 0, pak, (UBYTE *)cont, &TCPCallBackReceive);

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
    Session *peer = event->sess;
    
    if (!peer)
    {
        free (event);
        return;
    }
    
    ASSERT_ANY_DIRECT (peer);
    assert (event->type == QUEUE_TCP_TIMEOUT);
    
    if ((peer->connect & CONNECT_MASK) && prG->verbose)
    {
        M_print (i18n (1850, "Timeout on connection with %s at %s:%d\n"),
                 ContactFindName (peer->uin), UtilIOIP (peer->ip), peer->port);
        TCPClose (peer);
    }
    free (event);
}

/*
 * Handles timeout on TCP connect
 */
static void TCPCallBackTOConn (Event *event)
{
    if (!event->sess)
    {
        free (event);
        return;
    }

    ASSERT_ANY_DIRECT (event->sess);

    event->sess->connect += 2;
    TCPDispatchConn (event->sess);
    free (event);
}

/*
 *  Receives an incoming TCP packet.
 *  Resets socket on error. Paket must be freed.
 */
static Packet *TCPReceivePacket (Session *peer)
{
    Packet *pak;

    ASSERT_ANY_DIRECT (peer);

    if (!(peer->connect & CONNECT_MASK))
        return NULL;

    pak = UtilIOReceiveTCP (peer);

    if (!peer->connect)
    {
        TCPClose (peer);
        return NULL;
    }
    
    if (!pak)
        return NULL;

    peer->stat_pak_rcvd++;
    peer->stat_real_pak_rcvd++;

    if (peer->connect & CONNECT_OK && peer->type == TYPE_MSGDIRECT)
    {
        if (Decrypt_Pak ((UBYTE *) &pak->data + (peer->ver > 6 ? 1 : 0), pak->len - (peer->ver > 6 ? 1 : 0)) < 0)
        {
            if (prG->verbose & DEB_TCP)
            {
                Time_Stamp ();
                M_print (" " COLINDENT COLSERVER "");
                M_print (i18n (1789, "Received malformed packet: (%d)"), peer->sok);
                M_print (COLNONE "\n");
                Hex_Dump (pak->data, pak->len);
                M_print (COLEXDENT "\r");

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
Packet *PeerPacketC (Session *peer, UBYTE cmd)
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
void PeerPacketSend (Session *peer, Packet *pak)
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
void TCPSendInitv6 (Session *peer)
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

    Debug (DEB_TCP, "HS %d uin %d nick %s CONNECT pak %p peer %d",
                    peer->sok, peer->uin, ContactFindName (peer->uin), pak, peer);

    PeerPacketSend (peer, pak);
    PacketD (pak);
}

/*
 * Sends a v7/v8 TCP initialization packet.
 */
static void TCPSendInit (Session *peer)
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

        cont = ContactFind (peer->uin);
        if (!cont)
        {
            TCPClose (peer);
            return;
        }
        peer->our_session = cont->cookie;
    }

    peer->stat_real_pak_sent++;

    pak = PeerPacketC (peer, PEER_INIT);
    PacketWrite2  (pak, peer->parent->ver);            /* TCP version      */
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

    Debug (DEB_TCP, "HS %d uin %d nick %s CONNECTv8 pak %p peer %d",
                    peer->sok, peer->uin, ContactFindName (peer->uin), pak, peer);

    PeerPacketSend (peer, pak);
    PacketD (pak);
}

/*
 * Sends the initialization packet
 */
static void TCPSendInitAck (Session *peer)
{
    Packet *pak;
    
    ASSERT_ANY_DIRECT (peer);

    if (!(peer->connect & CONNECT_MASK))
        return;

    peer->stat_real_pak_sent++;

    pak = PeerPacketC (peer, PEER_INITACK);
    PacketWrite1 (pak, 0);
    PacketWrite2 (pak, 0);

    Debug (DEB_TCP, "HS %d uin %d nick %s INITACK pak %p peer %d",
                    peer->sok, peer->uin, ContactFindName (peer->uin), pak, peer);

    PeerPacketSend (peer, pak);
    PacketD (pak);
}

static void TCPSendInit2 (Session *peer)
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

    Debug (DEB_TCP, "HS %d uin %d nick %s INITMSG pak %p peer %d",
                    peer->sok, peer->uin, ContactFindName (peer->uin), pak, peer);

    PeerPacketSend (peer, pak);
    PacketD (pak);
}

#define FAIL(x) { err = x; break; }

static Session *TCPReceiveInit (Session *peer, Packet *pak)
{
    Contact *cont;
    UDWORD muin, uin, sid, port, port2, oip, iip;
    UWORD  cmd, len, len2, tcpflag, err, nver, i;
    Session *peer2;

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
        
        if (!(cont = ContactFind (uin)))
            FAIL (7);

        peer->ver = (peer->ver > nver ? nver : peer->ver);

        if (!peer->our_session)
            peer->our_session = peer->ver > 6 ? cont->cookie : sid;
        if (sid  != peer->our_session)
            FAIL (8);

        if (cont->flags & CONT_IGNORE)
            FAIL (9);

        /* okay, the connection seems not to be faked, so update using the following information. */

        peer->uin = uin;
        if (port2)    cont->port = port2;
        if (port)     cont->port = port;
        if (oip)      cont->outside_ip = oip;
        if (iip)      cont->local_ip = iip;
        if (tcpflag)  cont->connection_type = tcpflag;

        Debug (DEB_TCP, "HS %d uin %d nick %s init pak %p peer %d: ver %04x:%04x port %d uin %d SID %08x",
                        peer->sok, peer->uin, ContactFindName (peer->uin), pak, peer, peer->ver, len, port, uin, sid);

        for (i = 0; (peer2 = SessionNr (i)); i++)
            if (     peer2->type == peer->type && peer2->parent == peer->parent
                  && peer2->uin  == peer->uin  && !peer->assoc && peer2 != peer)
                break;

        if (peer2)
        {
            if (peer2->connect & CONNECT_OK)
            {
                TCPClose (peer);
                SessionClose (peer);
                return NULL;
            }
            if ((peer2->connect & CONNECT_MASK) == (UDWORD)TCP_STATE_WAITING)
                QueueDequeue (peer2->ip, QUEUE_TCP_TIMEOUT);
            if (peer2->sok != -1)
                TCPClose (peer2);
            SessionClose (peer2);
        }
        return peer;
    }
    if ((prG->verbose & DEB_TCP) && err)
    {
        Time_Stamp ();
        M_print (" %s: %d\n", i18n (2029, "Protocol error on peer-to-peer connection"), err);
    }
    TCPClose (peer);
    return NULL;
}

/*
 * Receives the acknowledge packet for the initialization packet.
 */
static void TCPReceiveInitAck (Session *peer, Packet *pak)
{
    ASSERT_ANY_DIRECT (peer);
    assert (pak);
    
    if (pak->len != 4 || PacketReadAt4 (pak, 0) != PEER_INITACK)
    {
        Debug (DEB_TCP | DEB_PROTOCOL, i18n (1841, "Received malformed initialization acknowledgement packet.\n"));
        TCPClose (peer);
    }
}

static Session *TCPReceiveInit2 (Session *peer, Packet *pak)
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
    {
        Time_Stamp ();
        M_print (" %s: %d\n", i18n (2029, "Protocol error on peer-to-peer connection"), err);
    }
    else
        peer->connect = 0;
    TCPClose (peer);
    return NULL;
}


/*
 * Close socket and mark as inactive. If verbose, complain.
 */
void TCPClose (Session *peer)
{
    assert (peer);
    
    if (peer->assoc && peer->assoc->type == TYPE_FILE)
    {
        if (peer->assoc->sok != -1)
        {
            close (peer->assoc->sok);
            peer->assoc->sok = -1;
        }
        SessionClose (peer->assoc);
    }
    
    if (peer->sok != -1)
    {
        if (peer->connect & CONNECT_MASK && prG->verbose)
        {
            Time_Stamp ();
            M_print (" ");
            if (peer->uin)
                M_print (i18n (1842, "Closing socket %d to %s.\n"), peer->sok, ContactFindName (peer->uin));
            else
                M_print (i18n (1843, "Closing socket %d.\n"), peer->sok);
        }
        sockclose (peer->sok);
    }
    peer->sok     = -1;
    peer->connect = (peer->connect & CONNECT_MASK && !(peer->connect & CONNECT_OK)) ? CONNECT_FAIL : 0;
    peer->our_session = 0;
    if (!peer->uin)
        SessionClose (peer);
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
    
    if (peer->type == TYPE_FILEDIRECT)
        SessionClose (peer);
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
void TCPPrint (Packet *pak, Session *peer, BOOL out)
{
    UWORD cmd;
    
    ASSERT_ANY_DIRECT(peer);
    
    pak->rpos = 0;
    cmd = *pak->data;

    Time_Stamp ();
    M_print (out ? " " COLINDENT COLCLIENT "" : " " COLINDENT COLSERVER "");
    M_print (out ? i18n (2078, "Outgoing TCP packet (%d - %s): %s")
                 : i18n (2079, "Incoming TCP packet (%d - %s): %s"),
             peer->sok, ContactFindName (peer->uin), TCPCmdName (cmd));
    M_print (COLNONE "\n");
    if (peer->connect & CONNECT_OK && peer->type == TYPE_MSGDIRECT && cmd == 2)
    {
        UWORD seq, typ;
        UDWORD sta, fla;
        char *msg;

        cmd = PacketRead1 (pak);
              PacketRead4 (pak);
        cmd = PacketRead2 (pak);
              PacketRead2 (pak);
        seq = PacketRead2 (pak);
              PacketRead4 (pak);
              PacketRead4 (pak);
              PacketRead4 (pak);
        typ = PacketRead2 (pak);
        sta = PacketRead2 (pak);
        fla = PacketRead2 (pak);
        msg = PacketReadLNTS (pak);
        M_print (i18n (2053, "TCP %s seq %x type %x status %x flags %x: '%s'\n"),
                 TCPCmdName (cmd), seq, typ, sta, fla, msg);
        free (msg);
        if (typ == TCP_MSG_GREETING)
        {
            UDWORD id1, id2, id3, id4, un1, un2, un3, un4;
            UWORD emp, port, pad, port2, len, flen;
            char *text, *reason, *name;

            cmd  = PacketRead2 (pak);
            id1  = PacketReadB4 (pak);
            id2  = PacketReadB4 (pak);
            id3  = PacketReadB4 (pak);
            id4  = PacketReadB4 (pak);
            emp  = PacketRead2 (pak);
            text = PacketReadDLStr (pak);
            un1  = PacketReadB4 (pak);
            un2  = PacketReadB4 (pak);
            un3  = PacketReadB4 (pak);
            un4  = PacketReadB2 (pak) << 8;
            un4 |= PacketRead1 (pak);
            len  = PacketRead4 (pak);
            len = len + pak->rpos - pak->len;
            reason=PacketReadDLStr (pak);
            port = PacketReadB2 (pak);
            pad  = PacketRead2 (pak);
            name = PacketReadLNTS (pak);
            flen = PacketRead4 (pak);
            port2= PacketRead4 (pak);
            M_print ("GREET %s (empty: %d) text '%s' lendiff %d reason '%s' port %d pad %x name '%s' flen %d port2 %d\n",
                     TCPCmdName (cmd), emp, text, len, reason, port, pad, name, flen, port2);
            M_print ("   ID %08x %08x %08x %08x\n", id1, id2, id3, id4);
            M_print ("  UNK %08x %08x %08x %06x\n", un1, un2, un3, un4);
            free (name);
            free (text);
            free (reason);
        }
        Hex_Dump (pak->data + pak->rpos, pak->len - pak->rpos);
        M_print ("---\n");
    }
    if (cmd != 6)
        if (prG->verbose & DEB_PACKTCPDATA)
            Hex_Dump (pak->data, pak->len);
    M_print (COLEXDENT "\r");
}

/*
 * Create and setup a TCP communication packet.
 */
static Packet *PacketTCPC (Session *peer, UDWORD cmd, UDWORD seq, UWORD type, UWORD flags, UWORD status, const char *msg)
{
    Packet *pak;
    
    ASSERT_ANY_DIRECT(peer);

    pak = PeerPacketC (peer, PEER_MSG);
    PacketWrite4      (pak, 0);          /* checksum - filled in later */
    PacketWrite2      (pak, cmd);        /* command                    */
    PacketWrite2      (pak, TCP_MSG_X1); /* unknown                    */
    PacketWrite2      (pak, seq);        /* sequence number            */
    PacketWrite4      (pak, 0);          /* unknown                    */
    PacketWrite4      (pak, 0);          /* unknown                    */
    PacketWrite4      (pak, 0);          /* unknown                    */
    PacketWrite2      (pak, type);       /* message type               */
    PacketWrite2      (pak, status);     /* flags                      */
    PacketWrite2      (pak, flags);      /* status                     */
    PacketWriteLNTS   (pak, msg);        /* the message                */
    
    return pak;
}

/*
 * Append the "geeting" part to a packet.
 */
void TCPGreet (Packet *pak, UWORD cmd, char *reason, UWORD port, UDWORD len, char *msg)
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
    PacketWriteDLStr (pak, reason);
    PacketWriteB2    (pak, port);
    PacketWriteB2    (pak, 0);
    PacketWriteLNTS  (pak, msg);
    PacketWrite4     (pak, len);
    if (cmd != 0x2d)
        PacketWrite4     (pak, port);
    PacketWriteLen4Done (pak);
}

/*
 * Acks a TCP packet.
 */
static int TCPSendMsgAck (Session *peer, UWORD seq, UWORD type, BOOL accept)
{
    Packet *pak;
    const char *msg;
    UWORD status, flags;
    Session *fpeer, *flist;

    ASSERT_MSGDIRECT (peer);

    switch (type)
    {
        case TCP_MSG_GET_AWAY:  msg = prG->auto_away; break;
        case TCP_MSG_GET_OCC:   msg = prG->auto_occ;  break;
        case TCP_MSG_GET_NA:    msg = prG->auto_na;   break;
        case TCP_MSG_GET_DND:   msg = prG->auto_dnd;  break;
        case TCP_MSG_GET_FFC:   msg = prG->auto_ffc;  break;
        case TCP_MSG_GET_VER:
            msg = BuildVersionText;
            break;
        default:
            if      (peer->parent->parent->status & STATUSF_DND)
                msg = prG->auto_dnd;
            else if (peer->parent->parent->status & STATUSF_OCC)
                msg = prG->auto_occ;
            else if (peer->parent->parent->status & STATUSF_NA)
                msg = prG->auto_na;
            else if (peer->parent->parent->status & STATUSF_AWAY)
                msg = prG->auto_away;
            else
                msg = "";
    }

    if (peer->parent->parent->status & STATUSF_DND)  status  = TCP_STAT_DND;   else
    if (peer->parent->parent->status & STATUSF_OCC)  status  = TCP_STAT_OCC;   else
    if (peer->parent->parent->status & STATUSF_NA)   status  = TCP_STAT_NA;    else
    if (peer->parent->parent->status & STATUSF_AWAY) status  = TCP_STAT_AWAY;
    else                                             status  = TCP_STAT_ONLINE;
    if (!accept)                                     status  = TCP_STAT_REFUSE;

    flags = 0;
    if (peer->parent->parent->status & STATUSF_INV)  flags |= TCP_MSGF_INV;
    flags ^= TCP_MSGF_LIST;

    pak = PacketTCPC (peer, TCP_CMD_ACK, seq, type, flags, status, msg);
    switch (type)
    {
        case TCP_MSG_FILE:
            flist = PeerFileCreate (peer->parent->parent);
            fpeer = SessionFind (TYPE_FILEDIRECT, peer->uin, flist);
            
            assert (flist && fpeer);
            
            PacketWriteB2   (pak, peer->port);   /* port */
            PacketWrite2    (pak, 0);            /* padding */
            PacketWriteStr  (pak, "");           /* file name - empty */
            PacketWrite4    (pak, 0);            /* file len - empty */
            if (peer->ver > 6)
                PacketWrite4 (pak, 0x20726f66);  /* unknown - strange - 'for ' */
            PacketWrite4    (pak, peer->port);   /* port again */
            break;
        case 0:
        case 1:
        case 4:
        case 6:
        case 7:
        case 8:
        default:
            PacketWrite4 (pak, TCP_COL_FG);      /* foreground color           */
            PacketWrite4 (pak, TCP_COL_BG);      /* background color           */
            break;
        case 0x1a:
            /* no idea */
    }
    PeerPacketSend (peer, pak);
    return 1;
}

/*
 * Acks a TCP packet.
 */
static int TCPSendGreetAck (Session *peer, UWORD seq, UWORD cmd, BOOL accept)
{
    Packet *pak;
    UWORD status, flags;
    Session *flist;

    ASSERT_MSGDIRECT (peer);

    if (peer->parent->parent->status & STATUSF_DND)  status  = TCP_STAT_DND;   else
    if (peer->parent->parent->status & STATUSF_OCC)  status  = TCP_STAT_OCC;   else
    if (peer->parent->parent->status & STATUSF_NA)   status  = TCP_STAT_NA;    else
    if (peer->parent->parent->status & STATUSF_AWAY) status  = TCP_STAT_AWAY;
    else                                             status  = TCP_STAT_ONLINE;
    if (!accept)                                     status  = TCP_STAT_REFUSE;

    flags = 0;
    if (peer->parent->parent->status & STATUSF_INV)  flags |= TCP_MSGF_INV;
    flags ^= TCP_MSGF_LIST;

    flist = PeerFileCreate (peer->parent->parent);
    if (!flist)
        status = TCP_STAT_REFUSE;

    pak = PacketTCPC (peer, TCP_CMD_ACK, seq, TCP_MSG_GREETING, flags, status, "");
    TCPGreet (pak, cmd, "", flist ? flist->port : 0, 0, "");
    PeerPacketSend (peer, pak);
    return 1;
}

/*
 * Requests the auto-response message from remote user.
 */
BOOL TCPGetAuto (Session *list, UDWORD uin, UWORD which)
{
    Contact *cont;
    Packet *pak;
    Session *peer;

    if (!list || !list->parent)
        return FALSE;
    if (uin == list->parent->uin)
        return FALSE;
    cont = ContactFind (uin);
    if (!cont)
        return FALSE;
    if (!(list->connect & CONNECT_MASK))
        return FALSE;
    if (!cont->port)
        return FALSE;
    if (!cont->local_ip && !cont->outside_ip)
        return FALSE;

    ASSERT_MSGLISTEN(list);
    
    peer = SessionFind (TYPE_MSGDIRECT, uin, list);
    if (peer)
    {
        if (peer->connect & CONNECT_FAIL)
            return FALSE;
    }
    else
    {
        if (!TCPDirectOpen (list, uin))
            return FALSE;
        peer = SessionFind (TYPE_MSGDIRECT, uin, list);
        if (!peer)
            return FALSE;
    }

    ASSERT_MSGDIRECT(peer);
    
    if (!which)
    {
        if (cont->status & STATUSF_DND)
            which = TCP_MSG_GET_DND;
        else if (cont->status & STATUSF_OCC)
            which = TCP_MSG_GET_OCC;
        else if (cont->status & STATUSF_NA)
            which = TCP_MSG_GET_NA;
        else if (cont->status & STATUSF_AWAY)
            which = TCP_MSG_GET_AWAY;
        else if (cont->status & STATUSF_FFC)
            which = TCP_MSG_GET_FFC;
        else
            return 0;
    }

    pak = PacketTCPC (peer, TCP_CMD_MESSAGE, peer->our_seq, which, 0, list->parent->status, "...");
    PacketWrite4 (pak, TCP_COL_FG);      /* foreground color           */
    PacketWrite4 (pak, TCP_COL_BG);      /* background color           */

    peer->stat_real_pak_sent++;

    QueueEnqueueData (peer, peer->our_seq--, QUEUE_TCP_RESEND,
                      uin, time (NULL),
                      pak, strdup ("..."), &TCPCallBackResend);
    return 1;
}

/*
 * Sends a message via TCP.
 * Adds it to the resend queue until acked.
 */
BOOL TCPSendMsg (Session *list, UDWORD uin, char *msg, UWORD sub_cmd)
{
    Contact *cont;
    Packet *pak;
    Session *peer;

    if (!list || !list->parent)
        return FALSE;
    if (uin == list->parent->uin)
        return FALSE;
    cont = ContactFind (uin);
    if (!cont)
        return FALSE;
    if (!(list->connect & CONNECT_MASK))
        return FALSE;
    if (!cont->port)
        return FALSE;
    if (!cont->local_ip && !cont->outside_ip)
        return FALSE;

    ASSERT_MSGLISTEN(list);
    
    peer = SessionFind (TYPE_MSGDIRECT, uin, list);
    if (peer)
    {
        if (peer->connect & CONNECT_FAIL)
            return FALSE;
    }
    else
    {
        if (!TCPDirectOpen (list, uin))
            return FALSE;
        peer = SessionFind (TYPE_MSGDIRECT, uin, list);
        if (!peer)
            return FALSE;
    }

    ASSERT_MSGDIRECT(peer);

    pak = PacketTCPC (peer, TCP_CMD_MESSAGE, peer->our_seq, sub_cmd, 0, list->parent->status, msg);
    PacketWrite4 (pak, TCP_COL_FG);      /* foreground color           */
    PacketWrite4 (pak, TCP_COL_BG);      /* background color           */

    peer->stat_real_pak_sent++;

    QueueEnqueueData (peer, peer->our_seq--, QUEUE_TCP_RESEND,
                      uin, time (NULL),
                      pak, strdup (msg), &TCPCallBackResend);
    return 1;
}

/*
 * Sends a message via TCP.
 * Adds it to the resend queue until acked.
 */
BOOL TCPSendFiles (Session *list, UDWORD uin, char *description, char **files, char **as, int count)
{
    Contact *cont;
    Packet *pak;
    Session *peer, *flist, *fpeer;
    int i, rc, sumlen = 0, sum = 0;

    if (!count)
        return TRUE;
    if (count < 0)
        return FALSE;
    
    if (!list || !list->parent)
        return FALSE;
    if (uin == list->parent->uin)
        return FALSE;
    cont = ContactFind (uin);
    if (!cont)
        return FALSE;
    if (!(list->connect & CONNECT_MASK))
        return FALSE;
    if (!cont->port)
        return FALSE;
    if (!cont->local_ip && !cont->outside_ip)
        return FALSE;

    ASSERT_MSGLISTEN(list);
    
    peer = SessionFind (TYPE_MSGDIRECT, uin, list);
    if (peer)
    {
        if (peer->connect & CONNECT_FAIL)
            return FALSE;
    }
    else
    {
        if (!TCPDirectOpen (list, uin))
            return FALSE;
        peer = SessionFind (TYPE_MSGDIRECT, uin, list);
        if (!peer)
            return FALSE;
    }

    ASSERT_MSGDIRECT(peer);
    
    flist = PeerFileCreate (peer->parent->parent);
    if (!flist)
        return 0;
    
    ASSERT_FILELISTEN(flist);

    fpeer = SessionClone (flist, TYPE_FILEDIRECT);
    
    assert (fpeer);
    
    fpeer->uin     = uin;
    fpeer->connect = 77;
        
    for (i = 0; i < count; i++)
    {
        struct stat fstat;
        
        if (stat (files[i], &fstat))
        {
            rc = errno;
            M_print (i18n (2071, "Couldn't stat file %s: %s (%d)\n"),
                     files[i], strerror (rc), rc);
        }
        else
        {
            M_print (i18n (2091, "Queueing %s as %s for transfer.\n"), files[i], as[i]);
            sum++;
            sumlen += fstat.st_size;
            pak = PeerPacketC (fpeer, 2);
            PacketWrite1 (pak, 0);
            PacketWriteLNTS (pak, as[i]);
            PacketWriteLNTS (pak, "");
            PacketWrite4 (pak, fstat.st_size);
            PacketWrite4 (pak, 0);
            PacketWrite4 (pak, 64);
            QueueEnqueueData (fpeer, sum, QUEUE_PEER_FILE,
                              uin, time (NULL), pak, strdup (files[i]), &PeerFileResend);
        }
    }
    
    if (!sum)
    {
        SessionClose (fpeer);
        return 0;
    }
    
    pak = PeerPacketC (fpeer, 0);
    PacketWrite4 (pak, 0);
    PacketWrite4 (pak, sum);
    PacketWrite4 (pak, sumlen);
    PacketWrite4 (pak, 64);
    PacketWriteLNTS (pak, "Sender's nick");
    QueueEnqueueData (fpeer, 0, QUEUE_PEER_FILE,
                      uin, time (NULL), pak, strdup (description), &PeerFileResend);
        
    if (peer->ver < 8)
    {
        pak = PacketTCPC (peer, TCP_CMD_MESSAGE, peer->our_seq, TCP_MSG_FILE, 0, list->parent->status, description);
        PacketWrite2 (pak, 0);
        PacketWrite2 (pak, 0);
        PacketWriteLNTS (pak, "many, really many, files");
        PacketWrite4 (pak, sumlen);
        PacketWrite4 (pak, 0);
    }
    else
    {
        pak = PacketTCPC (peer, TCP_CMD_MESSAGE, peer->our_seq, TCP_MSG_GREETING, 0, list->parent->status, "");
        TCPGreet (pak, 0x29, description, 0, 12345, "many, really many files");
    }

    peer->stat_real_pak_sent++;

    QueueEnqueueData (peer, peer->our_seq--, QUEUE_TCP_RESEND,
                      uin, time (NULL), pak, strdup (description), &TCPCallBackResend);
    return 1;
}

/*
 * Resends TCP packets if necessary
 */
static void TCPCallBackResend (Event *event)
{
    Contact *cont;
    Session *peer = event->sess;
    Packet *pak = event->pak;
    int delta;
    
    if (!peer)
    {
        M_print (i18n (2092, "TCP message %s discarded - lost session.\n"), event->info);
        PacketD (event->pak);
        free (event->info);
        free (event);
        return;
    }

    ASSERT_MSGDIRECT (peer);
    assert (pak);
    
    delta = (peer->ver > 6 ? 1 : 0);

    cont = ContactFind (event->uin);

    if (event->attempts >= MAX_RETRY_ATTEMPTS)
        TCPClose (peer);

    if (peer->connect & CONNECT_MASK)
    {
        if (peer->connect & CONNECT_OK)
        {
            if (event->attempts > 1)
            {
                Time_Stamp ();
                M_print (" " COLACK "%10s" COLNONE " %s%s\n", cont->nick, MSGTCPSENTSTR, event->info);
            }

            if ((event->attempts++) < 2)
                PeerPacketSend (peer, pak);
            event->due = time (NULL) + 10;
        }
        else
            event->due = time (NULL) + 1;
        QueueEnqueue (event);
        return;
    }

    peer->connect = CONNECT_FAIL;

    if (PacketReadAt2 (pak, 4 + delta) == TCP_CMD_MESSAGE)
        icq_sendmsg (peer->parent->parent, cont->uin, event->info, PacketReadAt2 (pak, 22 + delta));
    else
        M_print (i18n (1844, "TCP message %04x discarded after timeout.\n"), PacketReadAt2 (pak, 4 + delta));
    
    PacketD (event->pak);
    free (event->info);
    free (event);
}



/*
 * Handles a just received packet.
 */
static void TCPCallBackReceive (Event *event)
{
    Contact *cont;
    Packet *pak;
    char *tmp, *tmp3, *text, *reason, *name;
    UWORD cmd, type, seq, port;
    UDWORD len, status, flags;

    if (!event->sess)
    {
        PacketD (event->pak);
        free (event);
        return;
    }
    
    ASSERT_MSGDIRECT (event->sess);
    assert (event->pak);
    
    pak = event->pak;
    cont = (Contact *) event->info;
    
    cmd    = PacketRead2 (pak);
             PacketRead2 (pak);
    seq    = PacketRead2 (pak);
             PacketRead4 (pak);
             PacketRead4 (pak);
             PacketRead4 (pak);
    type   = PacketRead2 (pak);
    status = PacketRead2 (pak);
    flags  = PacketRead2 (pak);
    tmp    = PacketReadLNTS (pak);
    /* fore/background color ignored */
    
    switch (cmd)
    {
        case TCP_CMD_ACK:
            event = QueueDequeue (seq, QUEUE_TCP_RESEND);
            if (!event)
                break;
            switch (type)
            {
                case MSG_NORM:
                    Time_Stamp ();
                    M_print (" " COLACK "%10s" COLNONE " " MSGTCPACKSTR "%s\n",
                             cont->nick, event->info);
                    if (~cont->flags & CONT_SEENAUTO && strlen (tmp))
                    {
                        Do_Msg (event->sess, NOW, MSG_NORM, tmp, cont->uin, status);
                        cont->flags |= CONT_SEENAUTO;
                    }
                    break;

                case TCP_MSG_GET_AWAY:
                case TCP_MSG_GET_OCC:
                case TCP_MSG_GET_NA:
                case TCP_MSG_GET_DND:
                case TCP_MSG_GET_FFC:
                case TCP_MSG_GET_VER:
                    Do_Msg (event->sess, NOW, type, tmp, cont->uin, status);
                    break;

                case TCP_MSG_FILE:
                    port = PacketReadB2 (pak);
                    if (PeerFileAccept (event->sess, status, port))
                    {
                        M_print (i18n (2070, "File transfer '%s' to port %d.\n"), event->info, port);
                    }
                    else
                    {
                        M_print (i18n (2069, "File transfer '%s' rejected by peer (%x,%x): %s.\n"),
                                 event->info, status, flags, tmp);
                    }
                    break;

                case TCP_MSG_GREETING:
                    cmd    = PacketRead2 (pak); 
                             PacketReadB4 (pak);   /* ID */
                             PacketReadB4 (pak);
                             PacketReadB4 (pak);
                             PacketReadB4 (pak);
                             PacketRead2 (pak);    /* EMPTY */
                    text   = PacketReadDLStr (pak);
                             PacketReadB4 (pak);   /* UNKNOWN */
                             PacketReadB4 (pak);
                             PacketReadB4 (pak);
                             PacketReadB2 (pak);
                             PacketRead1 (pak);
                             PacketRead4 (pak);    /* LEN */
                    reason = PacketReadDLStr (pak);
                    port   = PacketReadB2 (pak);
                             PacketRead2 (pak);    /* PAD */
                    name   = PacketReadLNTS (pak);
                    len    = PacketRead4 (pak);
                             /* PORT2 ignored */
                    switch (cmd)
                    {
                        case 0x0029:
                            if (PeerFileAccept (event->sess, status, port))
                            {
                                M_print (i18n (2070, "File transfer '%s' to port %d.\n"), event->info, port);
                            }
                            else
                            {
                                M_print (i18n (2069, "File transfer '%s' rejected by peer (%x,%x): %s.\n"),
                                         event->info, status, flags, tmp);
                            }
                            break;
                            
                        default:
                            cmd = 0;
                    }
                    free (name);
                    free (text);
                    free (reason);
                    if (cmd)
                        break;

                    /* fall through */
                default:
                    Debug (DEB_TCP, "ACK %d uin %d nick %s pak %p peer %d seq %04x",
                                     event->sess->sok, event->sess->uin, ContactFindName (event->sess->uin), event->pak, event->sess, seq);
            }
            PacketD (event->pak);
            free (event);
            return;
        
        case TCP_CMD_MESSAGE:
            switch (type)
            {
                /* Requests for auto-response message */
                case TCP_MSG_GET_AWAY:
                case TCP_MSG_GET_OCC:
                case TCP_MSG_GET_NA:
                case TCP_MSG_GET_DND:
                case TCP_MSG_GET_FFC:
                    M_print (i18n (1814, "Sent auto-response message to %s%s%s.\n"),
                             COLCONTACT, cont->nick, COLNONE);
                case TCP_MSG_GET_VER:
                    TCPSendMsgAck (event->sess, seq, type, TRUE);
                    break;

                /* Automatically reject file xfer and chat requests
                     as these are not implemented yet. */ 
                case TCP_MSG_CHAT:
                    TCPSendMsgAck (event->sess, seq, type, FALSE);
                    break;

                case TCP_MSG_FILE:
                    cmd  = PacketRead4 (pak);
                    tmp3 = PacketReadLNTS (pak);
                    len  = PacketRead4 (pak);
                    type = PacketRead4 (pak);

                    if (PeerFileRequested (event->sess, tmp3, len))
                    {
                        M_print (i18n (2052, "Accepting file '%s' (%d bytes) from %s.\n"),
                                 tmp3, len, cont->nick);
                        TCPSendMsgAck (event->sess, seq, TCP_MSG_FILE, TRUE);
                    }
                    else
                    {
                        M_print (i18n (2061, "Refused file request '%s' (%d bytes) from %s (unknown: %x, %x)\n"),
                                 tmp3, len, cont->nick, cmd, type);
                        TCPSendMsgAck (event->sess, seq, TCP_MSG_FILE, FALSE);
                    }
                    free (tmp3);
                    break;
                case TCP_MSG_GREETING:
                    {
                        UWORD port, port2, flen, pad;
                        char *text, *reason, *name;
                        
                        cmd    = PacketRead2 (pak);
                                 PacketReadData (pak, NULL, 16);
                                 PacketRead2 (pak);
                        text   = PacketReadDLStr (pak);
                                 PacketReadData (pak, NULL, 15);
                                 PacketRead4 (pak);
                        reason = PacketReadDLStr (pak);
                        port   = PacketReadB2 (pak);
                        pad    = PacketRead2 (pak);
                        name   = PacketReadLNTS (pak);
                        flen   = PacketRead4 (pak);
                        port2  = PacketRead4 (pak);
                        
                        switch (cmd)
                        {
                            case 0x0029:
                                if (PeerFileRequested (event->sess, name, flen))
                                {
                                    M_print (i18n (2052, "Accepting file '%s' (%d bytes) from %s.\n"),
                                             name, flen, cont->nick);
                                    TCPSendGreetAck (event->sess, seq, cmd, TRUE);
                                }
                                else
                                {
                                    M_print (i18n (2061, "Refused file request '%s' (%d bytes) from %s (unknown: %x, %x)\n"),
                                             name, flen, cont->nick, cmd, type);
                                    TCPSendGreetAck (event->sess, seq, cmd, FALSE);
                                }
                                break;
                            case 0x0032:

                            case 0x002d:
                                M_print (i18n (2064, "Refusing chat request (%s/%s/%s) from %s.\n"),
                                         text, reason, name, cont->nick);
                                TCPSendGreetAck (event->sess, seq, cmd, FALSE);

                            default:
                                if (prG->verbose & DEB_PROTOCOL)
                                    M_print (i18n (2065, "Unknown TCP_MSG_GREET_ command %04x.\n"), type);
                                TCPSendGreetAck (event->sess, seq, cmd, FALSE);
                                break;
                            
                        }
                        free (name);
                        free (text);
                        free (reason);
                    }
                    break;
                default:
                    if (prG->verbose & DEB_PROTOCOL)
                        M_print (i18n (2066, "Unknown TCP_MSG_ command %04x.\n"), type);
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
                    Do_Msg (event->sess, NOW, type, tmp, cont->uin, status);

                    TCPSendMsgAck (event->sess, seq, type, TRUE);
                    break;
            }
            break;
        default:
            /* ignore */
    }
    PacketD (pak);
    free (tmp);
    free (event);
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
static void Encrypt_Pak (Session *peer, Packet *pak)
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

static int Decrypt_Pak (UBYTE *pak, UDWORD size)
{
    UDWORD hex, key, B1, M1, check;
    int i;
    UBYTE X1, X2, X3;

    /* Get checkcode */
    check = Chars_2_DW (pak);

    /* primary decryption */
    key = 0x67657268 * size + check;
 
    for (i = 4; i < (size + 3) / 4; ) 
    {
        hex = key + client_check_data[i & 0xff];
        pak[i++] ^=  hex        & 0xff;
        pak[i++] ^= (hex >>  8) & 0xff;
        pak[i++] ^= (hex >> 16) & 0xff;
        pak[i++] ^= (hex >> 24) & 0xff;
    }

    B1 = (pak[4]<<24) | (pak[6]<<16) | (pak[4]<<8) | (pak[6]<<0);

    /* special decryption */
    B1 ^= check;

    /* validate packet */
    M1 = (B1 >> 24) & 0xFF;
    if (M1 < 10 || M1 >= size)
        return (-1);

    X1 = pak[M1] ^ 0xFF;
    if (((B1 >> 16) & 0xFF) != X1) 
        return (-1);

    X2 = ((B1 >> 8) & 0xFF);
    if (X2 < 220)
    {
        X3 = client_check_data[X2] ^ 0xFF;
        if((B1 & 0xFF) != X3)
            return (-1);
    }

    return (1);
}
#endif /* ENABLE_PEER2PEER */
