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
#include <arpa/inet.h>

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

static void       TCPCallBackTimeout (struct Event *event);
static void       TCPCallBackTOConn  (struct Event *event);
static void       TCPCallBackResend  (struct Event *event);
static void       TCPCallBackReceive (struct Event *event);

static void       Encrypt_Pak        (Session *sess, Packet *pak);
static int        Decrypt_Pak        (UBYTE *pak, UDWORD size);
static int        Send_TCP_Ack (Session *sess, UWORD seq, UWORD sub_cmd, BOOL accept);
/*static void       Get_Auto_Resp (Session *sess, UDWORD uin);*/

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
 * Closes TCP message connection to given contact.
 */
void TCPDirectClose (UDWORD uin)
{
    Session *peer;
    
    if ((peer = SessionFind (TYPE_DIRECT, uin)))
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
void TCPDispatchReconn (Session *sess)
{
    Time_Stamp ();
    M_print (" %s%10s%s ", COLCONTACT, ContactFindName (sess->uin), COLNONE);
    M_print (i18n (2023, "Direct connection closed by peer.\n"));
}

/*
 * Accepts a new direct connection.
 */
void TCPDispatchMain (Session *sess)
{
    struct sockaddr_in sin;
    Session *peer;
    int tmp;
 
    ASSERT_LISTEN (sess);

    if (!(sess->connect & CONNECT_OK))
    {
        switch (sess->connect & 3)
        {
            case 1:
                sess->connect |= CONNECT_OK | CONNECT_SELECT_R;
                sess->connect &= ~CONNECT_SELECT_W & ~CONNECT_SELECT_X;
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
void TCPDispatchConn (Session *sess)
{
    Contact *cont;
    int rc;
    
    ASSERT_DIRECT (sess);
    
    sess->dispatch = &TCPDispatchConn;

    while (1)
    {
        cont = ContactFind (sess->uin);
        if (!cont)
        {
            TCPClose (sess);
            return;
        }
        rc = 0;
        if (prG->verbose)
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
                sess->ip      = cont->outside_ip;
                sess->port    = cont->port;
                sess->connect = 1;
                
                Time_Stamp ();
                M_print (" %s%10s%s ", COLCONTACT, ContactFindName (sess->uin), COLNONE);
                M_print (i18n (2034, "Opening TCP connection at %s:%d... "),
                         UtilIOIP (sess->ip), sess->port);
                UtilIOConnectTCP (sess);
                return;
            case 3:
                if (!cont->local_ip || !cont->port)
                {
                    sess->connect = CONNECT_FAIL;
                    return;
                }
                sess->server  = NULL;
                sess->ip      = cont->local_ip;
                sess->port    = cont->port;
                sess->connect = 3;
                
                Time_Stamp ();
                M_print (" %s%10s%s ", COLCONTACT, ContactFindName (sess->uin), COLNONE);
                M_print (i18n (2034, "Opening TCP connection at %s:%d... "),
                         UtilIOIP (sess->ip), sess->port);
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
void TCPDispatchShake (Session *sess)
{
    Contact *cont;
    Packet *pak = NULL;
    
    ASSERT_DIRECT (sess);
    
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

        if (prG->verbose)
        {
            Time_Stamp ();
            M_print (" [%d %p %p] %s State: %d\n", sess->sok, pak, sess, ContactFindName (sess->uin), sess->connect);
        }

        cont = ContactFind (sess->uin);
        if (!cont && (sess->connect & CONNECT_MASK) != 16)
        {
            TCPClose (sess);
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
                Time_Stamp ();
                M_print (" %s%10s%s ", COLCONTACT, ContactFindName (sess->uin), COLNONE);
                M_print (i18n (1833, "Peer to peer TCP connection established.\n"), cont->nick);
                sess->connect = CONNECT_OK | CONNECT_SELECT_R;
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
void TCPDispatchPeer (Session *sess)
{
    Contact *cont;
    Packet *pak;
    struct Event *event;
    int i = 0;
    UWORD seq_in = 0, seq, cmd, type;
    
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
        type = PacketReadAt2 (pak, PacketReadPos (pak) + 18);
        
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
void TCPCallBackTimeout (struct Event *event)
{
    Session *sess = event->sess;
    
    ASSERT_DIRECT (event->sess);
    assert (event->type == QUEUE_TYPE_TCP_TIMEOUT);
    
    if (sess->connect & CONNECT_MASK)
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
void TCPCallBackTOConn (struct Event *event)
{
    ASSERT_DIRECT (event->sess);

    event->sess->connect += 2;
    TCPDispatchConn (event->sess);
    free (event);
}

/*
 *  Receives an incoming TCP packet.
 *  Resets socket on error. Paket must be freed.
 */
Packet *TCPReceivePacket (Session *sess)
{
    Packet *pak;

    ASSERT_DIRECT (sess);

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

    if (sess->connect & CONNECT_OK)
    {
        if (Decrypt_Pak ((UBYTE *) &pak->data + (sess->ver > 6 ? 1 : 0), pak->len - (sess->ver > 6 ? 1 : 0)) < 0)
        {
            Time_Stamp ();
            M_print (" \x1b«" COLSERV "");
            M_print (i18n (1789, "Received malformed packet: (%d)"), sess->sok);
            M_print (COLNONE "\n");
            Hex_Dump (pak->data, pak->len);
            M_print (ESC "»\r");

            TCPClose (sess);
            PacketD (pak);
            return NULL;
        }
    }

    if (prG->verbose & 4)
    {
        Time_Stamp ();
        M_print (" \x1b«" COLSERV "");
        M_print (i18n (1778, "Incoming TCP packet: (%d)"), sess->sok);
        M_print (COLNONE "\n");
        Hex_Dump (pak->data, pak->len);
        M_print (ESC "»\r");
    }
    
    pak->rpos = 0;
    pak->wpos = 0;
    return pak;
}

/*
 * Encrypts and sends a TCP packet.
 * Resets socket on error.
 */
void TCPSendPacket (Packet *pak, Session *sess)
{
    Packet *tpak;
    void *data;
    UBYTE buf[2];
    int rc, todo, bytessend = 0;
    
    ASSERT_DIRECT (sess);
    
    sess->stat_pak_sent++;
    
    if (!(sess->connect & CONNECT_MASK))
        return;

    if (prG->verbose & 4)
    {
        Time_Stamp ();
        M_print (" \x1b«" COLCLIENT "");
        M_print (i18n (1776, "Outgoing TCP packet (%d):"), sess->sok);
        M_print (COLNONE "\n");
        Hex_Dump (pak->data, pak->len);
        M_print (ESC "»\r");
    }

    tpak = PacketClone (pak);
    switch (PacketReadAt1 (tpak, 0))
    {
        case PEER_INIT:
        case PEER_INITACK:
        case PEER_INIT2:
            break;

        default:
            Encrypt_Pak (sess, tpak);
    }
    
    data = (void *) &tpak->data;

    for (;;)
    {
        errno = 0;

        buf[0] = tpak->len & 0xFF;
        buf[1] = tpak->len >> 8;
        if (sockwrite (sess->sok, buf, 2) < 2)
            break;

        for (todo = tpak->len; todo > 0; todo -= bytessend, data += bytessend)
        {
            bytessend = sockwrite (sess->sok, data, todo);
            if (bytessend <= 0)
                break;
        }
        if (bytessend <= 0)
            break;
        return;
    }
    
    if (prG->verbose)
    {
        rc = errno;
        Time_Stamp ();
        M_print (" ");
        M_print (i18n (1835, "Error while writing to socket - %s (%d)\n"),
                 strerror (rc), rc);
    }
    TCPClose (sess);
}

/*
 * Sends a TCP initialization packet.
 */
void TCPSendInitv6 (Session *sess)
{
    Packet *pak;
    
    ASSERT_DIRECT (sess);

    if (!(sess->connect & CONNECT_MASK))
        return;

    if (!(sess->our_session))
        sess->our_session = rand ();

    sess->stat_real_pak_sent++;

    if (prG->verbose)
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
void TCPSendInit (Session *sess)
{
    Packet *pak;
    
    ASSERT_DIRECT (sess);

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

    if (prG->verbose)
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
void TCPSendInitAck (Session *sess)
{
    Packet *pak;
    
    ASSERT_DIRECT (sess);

    if (!(sess->connect & CONNECT_MASK))
        return;

    if (prG->verbose)
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

void TCPSendInit2 (Session *sess)
{
    Packet *pak;
    
    ASSERT_DIRECT (sess);
    
    if (sess->ver == 6)
        return;
    
    if (!(sess->connect & CONNECT_MASK))
        return;
    
    if (prG->verbose)
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

Session *TCPReceiveInit (Session *sess, Packet *pak)
{
    Contact *cont;
    UDWORD muin, uin, sid, port, port2, oip, iip;
    UWORD  cmd, len, len2, tcpflag, err, nver;
    Session *peer;

    ASSERT_DIRECT (sess);
    assert (pak);

    for (err = 0;;)
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

        if (prG->verbose)
        {
            Time_Stamp ();
            M_print (" %d [%p] ", uin, sess);
            M_print (i18n (1838, "Received direct connection initialization.\n"));
            M_print ("    \x1b«");
            M_print (i18n (1839, "Version %04x:%04x, Port %d, UIN %d, session %08x\n"),
                     sess->ver, len, port, uin, sid);
            M_print ("\x1b»\n");
        }

        if ((peer = SessionFind (TYPE_DIRECT, uin)) && peer != sess)
        {
            if (peer->connect & CONNECT_OK)
            {
                TCPClose (sess);
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
    if (prG->verbose && err)
    {
        Time_Stamp ();
        M_print (" %s: %d\n", i18n (2029, "Protocol error on peer-to-peer connection"), err);
    }
    sess->connect = 0;
    TCPClose (sess);
    return NULL;
}

/*
 * Receives the acknowledge packet for the initialization packet.
 */
void TCPReceiveInitAck (Session *sess, Packet *pak)
{
    ASSERT_DIRECT (sess);
    assert (pak);
    
    if (pak->len != 4 || PacketReadAt4 (pak, 0) != PEER_INITACK)
    {
        M_print (i18n (1841, "Received malformed initialization acknowledgement packet.\n"));
        TCPClose (sess);
    }
}

Session *TCPReceiveInit2 (Session *sess, Packet *pak)
{
    UWORD  cmd, err;
    UDWORD ten, one;

    ASSERT_DIRECT (sess);
    assert (pak);

    if (!pak)
        return sess;

    for (err = 0;;)
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

    if (err)
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
void TCPClose (Session *sess)
{
    ASSERT_DIRECT (sess);
    
    if (sess->connect & CONNECT_MASK)
    {
        Time_Stamp ();
        M_print (" ");
        if (sess->uin)
            M_print (i18n (1842, "Closing socket %d to %s.\n"), sess->sok, ContactFindName (sess->uin));
        else
            M_print (i18n (1843, "Closing socket %d.\n"), sess->sok);
    }

    if (sess->sok != -1)
        close (sess->sok);
    sess->sok     = -1;
    sess->connect = 0;
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


/*
 * Sends a message via TCP.
 * Adds it to the resend queue until acked.
 */
BOOL TCPSendMsg (Session *sess, UDWORD uin, char *msg, UWORD sub_cmd)
{
    Contact *cont;
    int status;
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

         if (sess->status & STATUSF_INV)
        status = TCP_MSGF_INV;
    else if (sess->status & STATUSF_DND)
        status = TCP_MSGF_DND;
    else if (sess->status & STATUSF_OCC)
        status = TCP_MSGF_OCC;
    else if (sess->status & STATUSF_NA)
        status = TCP_MSGF_NA;
    else if (sess->status & STATUSF_AWAY)
        status = TCP_MSGF_AWAY;
    else
        status = 0;

    status ^= TCP_MSGF_LIST;

    pak = PacketC ();
    if (peer->ver > 6)
        PacketWrite1 (pak, PEER_MSG);
    PacketWrite4 (pak, 0);               /* checksum - filled in later */
    PacketWrite2 (pak, TCP_CMD_MESSAGE); /* command                    */
    PacketWrite2 (pak, TCP_MSG_X1);      /* unknown                    */
    PacketWrite2 (pak, peer->our_seq);   /* sequence number            */
    PacketWrite4 (pak, 0);               /* unknown                    */
    PacketWrite4 (pak, 0);               /* unknown                    */
    PacketWrite4 (pak, 0);               /* unknown                    */
    PacketWrite2 (pak, sub_cmd);         /* message type               */
    PacketWrite2 (pak, 0);               /* status - filled in later   */
    PacketWrite2 (pak, status);          /* our status                 */
    PacketWriteLNTS (pak, msg);          /* the message                */
    PacketWrite4 (pak, TCP_COL_FG);      /* foreground color           */
    PacketWrite4 (pak, TCP_COL_BG);      /* background color           */

    peer->stat_real_pak_sent++;

    QueueEnqueueData (queue, peer, peer->our_seq--, QUEUE_TYPE_TCP_RESEND,
                      uin, time (NULL),
                      pak, strdup (msg), &TCPCallBackResend);
    return 1;
}

/*
 * Resends TCP packets if necessary
 */
void TCPCallBackResend (struct Event *event)
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

            event->attempts++;
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
void TCPCallBackReceive (struct Event *event)
{
    Contact *cont;
    const char *tmp;
    UWORD cmd, type, seq, status;
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
             PacketRead2 (pak);
    status = PacketRead2 (pak);
    tmp    = PacketReadLNTS (pak);
    /* fore/background color ignored */
    
    switch (cmd)
    {
        case TCP_CMD_ACK:
            event = QueueDequeue (queue, seq, QUEUE_TYPE_TCP_RESEND);
            if (!event)
                break;
            if (type == NORM_MESS)
            {
                Time_Stamp ();
                M_print (" " COLACK "%10s" COLNONE " " MSGTCPACKSTR "%s\n",
                         cont->nick, event->info);
            }
            else if (type & TCP_AUTO_RESPONSE_MASK)
            {
                M_print (i18n (1965, "Auto-response message for %s:\n"), cont->nick);
                M_print (MESSCOL "%s\n" NOCOL, PacketReadAtLNTS (pak, PacketReadPos (pak) + 20));
            }
            if (prG->verbose && type != NORM_MESS)
            {
                M_print (i18n (1806, "Received ACK for message (seq %04x) from %s\n"),
                         seq, cont->nick);
            }
            PacketD (event->pak);
            free (event);
            break;
        
        /* Requests for auto-response message */
        case TCP_CMD_GET_AWAY:
        case TCP_CMD_GET_OCC:
        case TCP_CMD_GET_NA:
        case TCP_CMD_GET_DND:
        case TCP_CMD_GET_FFC:
            M_print (i18n (1814, "Sent auto-response message to %s%s%s.\n"),
                     COLCONTACT, cont->nick, COLNONE);
            Send_TCP_Ack (event->sess, seq, type, TRUE);
            break;

        /* Automatically reject file xfer and chat requests
             as these are not implemented yet. */ 
        case TCP_MSG_MESS:
        case TCP_MSG_FILE:
            Send_TCP_Ack (event->sess, seq, type, FALSE);
            break;

        /* Regular messages */
        default:
            uiG.last_rcvd_uin = cont->uin;
            Do_Msg (event->sess, NULL, type, tmp, cont->uin, STATUS_OFFLINE, 1);

            Send_TCP_Ack (event->sess, seq, type, TRUE);
    }
    PacketD (pak);
}


/*
 * Requests the auto-response message
 * from remote user.
 */
/*
void Get_Auto_Resp (Session *sess, UDWORD uin)
{
    Contact *cont;
    UWORD sub_cmd;
    char *msg = "...";    / * seem to need something (anything!) in the msg * /

    cont = ContactFind (uin);
    
    assert (cont);
    ASSERT_DIRECT (sess);

         if (sess->status & STATUSF_INV)
        sub_cmd = 0;
    else if (cont->status & STATUSF_DND)
        sub_cmd = TCP_CMD_GET_DND;
    else if (cont->status & STATUSF_OCC)
        sub_cmd = TCP_CMD_GET_OCC;
    else if (cont->status & STATUSF_NA)
        sub_cmd = TCP_CMD_GET_NA;
    else if (cont->status & STATUSF_AWAY)
        sub_cmd = TCP_CMD_GET_AWAY;
    else if (cont->status & STATUSF_FFC)
        sub_cmd = TCP_CMD_GET_FFC;
    else
        sub_cmd = 0;

    if (cont->connection_type != TCP_OK_FLAG)
        sub_cmd = 0;

    if (sub_cmd == 0)
        M_print (i18n (1809, "Unable to retrieve auto-response message for %s."),
                 ContactFindName (uin));
    else
        TCPSendMsg (sess, uin, msg, sub_cmd);
}
*/

/*
 * Acks a TCP packet.
 */
int Send_TCP_Ack (Session *sess, UWORD seq, UWORD sub_cmd, BOOL accept)
{
    Packet *pak;
    char *msg;

    ASSERT_DIRECT (sess);
    msg = Get_Auto_Reply (sess->assoc->assoc);
 
    pak = PacketC ();
    if (sess->ver > 6)
        PacketWrite1 (pak, 2);
    PacketWrite4 (pak, 0);               /* checksum - filled in later */
    PacketWrite2 (pak, TCP_CMD_ACK);     /* command                    */
    PacketWrite2 (pak, TCP_MSG_X1);      /* unknown                    */
    PacketWrite2 (pak, seq);             /* sequence number            */
    PacketWrite4 (pak, 0);               /* unknown                    */
    PacketWrite4 (pak, 0);               /* unknown                    */
    PacketWrite4 (pak, 0);               /* unknown                    */
    PacketWrite2 (pak, sub_cmd);         /* message type               */
    PacketWrite2 (pak, accept ? TCP_STAT_ONLINE : TCP_STAT_REFUSE);
                                         /* status                     */
    PacketWrite2 (pak, TCP_MSG_AUTO);    /* message type               */
    PacketWriteLNTS (pak, msg);          /* the message                */
    PacketWrite4 (pak, TCP_COL_FG);      /* foreground color           */
    PacketWrite4 (pak, TCP_COL_BG);      /* background color           */

    TCPSendPacket (pak, sess);

    free (msg);
    return 1;
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
void Encrypt_Pak (Session *sess, Packet *pak)
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

int Decrypt_Pak (UBYTE *pak, UDWORD size)
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
#endif
