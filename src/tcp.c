/*
 * This file handles TCP client-to-client communications.
 *
 *  Author/Copyright: James Schofield (jschofield@ottawa.com) 22 Feb 2001
 *  Lots of changes from Rüdiger Kuhlmann.
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

#ifdef TCP_COMM

static void       TCPDispatchReconn  (Session *sess);
static void       TCPDispatchMain    (Session *sess);
static void       TCPDispatchConn    (Session *sess);
static void       TCPDispatchShake   (Session *sess);
static void       TCPDispatchPeer    (Session *sess);

static Packet    *TCPReceivePacket   (Session *sess);
static void       TCPSendPacket      (Packet *pak, Session *sess);
static void       TCPClose           (Session *sess);

static void       TCPSendInit        (Session *sess);
static void       TCPSendInitAck     (Session *sess);
static void       TCPSendInit2       (Session *sess);
static Session   *TCPReceiveInit     (Session *sess, Packet *pak);
static void       TCPReceiveInitAck  (Session *sess, Packet *pak);
static Session   *TCPReceiveInit2    (Session *sess, Packet *pak);

static void       TCPPrint           (Packet *pak, Session *sess, BOOL out);

static Packet    *PacketTCPC         (Session *peer, UDWORD cmd, UDWORD seq,
                                      UWORD type, UWORD flags, UWORD status, char *msg);
static void       TCPGreet           (Packet *pak, UWORD cmd, char *reason,
                                      UWORD port, UDWORD len, char *msg);

static void       TCPCallBackTimeout (Event *event);
static void       TCPCallBackTOConn  (Event *event);
static void       TCPCallBackResend  (Event *event);
static void       TCPCallBackReceive (Event *event);

static void       Encrypt_Pak        (Session *sess, Packet *pak);
static int        Decrypt_Pak        (UBYTE *pak, UDWORD size);
static int        TCPSendMsgAck      (Session *sess, UWORD seq, UWORD sub_cmd, BOOL accept);

#ifdef WIP
static void PeerFileDispatch (Session *sess);
static void PeerFileResend (Event *event);
#endif

/*********************************************/

/*
 * "Logs in" TCP connection by opening listening socket.
 */
void SessionInitPeer (Session *sess)
{
    int port;
    
    assert (sess);
    
    if (sess->ver < 6 || sess->ver > 8)
    {
        M_print (i18n (2024, "Unknown protocol version %d for ICQ peer-to-peer protocol.\n"), sess->ver);
        return;
    }

    if (sess->ver == 6)
        M_print (i18n (2046, "You may want to use protocol version 8 for the ICQ peer-to-peer protocol instead.\n"));

    port = sess->port;
    M_print (i18n (1777, "Opening peer-to-peer connection at localhost:%d... "), port);

    sess->connect     = 0;
    sess->our_seq     = -1;
    if (sess->type != TYPE_FILE)
        sess->type        = TYPE_LISTEN;
    sess->flags       = 0;
    sess->dispatch    = &TCPDispatchMain;
    sess->reconnect   = &TCPDispatchReconn;
    sess->our_session = 0;

    UtilIOConnectTCP (sess);
}

/*
 *  Starts establishing a TCP connection to given contact.
 */
void TCPDirectOpen (Session *sess, UDWORD uin)
{
    Session *peer;
    Contact *cont;
    
    ASSERT_LISTEN (sess);
    
    if (uin == sess->assoc->uin)
        return;

    UtilCheckUIN (sess->assoc, uin);
    cont = ContactFind (uin);
    if (!cont || cont->TCP_version < 6)
        return;

    if ((peer = SessionFind (TYPE_DIRECT, uin)))
    {
        if (peer->connect & CONNECT_MASK)
            return;
    }
    else
        peer = SessionClone (sess);
    
    if (!peer)
        return;
    
    peer->port  = 0;
    peer->uin   = uin;
    peer->type  = TYPE_DIRECT;
    peer->flags = 0;
    peer->spref = NULL;
    peer->assoc = sess;
    peer->ver   = sess->ver <= cont->TCP_version ? sess->ver : cont->TCP_version;

    TCPDispatchConn (peer);
}

/*
 * Closes TCP message/file connection(s) to given contact.
 */
void TCPDirectClose (UDWORD uin)
{
    Session *peer;
    
    if ((peer = SessionFind (TYPE_DIRECT, uin)))
        TCPClose (peer);
    if ((peer = SessionFind (TYPE_FILE, uin)))
        TCPClose (peer);
}

/*
 * Switchs off TCP for a given uin.
 */
void TCPDirectOff (UDWORD uin)
{
    Contact *cont;
    Session *peer;
    
    peer = SessionFind (TYPE_DIRECT, uin);
    cont = ContactFind (uin);
    
    if (!peer && cont)
        peer = SessionC ();

    if (!peer)
        return;

    peer->uin     = cont->uin;
    peer->connect = CONNECT_FAIL;
    peer->type    = TYPE_DIRECT;
    peer->flags   = 0;
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
static void TCPDispatchReconn (Session *sess)
{
    if (prG->verbose)
    {
        Time_Stamp ();
        M_print (" %s%10s%s ", COLCONTACT, ContactFindName (sess->uin), COLNONE);
        M_print (i18n (2023, "Direct connection closed by peer.\n"));
    }
    TCPClose (sess);
}

/*
 * Accepts a new direct connection.
 */
static void TCPDispatchMain (Session *sess)
{
    struct sockaddr_in sin;
    Session *peer;
    int tmp, rc;
 
    assert (sess);
    assert (sess->type == TYPE_LISTEN || sess->type == TYPE_FILE);

    if (sess->connect & CONNECT_OK)
    {
        if ((rc = UtilIOError (sess)))
        {
            M_print (i18n (2051, "Error on socket: %s (%d)\n"), strerror (rc), rc);
            if (sess->sok > 0)
                sockclose (sess->sok);
            sess->sok = -1;
            sess->connect = 0;
            return;
        }
    }
    else
    {
        switch (sess->connect & 3)
        {
            case 1:
                sess->connect |= CONNECT_OK | CONNECT_SELECT_R | CONNECT_SELECT_X;
                sess->connect &= ~CONNECT_SELECT_W; /* & ~CONNECT_SELECT_X; */
                if (sess->assoc && sess->assoc->ver > 6 &&
                    (sess->assoc->connect & CONNECT_OK))
                    SnacCliSetstatus (sess->assoc, 0, 2);
                break;
            case 2:
                sess->connect = 0;
                break;
            default:
                assert (0);
        }
        return;
    }

    tmp = sizeof (sin);
    peer = SessionClone (sess);
    
    if (!peer)
    {
        M_print (i18n (1914, "Can't allocate connection structure.\n"));
        sess->connect = 0;
        if (sess->sok)
            sockclose (sess->sok);
        return;
    }
    
#ifdef WIP
    M_print ("New incoming\n");
#endif

    peer->type  = TYPE_DIRECT;
    peer->flags = 0;
    peer->spref = NULL;
    peer->our_session = 0;
    peer->dispatch    = &TCPDispatchShake;

    if (prG->s5Use)
    {
        peer->sok = sess->sok;
        sess->sok = -1;
        sess->connect = 0;
        UtilIOSocksAccept (peer);
        UtilIOConnectTCP (sess);
    }
    else
    {
        peer->sok  = accept (sess->sok, (struct sockaddr *)&sin, &tmp);
        
        if (peer->sok <= 0)
        {
            peer->connect = 0;
            peer->sok     = -1;
            return;
        }
    }

    peer->connect = 16 | CONNECT_SELECT_R;
    peer->uin     = 0;
}

/*
 * Continues connecting.
 */
static void TCPDispatchConn (Session *sess)
{
    Contact *cont;
    
    ASSERT_DIRECT_FILE (sess);
    
    sess->dispatch = &TCPDispatchConn;

    while (1)
    {
        cont = ContactFind (sess->uin);
        if (!cont)
        {
            TCPClose (sess);
            return;
        }
        if (prG->verbose & DEB_TCP)
            M_print ("debug: TCPDispatchConn; Nick: %s state: %x\n", ContactFindName (sess->uin), sess->connect);

        switch (sess->connect & CONNECT_MASK)
        {
            case 0:
                if (!cont->outside_ip)
                {
                    sess->connect = 3;
                    break;
                }
                sess->server  = NULL;
                if (sess->type == TYPE_DIRECT)
                {
                    sess->ip      = cont->outside_ip;
                    sess->port    = cont->port;
                }
                sess->connect = 1;
                
                if (prG->verbose)
                {
                    Time_Stamp ();
                    M_print (" %s%10s%s ", COLCONTACT, ContactFindName (sess->uin), COLNONE);
                    M_print (i18n (2034, "Opening TCP connection at %s:%d... "),
                              UtilIOIP (sess->ip), sess->port);
                }
                UtilIOConnectTCP (sess);
                return;
            case 3:
#ifdef WIP
                if (sess->type == TYPE_FILE)
                {
                    sockclose (sess->sok);
                    sess->sok = -1;
                    sess->connect = 0;
                    return;
                }
#endif
                if (!cont->local_ip || !cont->port)
                {
                    sess->connect = CONNECT_FAIL;
                    return;
                }
                sess->server  = NULL;
                sess->ip      = cont->local_ip;
                sess->port    = cont->port;
                sess->connect = 3;
                
                if (prG->verbose)
                {
                    Time_Stamp ();
                    M_print (" %s%10s%s ", COLCONTACT, ContactFindName (sess->uin), COLNONE);
                    M_print (i18n (2034, "Opening TCP connection at %s:%d... "),
                             UtilIOIP (sess->ip), sess->port);
                }
                UtilIOConnectTCP (sess);
                return;
            case 5:
            {
                if (sess->assoc && sess->assoc->assoc && sess->assoc->assoc->ver < 7)
                {
                    CmdPktCmdTCPRequest (sess->assoc->assoc, cont->uin, cont->port);
                    QueueEnqueueData (queue, sess, sess->ip, QUEUE_TYPE_TCP_TIMEOUT,
                                      cont->uin, time (NULL) + 30,
                                      NULL, NULL, &TCPCallBackTOConn);
                    sess->connect = TCP_STATE_WAITING;
                }
                else
                    sess->connect = CONNECT_FAIL;
                sockclose (sess->sok);
                sess->sok = -1;
                return;
            }
            case 2:
            case 4:
                if (prG->verbose)
                {
                    Time_Stamp ();
                    M_print (" %s%10s%s ", COLCONTACT, ContactFindName (cont->uin), COLNONE);
                    M_print (i18n (2034, "Opening TCP connection at %s:%d... "),
                             UtilIOIP (sess->ip), sess->port);
                    M_print (i18n (1785, "success.\n"));
                }
                QueueEnqueueData (queue, sess, sess->ip, QUEUE_TYPE_TCP_TIMEOUT,
                                  cont->uin, time (NULL) + 10,
                                  NULL, NULL, &TCPCallBackTimeout);
                sess->connect = 1 | CONNECT_SELECT_R;
                sess->dispatch = &TCPDispatchShake;
                TCPDispatchShake (sess);
                return;
            case TCP_STATE_WAITING:
                if (prG->verbose)
                    M_print (i18n (1855, "TCP connection to %s at %s:%d failed.\n") , cont->nick, UtilIOIP (sess->ip), sess->port);
                sess->connect = -1;
                sess->sok = -1;
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
static void TCPDispatchShake (Session *sess)
{
    Contact *cont;
    Packet *pak = NULL;
    
    ASSERT_DIRECT_FILE (sess);
    
    if ((sess->connect & CONNECT_MASK) != 1)
    {
        pak = TCPReceivePacket (sess);
        if (!pak)
            return;
    }

    while (1)
    {
        if (!sess)
            return;

        if (prG->verbose & DEB_TCP)
        {
            Time_Stamp ();
            M_print (" [%d %p %p] %s State: %d\n", sess->sok, pak, sess, ContactFindName (sess->uin), sess->connect);
        }

        cont = ContactFind (sess->uin);
        if (!cont && (sess->connect & CONNECT_MASK) != 16)
        {
            TCPClose (sess);
            sess->connect = CONNECT_FAIL;
            if (pak)
                PacketD (pak);
            return;
        }
        
        switch (sess->connect & CONNECT_MASK)
        {
            case 1:
                sess->connect++;
                TCPSendInit (sess);
                return;
            case 2:
                sess->connect++;
                TCPReceiveInitAck (sess, pak);
                PacketD (pak);
                return;
            case 3:
                sess->connect++;
                sess = TCPReceiveInit (sess, pak);
                PacketD (pak);
                pak = NULL;
                continue;
            case 4:
                sess->connect++;
                TCPSendInitAck (sess);
                continue;
            case 5:
                sess->connect++;
                TCPSendInit2 (sess);
                if (sess->ver > 6)
                    return;
                continue;
            case 6:
                sess->connect = 7 | CONNECT_SELECT_R;
                if (sess->ver > 6)
                    sess = TCPReceiveInit2 (sess, pak);
                continue;
            case 7:
                sess->connect = 48 | CONNECT_SELECT_R;
                continue;
            case 16:
                sess->connect++;
                sess = TCPReceiveInit (sess, pak);
                PacketD (pak);
                pak = NULL;
                continue;
            case 17:
                sess->connect++;
                TCPSendInitAck (sess);
                continue;
            case 18:
                sess->connect++;
                TCPSendInit (sess);
                return;
            case 19:
                sess->connect++;
                TCPReceiveInitAck (sess, pak);
                PacketD (pak);
                if (sess->ver > 6)
                    return;
                pak = NULL;
                continue;
            case 20:
                sess->connect++;
                if (sess->ver > 6)
                {
                    sess = TCPReceiveInit2 (sess, pak);
                    PacketD (pak);
                    pak = NULL;
                }
                continue;
            case 21:
                sess->connect = 48 | CONNECT_SELECT_R;
                TCPSendInit2 (sess);
                continue;
            case 48:
                QueueDequeue (queue, sess->ip, QUEUE_TYPE_TCP_TIMEOUT);
                if (prG->verbose)
                {
                    Time_Stamp ();
                    M_print (" %s%10s%s ", COLCONTACT, ContactFindName (sess->uin), COLNONE);
                    M_print (i18n (1833, "Peer to peer TCP connection established.\n"), cont->nick);
                }
                sess->connect = CONNECT_OK | CONNECT_SELECT_R;
#ifdef WIP
                if (sess->type == TYPE_FILE)
                    sess->dispatch = &PeerFileDispatch;
                else
#endif
                    sess->dispatch = &TCPDispatchPeer;
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
static void TCPDispatchPeer (Session *sess)
{
    Contact *cont;
    Packet *pak;
    Event *event;
    int i = 0;
    UWORD seq_in = 0, seq, cmd;
    
    ASSERT_DIRECT (sess);
    
    cont = ContactFind (sess->uin);
    if (!cont)
    {
        TCPClose (sess);
        return;
    }

    /* Recv all packets before doing anything else.
         The objective is to delete any packets CANCELLED by the remote user. */
    while (M_Is_Set (sess->sok) && i++ <= TCP_MSG_QUEUE)
    {
        if (!(pak = TCPReceivePacket (sess)))
            return;

        if (sess->ver > 6)
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
                    event = QueueDequeue (queue, seq_in, QUEUE_TYPE_TCP_RECEIVE);
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
                    QueueEnqueueData (queue, sess, seq_in, QUEUE_TYPE_TCP_RECEIVE,
                                      0, 0, pak, (UBYTE *)cont, &TCPCallBackReceive);

                    sess->our_seq--;
                break;
            }
        }

        M_select_init();
        M_set_timeout (0, 100000);
        M_Add_rsocket (sess->sok);
        M_select();
    }
}

/*********************************************/

/*
 * Handles timeout on TCP send/read/whatever
 */
static void TCPCallBackTimeout (Event *event)
{
    Session *sess = event->sess;
    
    ASSERT_DIRECT_FILE (event->sess);
    assert (event->type == QUEUE_TYPE_TCP_TIMEOUT);
    
    if ((sess->connect & CONNECT_MASK) && prG->verbose)
    {
        M_print (i18n (1850, "Timeout on connection with %s at %s:%d\n"),
                 ContactFindName (sess->uin), UtilIOIP (sess->ip), sess->port);
        TCPClose (sess);
    }
    free (event);
}

/*
 * Handles timeout on TCP connect
 */
static void TCPCallBackTOConn (Event *event)
{
    ASSERT_DIRECT_FILE (event->sess);

    event->sess->connect += 2;
    TCPDispatchConn (event->sess);
    free (event);
}

/*
 *  Receives an incoming TCP packet.
 *  Resets socket on error. Paket must be freed.
 */
static Packet *TCPReceivePacket (Session *sess)
{
    Packet *pak;

    ASSERT_DIRECT_FILE (sess);

    if (!(sess->connect & CONNECT_MASK))
        return NULL;

    pak = UtilIOReceiveTCP (sess);

    if (!sess->connect)
    {
        TCPClose (sess);
        return NULL;
    }
    
    if (!pak)
        return NULL;

    sess->stat_pak_rcvd++;
    sess->stat_real_pak_rcvd++;

    if (sess->connect & CONNECT_OK && sess->type == TYPE_DIRECT)
    {
        if (Decrypt_Pak ((UBYTE *) &pak->data + (sess->ver > 6 ? 1 : 0), pak->len - (sess->ver > 6 ? 1 : 0)) < 0)
        {
            if (prG->verbose & DEB_TCP)
            {
                Time_Stamp ();
                M_print (" \x1b«" COLSERV "");
                M_print (i18n (1789, "Received malformed packet: (%d)"), sess->sok);
                M_print (COLNONE "\n");
                Hex_Dump (pak->data, pak->len);
                M_print (ESC "»\r");

            }
            TCPClose (sess);
            PacketD (pak);
            return NULL;
        }
    }

    if (prG->verbose & DEB_PACKTCP)
        TCPPrint (pak, sess, FALSE);

    pak->rpos = 0;
    pak->wpos = 0;
    
    return pak;
}

/*
 * Encrypts and sends a TCP packet.
 * Resets socket on error.
 */
static void TCPSendPacket (Packet *pak, Session *sess)
{
    Packet *tpak;
    
    ASSERT_DIRECT_FILE (sess);
    
    sess->stat_pak_sent++;
    
    if (!(sess->connect & CONNECT_MASK))
        return;

    if (prG->verbose & DEB_PACKTCP)
        TCPPrint (pak, sess, TRUE);

    tpak = PacketClone (pak);
    if (sess->type == TYPE_DIRECT)
        if (PacketReadAt1 (tpak, 0) == PEER_MSG || !PacketReadAt1 (tpak, 0)) 
            Encrypt_Pak (sess, tpak);
    
    if (!UtilIOSendTCP (sess, tpak))
        TCPClose (sess);
}

/*
 * Sends a TCP initialization packet.
 */
void TCPSendInitv6 (Session *sess)
{
    Packet *pak;
    
    ASSERT_DIRECT_FILE (sess);

    if (!(sess->connect & CONNECT_MASK))
        return;

    if (!(sess->our_session))
        sess->our_session = rand ();

    sess->stat_real_pak_sent++;

    if (prG->verbose & DEB_TCP)
    {
        Time_Stamp ();
        M_print (" [%d %p] %s TCP>>CONNECT\n", sess->sok, sess, ContactFindName (sess->uin));
/*        , i18n (1836, "Sending TCP direct connection initialization packet.\n")); */
    }

    pak = PacketC ();
    PacketWrite1 (pak, PEER_INIT);                    /* command          */
    PacketWrite2 (pak, 6);                            /* TCP version      */
    PacketWrite2 (pak, 0);                            /* TCP revision     */
    PacketWrite4 (pak, sess->uin);                    /* destination UIN  */
    PacketWrite2 (pak, 0);                            /* unknown - zero   */
    PacketWrite4 (pak, sess->assoc->port);            /* our port         */
    PacketWrite4 (pak, sess->assoc->assoc->uin);             /* our UIN          */
    PacketWriteB4 (pak, sess->assoc->assoc->our_outside_ip);  /* our (remote) IP  */
    PacketWriteB4 (pak, sess->assoc->assoc->our_local_ip);    /* our (local)  IP  */
    PacketWrite1 (pak, TCP_OK_FLAG);                  /* connection type  */
    PacketWrite4 (pak, sess->assoc->port);            /* our (other) port */
    PacketWrite4 (pak, sess->our_session);            /* session id       */

    TCPSendPacket (pak, sess);
}

/*
 * Sends a v7/v8 TCP initialization packet.
 */
static void TCPSendInit (Session *sess)
{
    Packet *pak;
    
    ASSERT_DIRECT_FILE (sess);

    if (sess->ver == 6)
    {
        TCPSendInitv6 (sess);
        return;
    }

    if (!(sess->connect & CONNECT_MASK))
        return;

    if (!sess->our_session)
    {
        Contact *cont;

        cont = ContactFind (sess->uin);
        if (!cont)
        {
            TCPClose (sess);
            return;
        }
        sess->our_session = cont->cookie;
    }

    sess->stat_real_pak_sent++;

    if (prG->verbose & DEB_TCP)
    {
        Time_Stamp ();
        M_print (" [%d %p] %s TCP>>CONNECTv8\n", sess->sok, sess, ContactFindName (sess->uin));
/*        M_print (" %10d [%p] %s", sess->uin, sess, i18n (2025, "Sending v8 TCP direct connection initialization packet.\n"));*/
    }

    pak = PacketC ();
    PacketWrite1 (pak, PEER_INIT);                    /* command          */
    PacketWrite2 (pak, sess->assoc->ver);             /* TCP version      */
    PacketWrite2 (pak, 43);                           /* length           */
    PacketWrite4 (pak, sess->uin);                    /* destination UIN  */
    PacketWrite2 (pak, 0);                            /* unknown - zero   */
    PacketWrite4 (pak, sess->assoc->port);            /* our port         */
    PacketWrite4 (pak, sess->assoc->assoc->uin);             /* our UIN          */
    PacketWriteB4 (pak, sess->assoc->assoc->our_outside_ip);  /* our (remote) IP  */
    PacketWriteB4 (pak, sess->assoc->assoc->our_local_ip);    /* our (local)  IP  */
    PacketWrite1 (pak, TCP_OK_FLAG);                  /* connection type  */
    PacketWrite4 (pak, sess->assoc->port);            /* our (other) port */
    PacketWrite4 (pak, sess->our_session);            /* session id       */
    PacketWrite4 (pak, 0x00000050);
    PacketWrite4 (pak, 0x00000003);
    PacketWrite4 (pak, 0);

    TCPSendPacket (pak, sess);
}

/*
 * Sends the initialization packet
 */
static void TCPSendInitAck (Session *sess)
{
    Packet *pak;
    
    ASSERT_DIRECT_FILE (sess);

    if (!(sess->connect & CONNECT_MASK))
        return;

    if (prG->verbose & DEB_TCP)
    {
        Time_Stamp ();
        M_print (" [%d %p] %s TCP>>ACK\n", sess->sok, sess, ContactFindName (sess->uin));
/*        M_print (" %10d [%p] %s", sess->uin, sess, i18n (1837, "Acknowledging TCP direct connection initialization packet.\n"));*/
    }

    sess->stat_real_pak_sent++;

    pak = PacketC ();
    PacketWrite2 (pak, PEER_INITACK);
    PacketWrite2 (pak, 0);
    TCPSendPacket (pak, sess);
}

static void TCPSendInit2 (Session *sess)
{
    Packet *pak;
    
    ASSERT_DIRECT_FILE (sess);
    
    if (sess->ver == 6)
        return;
    
    if (!(sess->connect & CONNECT_MASK))
        return;
    
    if (prG->verbose & DEB_TCP)
    {
        Time_Stamp ();
        M_print (" [%d %p] %s <%x> TCP>>CONNECT2\n", sess->sok, sess, ContactFindName (sess->uin), sess->connect);
/*        M_print (" %10d [%p] %s", sess->uin, sess, i18n (2027, "Sending third TCP direct connection packet.\n"));*/
    }

    sess->stat_real_pak_sent++;
    
    pak = PacketC ();
    PacketWrite1 (pak, PEER_INIT2);
    PacketWrite4 (pak, 10);
    PacketWrite4 (pak, 1);
    PacketWrite4 (pak, (sess->connect & 16) ? 1 : 0);
    PacketWrite4 (pak, 0);
    PacketWrite4 (pak, 0);
    PacketWrite4 (pak, (sess->connect & 16) ? 0x40001 : 0);
    PacketWrite4 (pak, 0);
    PacketWrite4 (pak, (sess->connect & 16) ? 0 : 0x40001);
    TCPSendPacket (pak, sess);
}

#define FAIL(x) { err = x; break; }

static Session *TCPReceiveInit (Session *sess, Packet *pak)
{
    Contact *cont;
    UDWORD muin, uin, sid, port, port2, oip, iip;
    UWORD  cmd, len, len2, tcpflag, err, nver;
    Session *peer;

    ASSERT_DIRECT_FILE (sess);
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
        
        /* check validity of this conenction; fail silently */
        
        if (cmd != PEER_INIT)
            FAIL (1);
        
        if (nver < 6)
            FAIL (2);
        
        if (nver > 6 && len != len2)
            FAIL (3);
        
        if (nver == 6 && 23 > len2)
            FAIL (4);

        if (muin  != sess->assoc->assoc->uin)
            FAIL (5);
        
        if (uin  == sess->assoc->assoc->uin)
            FAIL (6);
        
        if (!(cont = ContactFind (uin)))
            FAIL (7);

        sess->ver = (sess->ver > nver ? nver : sess->ver);

        if (!sess->our_session)
            sess->our_session = sess->ver > 6 ? cont->cookie : sid;
        if (sid  != sess->our_session)
            FAIL (8);

        if (cont->flags & CONT_IGNORE)
            FAIL (9);

        /* okay, the connection seems not to be faked, so update using the following information. */

        sess->uin = uin;
        if (port2)    cont->port = port2;
        if (port)     cont->port = port;
        if (oip)      cont->outside_ip = oip;
        if (iip)      cont->local_ip = iip;
        if (tcpflag)  cont->connection_type = tcpflag;

        if (prG->verbose & DEB_TCP)
        {
            Time_Stamp ();
            M_print (" %d [%p] ", uin, sess);
            M_print (i18n (1838, "Received direct connection initialization.\n"));
            M_print ("    \x1b«");
            M_print (i18n (1839, "Version %04x:%04x, Port %d, UIN %d, session %08x\n"),
                     sess->ver, len, port, uin, sid);
            M_print ("\x1b»\n");
        }

        if ((peer = SessionFind (sess->type, uin)) && peer != sess)
        {
            if (peer->connect & CONNECT_OK)
            {
                TCPClose (sess);
                sess->connect = CONNECT_FAIL;
                return NULL;
            }
            if ((peer->connect & CONNECT_MASK) == (UDWORD)TCP_STATE_WAITING)
                QueueDequeue (queue, peer->ip, QUEUE_TYPE_TCP_TIMEOUT);
            if (peer->sok != -1)
                TCPClose (peer);
            SessionClose (peer);
        }
        return sess;
    }
    if ((prG->verbose & DEB_TCP) && err)
    {
        Time_Stamp ();
        M_print (" %s: %d\n", i18n (2029, "Protocol error on peer-to-peer connection"), err);
    }
    TCPClose (sess);
    return NULL;
}

/*
 * Receives the acknowledge packet for the initialization packet.
 */
static void TCPReceiveInitAck (Session *sess, Packet *pak)
{
    ASSERT_DIRECT_FILE (sess);
    assert (pak);
    
    if (pak->len != 4 || PacketReadAt4 (pak, 0) != PEER_INITACK)
    {
        Debug (DEB_TCP | DEB_PROTOCOL, i18n (1841, "Received malformed initialization acknowledgement packet.\n"));
        TCPClose (sess);
    }
}

static Session *TCPReceiveInit2 (Session *sess, Packet *pak)
{
    UWORD  cmd, err;
    UDWORD ten, one;

    ASSERT_DIRECT_FILE (sess);
    assert (pak);

    if (!pak)
        return sess;

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
        
        return sess;
    }

    if (err && (prG->verbose & (DEB_TCP | DEB_PROTOCOL)))
    {
        Time_Stamp ();
        M_print (" %s: %d\n", i18n (2029, "Protocol error on peer-to-peer connection"), err);
    }
    else
        sess->connect = 0;
    TCPClose (sess);
    return NULL;
}


/*
 * Close socket and mark as inactive. If verbose, complain.
 */
static void TCPClose (Session *sess)
{
    ASSERT_DIRECT_FILE (sess);
    
    if (sess->sok != -1)
    {
        if (sess->connect & CONNECT_MASK && prG->verbose)
        {
            Time_Stamp ();
            M_print (" ");
            if (sess->uin)
                M_print (i18n (1842, "Closing socket %d to %s.\n"), sess->sok, ContactFindName (sess->uin));
            else
                M_print (i18n (1843, "Closing socket %d.\n"), sess->sok);
        }
        sockclose (sess->sok);
    }
    sess->sok     = -1;
    sess->connect = (sess->connect & CONNECT_MASK && !(sess->connect & CONNECT_OK)) ? CONNECT_FAIL : 0;
    sess->our_session = 0;
    /* TODO: is this robust? */
    if (!sess->uin)
        SessionClose (sess);
    if (sess->incoming)
    {
        PacketD (sess->incoming);
        sess->incoming = NULL;
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
static void TCPPrint (Packet *pak, Session *peer, BOOL out)
{
    UWORD cmd, done;
    
    pak->rpos = 0;
    if (peer->ver > 6)
        cmd = PacketRead1 (pak);
    else
        cmd = *pak->data;
    done = pak->rpos;

    Time_Stamp ();
    M_print (out ? " \x1b«" COLCLIENT "" : " \x1b«" COLSERV "");
    M_print (out ? i18n (1776, "Outgoing TCP packet (%d - %s): %s")
                 : i18n (1778, "Incoming TCP packet (%d - %s): %s"),
             peer->sok, ContactFindName (peer->uin), TCPCmdName (cmd));
    M_print (COLNONE "\n");
#ifdef WIP
    if (cmd == 2 || (peer->connect & CONNECT_OK && peer->type == TYPE_DIRECT))
    {
        UWORD seq, typ;
        UDWORD sta, fla;
        const char *msg;

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
        msg = PacketReadLNTSC (pak);
        M_print (i18n (2053, "TCP %s seq %x type %x status %x flags %x: '%s'\n"),
                 TCPCmdName (cmd), seq, typ, sta, fla, msg);
        if (typ == TCP_MSG_GREETING)
        {
            UDWORD id1, id2, id3, id4, un1, un2, un3, un4;
            UWORD emp, port, pad, port2, len, flen;
            const char *text, *reason, *name;
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
            name = PacketReadLNTSC (pak);
            flen = PacketRead4 (pak);
            port2= PacketRead4 (pak);
            M_print ("GREET %s (empty: %d) text '%s' len %d reason '%s' port %d pad %x name '%s' flen %d port2 %d\n",
                     TCPCmdName (cmd), emp, text, len, reason, port, pad, name, flen, port2);
            M_print ("   ID %08x %08x %08x %08x\n", id1, id2, id3, id4);
            M_print ("  UNK %08x %08x %08x %06x\n", un1, un2, un3, un4);
        }
        Hex_Dump (pak->data + pak->rpos, pak->len - pak->rpos);
        if (prG->verbose & DEB_PACKTCPDATA)
            Hex_Dump (pak->data + done, pak->len - done);
    }
    else
#endif
        if (prG->verbose & DEB_PACKTCPDATA)
            Hex_Dump (pak->data, pak->len);
    M_print (ESC "»\r");
}

/*
 * Create and setup a TCP communication packet.
 */
static Packet *PacketTCPC (Session *peer, UDWORD cmd, UDWORD seq, UWORD type, UWORD flags, UWORD status, char *msg)
{
    Packet *pak;

    pak = PacketC ();
    if (peer->ver > 6)
        PacketWrite1 (pak, PEER_MSG);
    PacketWrite4     (pak, 0);          /* checksum - filled in later */
    PacketWrite2     (pak, cmd);        /* command                    */
    PacketWrite2     (pak, TCP_MSG_X1); /* unknown                    */
    PacketWrite2     (pak, seq);        /* sequence number            */
    PacketWrite4     (pak, 0);          /* unknown                    */
    PacketWrite4     (pak, 0);          /* unknown                    */
    PacketWrite4     (pak, 0);          /* unknown                    */
    PacketWrite2     (pak, type);       /* message type               */
    PacketWrite2     (pak, flags);      /* flags                      */
    PacketWrite2     (pak, status);     /* status                     */
    PacketWriteLNTS  (pak, msg);        /* the message                */
    
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
}

/*
 * Acks a TCP packet.
 */
static int TCPSendMsgAck (Session *sess, UWORD seq, UWORD sub_cmd, BOOL accept)
{
    Packet *pak;
    char *msg;
    UWORD status, flags;

    ASSERT_DIRECT (sess);

    switch (sub_cmd)
    {
        case TCP_MSG_GET_AWAY:  msg = prG->auto_away; break;
        case TCP_MSG_GET_OCC:   msg = prG->auto_occ;  break;
        case TCP_MSG_GET_NA:    msg = prG->auto_na;   break;
        case TCP_MSG_GET_DND:   msg = prG->auto_dnd;  break;
        case TCP_MSG_GET_FFC:   msg = prG->auto_ffc;  break;
        case TCP_MSG_GET_VER:   msg = "...";          break;
        default:
            if (sess->status & STATUSF_DND)
                msg = prG->auto_dnd;
            else if (sess->status & STATUSF_OCC)
                msg = prG->auto_occ;
            else if (sess->status & STATUSF_NA)
                msg = prG->auto_na;
            else if (sess->status & STATUSF_AWAY)
                msg = prG->auto_away;
            else
                msg = "";
    }

    if (sess->status & STATUSF_DND)  status  = TCP_STAT_DND;   else
    if (sess->status & STATUSF_OCC)  status  = TCP_STAT_OCC;   else
    if (sess->status & STATUSF_NA)   status  = TCP_STAT_NA;    else
    if (sess->status & STATUSF_AWAY) status  = TCP_STAT_AWAY;
    else                             status  = TCP_STAT_ONLINE;
    if (!accept)                     status  = TCP_STAT_REFUSE;

    flags = 0;
    if (sess->status & STATUSF_INV)  flags |= TCP_MSGF_INV;
    flags ^= TCP_MSGF_LIST;

    pak = PacketTCPC (sess, TCP_CMD_ACK, seq, sub_cmd, status, flags, msg);
    switch (sub_cmd)
    {
        case 3:
            PacketWriteB2   (pak, sess->port);   /* port */
            PacketWrite2    (pak, 0);            /* padding */
            PacketWriteStr  (pak, "");           /* file name - empty */
            PacketWrite4    (pak, 0);            /* file len - empty */
            PacketWrite4    (pak, sess->port);   /* port again */
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
    TCPSendPacket (pak, sess);

    free (msg);
    return 1;
}

/*
 * Requests the auto-response message from remote user.
 */
BOOL TCPGetAuto (Session *sess, UDWORD uin, UWORD which)
{
    Contact *cont;
    Packet *pak;
    Session *peer;

    if (!sess)
        return 0;
    if (uin == sess->assoc->uin)
        return 0;
    cont = ContactFind (uin);
    if (!cont)
        return 0;
    if (!(sess->connect & CONNECT_MASK))
        return 0;
    if (!cont->port)
        return 0;
    if (!cont->local_ip && !cont->outside_ip)
        return 0;

    ASSERT_LISTEN (sess);
    
    peer = SessionFind (TYPE_DIRECT, uin);
    if (peer && (peer->connect & CONNECT_FAIL))
        return 0;
    TCPDirectOpen (sess, uin);
    peer = SessionFind (TYPE_DIRECT, uin);
    
    if (!peer)
        return 0;

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

    pak = PacketTCPC (peer, TCP_CMD_MESSAGE, peer->our_seq, which, 0, sess->assoc->status, "...");
    PacketWrite4 (pak, TCP_COL_FG);      /* foreground color           */
    PacketWrite4 (pak, TCP_COL_BG);      /* background color           */

    peer->stat_real_pak_sent++;

    QueueEnqueueData (queue, peer, peer->our_seq--, QUEUE_TYPE_TCP_RESEND,
                      uin, time (NULL),
                      pak, strdup ("..."), &TCPCallBackResend);
    return 1;
}

/*
 * Sends a message via TCP.
 * Adds it to the resend queue until acked.
 */
BOOL TCPSendMsg (Session *sess, UDWORD uin, char *msg, UWORD sub_cmd)
{
    Contact *cont;
    Packet *pak;
    Session *peer;

    if (!sess)
        return 0;
    if (uin == sess->assoc->uin)
        return 0;
    cont = ContactFind (uin);
    if (!cont)
        return 0;
    if (!(sess->connect & CONNECT_MASK))
        return 0;
    if (!cont->port)
        return 0;
    if (!cont->local_ip && !cont->outside_ip)
        return 0;

    ASSERT_LISTEN (sess);
    
    peer = SessionFind (TYPE_DIRECT, uin);
    if (peer && (peer->connect & CONNECT_FAIL))
        return 0;
    TCPDirectOpen (sess, uin);
    peer = SessionFind (TYPE_DIRECT, uin);
    
    if (!peer)
        return 0;

    pak = PacketTCPC (peer, TCP_CMD_MESSAGE, peer->our_seq, sub_cmd, sess->assoc->status, 0, msg);
    PacketWrite4 (pak, TCP_COL_FG);      /* foreground color           */
    PacketWrite4 (pak, TCP_COL_BG);      /* background color           */

    peer->stat_real_pak_sent++;

    QueueEnqueueData (queue, peer, peer->our_seq--, QUEUE_TYPE_TCP_RESEND,
                      uin, time (NULL),
                      pak, strdup (msg), &TCPCallBackResend);
    return 1;
}

#ifdef WIP
/*
 * Sends a message via TCP.
 * Adds it to the resend queue until acked.
 */
BOOL TCPSendFile (Session *sess, UDWORD uin, char *file)
{
    Contact *cont;
    Packet *pak;
    Session *peer;

    if (!sess)
        return 0;
    if (uin == sess->assoc->uin)
        return 0;
    cont = ContactFind (uin);
    if (!cont)
        return 0;
    if (!(sess->connect & CONNECT_MASK))
        return 0;
    if (!cont->port)
        return 0;
    if (!cont->local_ip && !cont->outside_ip)
        return 0;

    ASSERT_LISTEN (sess);
    
    peer = SessionFind (TYPE_DIRECT, uin);
    if (peer && (peer->connect & CONNECT_FAIL))
        return 0;
    TCPDirectOpen (sess, uin);
    peer = SessionFind (TYPE_DIRECT, uin);
    
    if (!peer)
        return 0;

    if (peer->ver < 8)
    {
        pak = PacketTCPC (peer, TCP_CMD_MESSAGE, peer->our_seq, TCP_MSG_FILE, sess->assoc->status, 0, "no description");
        PacketWrite2 (pak, 0);
        PacketWrite2 (pak, 0);
        PacketWriteLNTS (pak, file);
        PacketWrite4 (pak, 12345);
        PacketWrite4 (pak, 0);
    }
    else
    {
        pak = PacketTCPC (peer, TCP_CMD_MESSAGE, peer->our_seq, TCP_MSG_GREETING, sess->assoc->status, 0, "");
        TCPGreet (pak, 0x29, "without reason", 0, 12345, file);
    }

    peer->stat_real_pak_sent++;

    QueueEnqueueData (queue, peer, peer->our_seq--, QUEUE_TYPE_TCP_RESEND,
                      uin, time (NULL),
                      pak, strdup (file), &TCPCallBackResend);
    return 1;
}
#endif

/*
 * Resends TCP packets if necessary
 */
static void TCPCallBackResend (Event *event)
{
    Contact *cont;
    Session *peer = event->sess;
    Packet *pak = event->pak;
    int delta = (peer->ver > 6 ? 1 : 0);
    
    ASSERT_DIRECT (peer);
    assert (pak);

    cont = ContactFind (event->uin);

    if (event->attempts >= MAX_RETRY_ATTEMPTS)
    {
        TCPClose (peer);
        peer->connect = CONNECT_FAIL;
    }

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
                TCPSendPacket (pak, peer);
            event->due = time (NULL) + 10;
        }
        else
            event->due = time (NULL) + 1;
        QueueEnqueue (queue, event);
        return;
    }

    if (PacketReadAt2 (pak, 4 + delta) == TCP_CMD_MESSAGE)
        icq_sendmsg (peer->assoc->assoc, cont->uin, event->info, PacketReadAt2 (pak, 22 + delta));
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
    const char *tmp, *tmp2;
    UWORD cmd, type, seq;
    UDWORD len, status, flags;
    Packet *pak;
    
    ASSERT_DIRECT (event->sess);
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
            event = QueueDequeue (queue, seq, QUEUE_TYPE_TCP_RESEND);
            if (!event)
                break;
            switch (type)
            {
                case TCP_MSG_MESS:
                    Time_Stamp ();
                    M_print (" " COLACK "%10s" COLNONE " " MSGTCPACKSTR "%s\n",
                             cont->nick, event->info);
                    break;

                    while (0) {  /* Duff-uesque */
                case TCP_MSG_GET_AWAY:  tmp2 = i18n (1972, "away");           break;
                case TCP_MSG_GET_OCC:   tmp2 = i18n (1973, "occupied");       break;
                case TCP_MSG_GET_NA:    tmp2 = i18n (1974, "not available");  break;
                case TCP_MSG_GET_DND:   tmp2 = i18n (1971, "do not disturb"); break;
                case TCP_MSG_GET_FFC:   tmp2 = i18n (1976, "free for chat");  break;
                case TCP_MSG_GET_VER:   tmp2 = i18n (2062, "version");  }
                    Time_Stamp ();
                    M_print (" " COLACK "%10s" COLNONE " <%s> %s\n",
                             cont->nick, tmp2, tmp);
                    break;

#ifdef WIP
                case TCP_MSG_FILE:
                    if (status)
                    {
                        M_print (i18n (2069, "File transfer %s rejected by peer (%x,%x): %s.\n"),
                                 event->info, status, flags, tmp);
                    }
                    else
                    {
                        Session *fsess;
                        UDWORD port;
                        
                        port = PacketReadB2 (pak);
                        M_print (i18n (2070, "File transfer to port %d.\n"), port);
                        
                        fsess = SessionClone (event->sess);
                        fsess->port = port;
                        fsess->assoc = event->sess->assoc;
                        fsess->ip = event->sess->ip;
                        fsess->server = "";
                        fsess->uin = event->sess->uin;

                        QueueEnqueueData (queue, fsess, fsess->our_seq--, QUEUE_TYPE_PEER_RESEND,
                                          event->sess->uin, time (NULL),
                                          NULL, strdup (event->info), &PeerFileResend);

                        SessionInitFile (fsess);
                    }
                    break;
#endif

                default:
                    if (prG->verbose & DEB_TCP)
                        M_print (i18n (1806, "Received ACK for message (seq %04x) from %s\n"),
                                 seq, cont->nick);
            }
            PacketD (event->pak);
            free (event);
            break;
        
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
                    tmp2 = PacketReadLNTS (pak);
                    len  = PacketRead4 (pak);
                    type = PacketRead4 (pak);
#ifdef WIP
                    if (event->sess->ver < 8 || cont->flags & CONT_TEMPORARY)
                    {
#endif
                    M_print (i18n (2061, "Refused file request %s (%d bytes) from %s (unknown: %x, %x)\n"),
                             tmp2, len, cont->nick, cmd, type);
                    TCPSendMsgAck (event->sess, seq, type, FALSE);
#ifdef WIP
                    }
                    else
                    {
                        M_print (i18n (2052, "Accepting file %s (%d bytes) from %s.\n"),
                                 tmp2, len, cont->nick);
                        TCPSendMsgAck (event->sess, seq, type, TRUE);
                    }
#endif
                    break;

                case TCP_MSG_GREETING:
#ifdef WIP
                    {
                        UWORD port, port2, flen, pad;
                        const char *text, *reason, *name;
                        
                        cmd    = PacketRead2 (pak);
                                 PacketReadData (pak, NULL, 16);
                                 PacketRead2 (pak);
                        text   = PacketReadDLStr (pak);
                                 PacketReadData (pak, NULL, 15);
                                 PacketRead4 (pak);
                        reason = PacketReadDLStr (pak);
                        port   = PacketReadB2 (pak);
                        pad    = PacketRead2 (pak);
                        name   = PacketReadDLStr (pak);
                        flen   = PacketRead4 (pak);
                        port2  = PacketRead4 (pak);
                        
                        switch (cmd)
                        {
                            case 0x0029:
                                if (event->sess->ver < 8 || cont->flags & CONT_TEMPORARY)
                                {
                                    M_print (i18n (2061, "Refused file request %s (%d bytes) from %s (unknown: %x, %x)\n"),
                                             name, flen, cont->nick, port, pad);
                                    TCPSendMsgAck (event->sess, seq, type, FALSE);
                                }
                                else
                                {
                                    M_print (i18n (2052, "Accepting file %s (%d bytes) from %s.\n"),
                                             name, flen, cont->nick);
                                    TCPSendMsgAck (event->sess, seq, type, TRUE);
                                }
                                break;
                            case 0x0032:

                            case 0x002d:
                                M_print (i18n (2064, "Refusing chat request (%s/%s/%s) from %s.\n"),
                                         text, reason, name, cont->nick);
                                TCPSendMsgAck (event->sess, seq, type, FALSE);

                            default:
                                if (prG->verbose & DEB_PROTOCOL)
                                    M_print (i18n (2065, "Unknown TCP_MSG_GREET_ command %04x.\n"), type);
                                TCPSendMsgAck (event->sess, seq, type, FALSE);
                                break;
                            
                        }
                    }
                    break;
#endif
                default:
                    if (prG->verbose & DEB_PROTOCOL)
                        M_print (i18n (2066, "Unknown TCP_MSG_ command %04x.\n"), type);

                /* Regular messages */
                case TCP_MSG_AUTO:
                case TCP_MSG_MESS:
                case TCP_MSG_URL:
                case TCP_MSG_REQ_AUTH:
                case TCP_MSG_DENY_AUTH:
                case TCP_MSG_GIVE_AUTH:
                case TCP_MSG_ADDED:
                case TCP_MSG_WEB_PAGER:
                case TCP_MSG_EMAIL_PAGER:
                case TCP_MSG_ADDUIN:
                    uiG.last_rcvd_uin = cont->uin;
                    Do_Msg (event->sess, NULL, type, tmp, cont->uin, STATUS_OFFLINE, 1);

                    TCPSendMsgAck (event->sess, seq, type, TRUE);
            }
            break;
        default:
            /* ignore */
    }
    PacketD (pak);
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
static void Encrypt_Pak (Session *sess, Packet *pak)
{
    UDWORD B1, M1, check, size;
    int i;
    UBYTE X1, X2, X3, *p;
    UDWORD hex, key;

    p = pak->data;
    size = pak->len;
    
    if (sess->ver > 6)
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
    PacketWriteAt4 (pak, sess->ver > 6 ? 1 : 0, check);
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

#ifdef WIP
static void PeerFileResend (Event *event)
{
    Contact *cont;
    Session *fpeer = event->sess;
    Packet *pak;
    
    ASSERT_FILE (fpeer);

    cont = ContactFind (event->uin);

    if (event->attempts >= MAX_RETRY_ATTEMPTS)
        TCPClose (fpeer);

    if (fpeer->connect & CONNECT_MASK)
    {
        if (fpeer->connect & CONNECT_OK)
        {
            pak = PacketC ();
            PacketWrite1 (pak, 0);
            PacketWrite4 (pak, 0);
            PacketWrite4 (pak, 1);
            PacketWrite4 (pak, 12345);
            PacketWrite4 (pak, 64);
            PacketWriteLNTS (pak, "my Nick");
            TCPSendPacket (pak, fpeer);
            fpeer->dispatch = &PeerFileDispatch;
        }
        else
        {
            if (event->attempts)
            {
                Time_Stamp ();
                M_print (" " COLACK "%10s" COLNONE " %s%s\n", cont->nick, " + ", event->info);
            }

            event->attempts++;
            event->due = time (NULL) + 3;
            QueueEnqueue (queue, event);
            return;
        }
    }
    free (event->info);
    free (event);
}

void SessionInitFile (Session *sess)
{
    if (prG->verbose)
        M_print (i18n (2068, "Opening file transfer connection at %s:%d... "),
                 sess->ip ? sess->server = strdup (UtilIOIP (sess->ip)) : "localhost", sess->port);

    sess->connect  = 0;
    sess->type     = TYPE_FILE;
    sess->flags    = 0;
    
    if (sess->ip)
    {
        TCPDispatchConn (sess);
    }
    else
    {
        sess->port = 0;
        sess->dispatch = &TCPDispatchMain;
        UtilIOConnectTCP (sess);
    }
}

#define FAIL(x) { err = x; break; }
#define PeerFileClose TCPClose

void PeerFileDispatch (Session *sess)
{
    Packet *pak;
    UDWORD err = 0;
    
    ASSERT_FILE (sess);
    
    if (!(pak = UtilIOReceiveTCP (sess)))
        return;

    if (prG->verbose & DEB_PACKTCP)
        TCPPrint (pak, sess, FALSE);

    switch (PacketRead1 (pak))
    {
        const char *name, *text;
        UDWORD len, off, nr, speed;

        case 0:
                   PacketRead4 (pak); /* EMPTY */
            nr   = PacketRead4 (pak); /* COUNT */
            len  = PacketRead4 (pak); /* BYTES */
            speed= PacketRead4 (pak); /* SPEED */
            name = PacketReadLNTS (pak); /* NICK  */
            PacketD (pak);
            
            M_print ("Incoming initialization: %d files with together %d bytes @ %x from %s.\n",
                     nr, len, speed, name);
            
            pak = PacketC ();
            PacketWrite1 (pak, 1);
            PacketWrite4 (pak, 64);
            PacketWriteLNTS (pak, "my Nick0");
            TCPSendPacket (pak, sess);
            return;
        
        case 1:
            speed= PacketRead4 (pak); /* SPEED */
            name = PacketReadLNTS (pak); /* NICK  */
            PacketD (pak);
            
            M_print ("Files accepted @ %x by %s.\n", speed, name);
            
            pak = PacketC ();
            PacketWrite1 (pak, 2);
            PacketWrite1 (pak, 0);
            PacketWriteLNTS (pak, "filename");
            PacketWriteLNTS (pak, "");
            PacketWrite4 (pak, 12345);
            PacketWrite4 (pak, 0);
            PacketWrite4 (pak, 64);
            TCPSendPacket (pak, sess);
            return;
            
        case 2:
                   PacketRead1 (pak); /* EMPTY */
            name = PacketReadLNTS (pak);
            text = PacketReadLNTS (pak);
            len  = PacketRead4 (pak);
                   PacketRead4 (pak); /* EMPTY */
                   PacketRead4 (pak); /* SPEED */
            PacketD (pak);
            
            M_print ("Starting receiving %s (%s), len %d\n",
                     name, text, len);
            
            pak = PacketC ();
            PacketWrite1 (pak, 3);
            PacketWrite4 (pak, 0);
            PacketWrite4 (pak, 0);
            PacketWrite4 (pak, 64);
            PacketWrite4 (pak, 1);
            TCPSendPacket (pak, sess);
            return;

        case 3:
            off = PacketRead4 (pak);
                  PacketRead4 (pak); /* EMPTY */
                  PacketRead4 (pak); /* SPEED */
            nr  = PacketRead4 (pak); /* NR */
            PacketD (pak);
            
            M_print ("Sending file nr %d at offset %d.\n",
                     nr, off);
            
            pak = PacketC ();
            PacketWrite1 (pak, 6);
            PacketWrite1 (pak, 'A'); /* DATA ... */
            PacketWrite1 (pak, 'B');
            PacketWrite1 (pak, 'C');
            PacketWrite1 (pak, 'D');
            PacketWrite1 (pak, 'E');
            PacketWrite1 (pak, 'F');
            PacketWrite1 (pak, 'G');
            TCPSendPacket (pak, sess);
            return;
            
        case 4:
            M_print ("File transfer aborted by peer (%d).\n",
                     PacketRead1 (pak));
            PacketD (pak);
            PeerFileClose (sess);
            return;

        case 5:
            M_print ("Ignoring speed change to %d.\n",
                     PacketRead1 (pak));
            PacketD (pak);
            return;

        case 6:
        default:
            M_print ("Error - unknown packet.\n");
            Hex_Dump (pak->data, pak->len);
            PacketD (pak);
            PeerFileClose (sess);
    }
    if ((prG->verbose & DEB_TCP) && err)
    {
        Time_Stamp ();
        M_print (" %s: %d\n", i18n (2029, "Protocol error on peer-to-peer connection"), err);
        PeerFileClose (sess);
    }
}
#endif /* WIP */

#endif /* TCP_COMM */
