/*
 * This file handles TCP client-to-client communications.
 *
 *  Author/Copyright: James Schofield (jschofield@ottawa.com) 22 Feb 2001
 *  Lots of changes from Rüdiger Kuhlmann.
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

static void       TCPDispatchMain   (Session *sess);
static void       TCPDispatchConn   (Session *sess);
static void       TCPDispatchShake  (Session *sess);
static void       TCPDispatchPeer   (Session *sess);

static Packet    *TCPReceivePacket  (Session *sess);
static void       TCPSendPacket     (Packet *pak, Session *sess);
static void       TCPClose          (Session *sess);

static void       TCPSendInit       (Session *sess);
static void       TCPSendInitAck    (Session *sess);
static Session   *TCPReceiveInit    (Session *sess, Packet *pak);
static void       TCPReceiveInitAck (Session *sess, Packet *pak);

static void       TCPCallBackTimeout (struct Event *event);
static void       TCPCallBackTOConn  (struct Event *event);
static void       TCPCallBackResend  (struct Event *event);
static void       TCPCallBackReceive (struct Event *event);

static void       Encrypt_Pak        (Packet *pak);
static int        Decrypt_Pak        (UBYTE *pak, UDWORD size);
static int        Send_TCP_Ack (Session *sess, UWORD seq, UWORD sub_cmd, BOOL accept);
static void       Get_Auto_Resp (Session *sess, UDWORD uin);

/*********************************************/

/*
 * "Logs in" TCP connection by opening listening socket.
 */
void SessionInitPeer (Session *sess)
{
    int port;
    
    assert (sess);
    
    port = sess->port;
    M_print (i18n (777, "Opening peer-to-peer connection at localhost:%d... "), port);

    sess->connect = 0;
    sess->our_seq = -1;
    sess->type = 4;

    UtilIOConnectTCP (sess);
    
    if (!sess->connect)
        return;

    sess->dispatch = &TCPDispatchMain;
    if (port)
        while (port > 10)
            M_print ("\b");
    else
        M_print ("\b");
    M_print ("\b\b\b\b%d.\n", sess->port);
}

/*
 *  Starts establishing a TCP connection to given contact.
 */
void TCPDirectOpen (Session *sess, UDWORD uin)
{
    Session *peer;
    
    ASSERT_PEER (sess);
    
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
    peer->spref = NULL;

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
    cont = ContactFindM (uin);
    
    if (!peer && cont)
    {
        peer = SessionC ();
        if (!peer)
            return;
    }
    peer->uin = cont->uin;
    peer->connect = CONNECT_FAIL;
}

/**************************************************/

/*
 * Accepts a new direct connection.
 */
void TCPDispatchMain (Session *sess)
{
    struct sockaddr_in sin;
    Session *peer;
    int tmp;
 
    ASSERT_PEER (sess);
 
    tmp = sizeof (sin);
    peer = SessionClone (sess);
    
    if (!peer)
    {
        M_print (i18n (914, "Can't allocate connection structure.\n"));
        sess->connect = 0;
        if (sess->sok)
            sockclose (sess->sok);
        return;
    }

    peer->type = TYPE_DIRECT;
    peer->spref = NULL;
    peer->sok  = accept (sess->sok, (struct sockaddr *)&sin, &tmp);
    
    if (peer->sok <= 0)
    {
        peer->connect = 0;
        peer->sok = -1;
        return;
    }
    
    peer->connect     = 10 | CONNECT_SELECT_R;
    peer->our_session = 0;
    peer->uin         = 0;
    peer->dispatch    = &TCPDispatchShake;
}

/*
 * Continues connecting.
 */
void TCPDispatchConn (Session *sess)
{
    Contact *cont;
    int flags, rc;
    
    ASSERT_DIRECT (sess);
    
    sess->dispatch = &TCPDispatchConn;

    while (1)
    {
        cont = ContactFind (sess->uin);
        assert (cont);
        rc = 0;
        if (prG->verbose)
            M_print ("debug: TCPDispatchConn; Nick: %s state: %d\n", ContactFindName (sess->uin), sess->connect);

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
                sess->connect = 0;
                
                M_print (i18n (631, "Opening TCP connection to %s%s%s at %s:%d... "),
                             COLCONTACT, cont->nick, COLNONE, UtilIOIP (sess->ip), sess->port);
                UtilIOConnectTCP (sess);
                switch (sess->connect & CONNECT_MASK)
                {
                    case 0:
                        sess->connect = 3;
                        break;
                    case 1:
                        QueueEnqueueData (queue, sess, sess->ip, QUEUE_TYPE_TCP_TIMEOUT,
                                          cont->uin, time (NULL) + 10,
                                          NULL, NULL, &TCPCallBackTOConn);
                        return;
                    case 2:
                        sess->connect = TCP_STATE_CONNECTED;
                        break;
                }
                break;
            case 1:
                flags = sizeof (int);
                if (getsockopt (sess->sok, SOL_SOCKET, SO_ERROR, &rc, &flags) < 0)
                    rc = errno;
                if (!rc)
                {
                    sess->connect = TCP_STATE_CONNECTED;
                    break;
                }
                QueueDequeue (queue, sess->ip, QUEUE_TYPE_TCP_TIMEOUT);
            case 2:
                if (!rc)
                    rc = ETIMEDOUT;
                sockclose (sess->sok);
                M_print (i18n (633, "failed:\n"));
                M_print (i18n (634, "Connection failed: %s (%d)\n"), strerror (rc), rc);
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
                
                M_print (i18n (631, "Opening TCP connection to %s%s%s at %s:%d... "),
                             COLCONTACT, cont->nick, COLNONE, UtilIOIP (sess->ip), sess->port);

                UtilIOConnectTCP (sess);
                /* connect = 4,5 */
                
                switch (sess->connect)
                {
                    case 0:
                        sess->connect = 5;
                        break;
                    case 4:
                        QueueEnqueueData (queue, sess, sess->ip, QUEUE_TYPE_TCP_TIMEOUT,
                                          cont->uin, time (NULL) + 10,
                                          NULL, NULL, &TCPCallBackTOConn);
                        return;
                    case 5:
                        sess->connect = TCP_STATE_CONNECTED;
                        break;
                }
                break;
            case 4:
                flags = sizeof (int);
                if (getsockopt (sess->sok, SOL_SOCKET, SO_ERROR, &rc, &flags) < 0)
                    rc = errno;
                if (!rc)
                {
                    sess->connect = TCP_STATE_CONNECTED;
                    break;
                }
                QueueDequeue (queue, sess->ip, QUEUE_TYPE_TCP_TIMEOUT);
            case 5:
                if (!rc)
                    rc = ETIMEDOUT;
                sockclose (sess->sok);
                M_print (i18n (633, "failed:\n"));
                M_print (i18n (634, "Connection failed: %s (%d)\n"), strerror (rc), rc);
            case 6:
            {
                if (sess->assoc && sess->assoc->assoc && sess->assoc->assoc->ver < 7)
                {
                    CmdPktCmdTCPRequest (sess->assoc->assoc, cont->uin, cont->port);
                    QueueEnqueueData (queue, sess, sess->ip, QUEUE_TYPE_TCP_TIMEOUT,
                                      cont->uin, time (NULL) + 30,
                                      NULL, NULL, &TCPCallBackTOConn);
                }
                sockclose (sess->sok);
                sess->sok = -1;
                sess->connect = TCP_STATE_WAITING;
                return;
            }
            case TCP_STATE_CONNECTED:
                QueueDequeue (queue, sess->ip, QUEUE_TYPE_TCP_TIMEOUT);
                if (prG->verbose)
                {
                    M_print (i18n (779, "Opening TCP connection to %s at %s:%d... "), cont->nick, UtilIOIP (sess->ip), sess->port);
                    M_print (i18n (785, "success.\n"));
                }
                sess->connect = 1 | CONNECT_SELECT_R;
                sess->dispatch = &TCPDispatchShake;
                QueueEnqueueData (queue, sess, sess->ip, QUEUE_TYPE_TCP_TIMEOUT,
                                  cont->uin, time (NULL) + 10,
                                  NULL, NULL, &TCPCallBackTimeout);
                TCPSendInit (sess);
                return;
            case TCP_STATE_WAITING:
                QueueDequeue (queue, sess->ip, QUEUE_TYPE_TCP_TIMEOUT);
                M_print (i18n (855, "TCP connection to %s at %s:%d failed.\n") , cont->nick, UtilIOIP (sess->ip), sess->port);
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
    
    pak = TCPReceivePacket (sess);
    if (!pak)
        return;

    while (1)
    {
        if (prG->verbose)
            M_print ("debug: TCPDispatchShake; Nick: %s state: %d\n", ContactFindName (sess->uin), sess->connect);

        cont = ContactFind (sess->uin);
        assert (cont || ((sess->connect & CONNECT_MASK) == 10));
        
        switch (sess->connect & CONNECT_MASK)
        {
            case 1:
                sess->connect++;
                TCPReceiveInitAck (sess, pak);
                return;
            case 2:
                sess->connect++;
                TCPReceiveInit (sess, pak);
                continue;
            case 3:
                sess->connect = 31 | CONNECT_SELECT_R;
                TCPSendInitAck (sess);
                continue;
            case 10:
                sess = TCPReceiveInit (sess, pak);
                if (!sess || !sess->connect)
                    return;
                sess->connect++;
                TCPSendInitAck (sess);
                continue;
            case 11:
                sess->connect++;
                TCPSendInit (sess);
                return;
            case 12:
                sess->connect = 31 | CONNECT_SELECT_R;
                TCPReceiveInitAck (sess, pak);
                continue;
            case 31:
                QueueDequeue (queue, sess->ip, QUEUE_TYPE_TCP_TIMEOUT);
                Time_Stamp ();
                M_print (" ");
                M_print (i18n (833, "Client-to-client TCP connection to %s established.\n"), cont->nick);
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
    int size;
    int i = 0;
    UWORD seq_in = 0;
    
    ASSERT_DIRECT (sess);
    
    cont = ContactFind (sess->uin);
    assert (cont);

    /* Recv all packets before doing anything else.
         The objective is to delete any packets CANCELLED by the remote user. */
    while (M_Is_Set (sess->sok) && i++ <= TCP_MSG_QUEUE)
    {
        if (!(pak = TCPReceivePacket (sess)))
            return;

        size = pak->len;

        if (Decrypt_Pak ((UBYTE *) &pak->data, size) < 0)
        {
            M_print (i18n (789, "Received malformed packet."));
            TCPClose (sess);
            break;
        }

        if (prG->verbose & 4)
        {
            Time_Stamp ();
            M_print (" \x1b«" COLSERV "");
            M_print (i18n (778, "Incoming TCP packet:"));
            M_print (COLNONE "\n");
            Hex_Dump (pak->data, size);
            M_print (ESC "»\r");
        }
        
        /* Make sure this isn't a resend */
        if ((seq_in == 0) || (PacketReadAt2 (pak, 8) < seq_in))
        { 
            seq_in = PacketReadAt2 (pak, 8);

            /* Deal with CANCEL and ACK packets now */
            switch (PacketReadAt2 (pak, 4))
            {
                case TCP_CMD_ACK:
                    event = QueueDequeue (queue, seq_in, QUEUE_TYPE_TCP_RESEND);
                    if (!event)
                        break;
                    if (PacketReadAt2 (pak, 26) == NORM_MESS)
                    {
                        log_event (cont->uin, LOG_MESS, "You sent a TCP message to %s\n%s\n",
                                   cont->nick, PacketReadAtStrN (pak, 28));

                        Time_Stamp ();
                        M_print (" " COLACK "%10s" COLNONE " " MSGTCPACKSTR "%s\n",
                                 cont->nick, event->info);
                    }
                    else if (PacketReadAt2 (pak, 26) & TCP_AUTO_RESPONSE_MASK)
                    {
                        M_print (i18n (636, "Auto-response message for %s:\n"), cont->nick);
                        M_print (MESSCOL "%s\n" NOCOL, PacketReadAtStrN (pak, 28));
                    }
                    if (prG->verbose && PacketReadAt2 (pak, 26) != NORM_MESS)
                    {
                        M_print (i18n (806, "Received ACK for message (seq %04X) from %s\n"),
                                 seq_in, cont->nick);
                    }
                    PacketD (event->pak);
                    free (event);
                    break;
                
                case TCP_CMD_CANCEL:
                    event = QueueDequeue (queue, seq_in, QUEUE_TYPE_TCP_RECEIVE);
                    if (!event)
                        break;
                    
                    if (prG->verbose)
                    {
                        M_print (i18n (807, "Cancelled incoming message (seq %04X) from %s\n"),
                                 seq_in, cont->nick);
                    }
                    PacketD (event->pak);
                    free (event);
                    break;

                default:
                    /* Store the event in the recv queue for handling later */            
                    QueueEnqueueData (queue, sess, seq_in, QUEUE_TYPE_TCP_RECEIVE,
                                      0, 0,
                                      pak, (UBYTE *)cont, &TCPCallBackReceive);

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
        M_print (i18n (850, "Timeout on connection with %s at %s:%d\n"),
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

    event->sess->connect++;
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

    if (prG->verbose & 4)
    {
        Time_Stamp ();
        M_print (" \x1b«" COLSERV "");
        M_print (i18n (778, "Incoming TCP packet:"));
        M_print (COLNONE "*\n");
        Hex_Dump (pak->data, pak->len);
        M_print (ESC "»\r");
    }
    
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
    
    if (!(sess->connect & CONNECT_MASK))
        return;

    if (prG->verbose & 4)
    {
        Time_Stamp ();
        M_print (" \x1b«" COLCLIENT "");
        M_print (i18n (776, "Outgoing TCP packet:"));
        M_print (COLNONE "\n");
        Hex_Dump (pak->data, pak->len);
        M_print (ESC "»\r");
    }

    tpak = PacketClone (pak);
    switch (PacketReadAt1 (tpak, 0))
    {
        case TCP_CMD_INIT:
        case TCP_CMD_INIT_ACK:
        case TCP_CMD_ACK:
            break;

        default:
            Encrypt_Pak (tpak);
    }
    
    data = (void *) &tpak->data;

    for (;;)
    {
        errno = 0;

        buf[0] = tpak->len & 0xFF;
        buf[1] = tpak->len >> 8;
        if (sockwrite (sess->sok, buf, 2) < 2)
            break;

        if (prG->verbose & 4)
        {
            Time_Stamp ();
            M_print (" \x1b«" COLCLIENT "");
            M_print (i18n (776, "Outgoing TCP packet:"));
            M_print (COLNONE "*\n");
            Hex_Dump (data, tpak->len);
            M_print (ESC "»\r");
        }

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
        M_print (i18n (835, "Error while writing to socket - %s (%d)\n"),
                 strerror (rc), rc);
    }
    TCPClose (sess);
}

/*
 * Sends a TCP initialization packet.
 */
void TCPSendInit (Session *sess)
{
    Packet *pak;
    
    ASSERT_DIRECT (sess);

    if (!(sess->connect & CONNECT_MASK))
        return;

    if (!sess->our_session)
        sess->our_session = rand ();

    if (prG->verbose)
        M_print (i18n (836, "Sending TCP direct connection initialization packet.\n"));

    pak = PacketC ();
    PacketWrite1 (pak, TCP_CMD_INIT);                 /* command          */
    PacketWrite2 (pak, TCP_VER);                      /* TCP version      */
    PacketWrite2 (pak, TCP_VER_REV);                  /* TCP revision     */
    PacketWrite4 (pak, sess->uin);                    /* destination UIN  */
    PacketWrite2 (pak, 0);                            /* unknown - zero   */
    PacketWrite4 (pak, sess->assoc->port);            /* our port         */
    PacketWrite4 (pak, sess->assoc->assoc->uin);             /* our UIN          */
    PacketWrite4 (pak, sess->assoc->assoc->our_outside_ip);  /* our (remote) IP  */
    PacketWrite4 (pak, sess->assoc->assoc->our_local_ip);    /* our (local)  IP  */
    PacketWrite1 (pak, TCP_OK_FLAG);                  /* connection type  */
    PacketWrite4 (pak, sess->assoc->port);            /* our (other) port */
    PacketWrite4 (pak, sess->our_session);            /* session id       */

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
        M_print (i18n (837, "Acknowledging TCP direct connection initialization packet.\n"));

    pak = PacketC ();
    PacketWrite2 (pak, TCP_CMD_INIT_ACK);
    PacketWrite2 (pak, 0);
    TCPSendPacket (pak, sess);
}

Session *TCPReceiveInit (Session *sess, Packet *pak)
{
    Contact *cont;
    UDWORD uin, sid, size;
    Session *peer;

    ASSERT_DIRECT (sess);

    if (!pak)
        return sess;

    size = pak->len;

    for (;;)
    {
        if (size < 26)
            break;
    
        uin  = PacketReadAt4 (pak, 15);
        cont = ContactFind (uin);
        sid  = PacketReadAt4 (pak, 32);

        if (!cont)
            break;

        if (prG->verbose)
        {
            M_print (i18n (838, "Received direct connection initialization.\n"));
            M_print ("    \x1b«");
            M_print (i18n (839, "Version %04x:%04x, Port %d, UIN %d, session %08x\n"),
                     PacketReadAt2 (pak, 1), PacketReadAt2 (pak, 3),
                     PacketReadAt4 (pak, 11), PacketReadAt4 (pak, 15),
                     PacketReadAt4 (pak, 32));
            M_print ("\x1b»\n");
        }

        /* assume new connection is spoofed */
        if ((peer = SessionFind (TYPE_DIRECT, uin)))
        {
            if (peer != sess)
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
            else if (sess->our_session != sid)
                break;
        }
        sess->uin = uin;
        sess->our_session = sid;
        
        PacketD (pak);
        return sess;
    }
    if (prG->verbose)
        M_print (i18n (840, "Received malformed direct connection initialization.\n"));

    TCPClose (sess);
    return sess;
}

/*
 * Receives the acknowledge packet for the initialization packet.
 */
void TCPReceiveInitAck (Session *sess, Packet *pak)
{
    ASSERT_DIRECT (sess);
    
    if (pak && (pak->len != 4 || PacketReadAt1 (pak, 0) != TCP_CMD_INIT_ACK))
    {
        M_print (i18n (841, "Received malformed initialization acknowledgement packet.\n"));
        TCPClose (sess);
    }
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
            M_print (i18n (842, "Closing socket %d to %s.\n"), sess->sok, ContactFindName (sess->uin));
        else
            M_print (i18n (843, "Closing socket %d.\n"), sess->sok);
    }

    if (sess->sok != -1)
        close (sess->sok);
    sess->sok     = -1;
    sess->connect = 0;
    sess->our_session = 0;
    if (!sess->uin)
        SessionClose (sess);
}


/*
 * Sends a message via TCP.
 * Adds it to the resend queue until acked.
 */
BOOL TCPSendMsg (Session *sess, UDWORD uin, char *msg, UWORD sub_cmd)
{
    Contact *cont;
    int msgtype;
    Packet *pak;
    Session *peer;

    if (!sess)
        return 0;
    cont = ContactFind (uin);
    if (!cont)
        return 0;
    if (cont->status == STATUS_OFFLINE)
        return 0;
    if (!(sess->connect & CONNECT_MASK))
        return 0;
    if (!cont->port)
        return 0;
    if (!cont->local_ip && !cont->outside_ip)
        return 0;

    ASSERT_PEER (sess);
    
    TCPDirectOpen (sess, uin);
    peer = SessionFind (TYPE_DIRECT, uin);
    
    if (!peer)
        return 0;

    switch (sess->status)
    {
        case STATUS_AWAY:
            msgtype = TCP_MSGF_AWAY;
            break;
 
        case STATUS_NA:
        case STATUS_NA_99:
            msgtype = TCP_MSGF_NA;
            break;

        case STATUS_OCCUPIED:
        case STATUS_OCCUPIED_MAC:
            msgtype = TCP_MSGF_OCC;
            break;
            
        case STATUS_DND:
        case STATUS_DND_99:
            msgtype = TCP_MSGF_DND;
            break;

        case STATUS_INVISIBLE:
            msgtype = TCP_MSGF_INV;
            break;

        default:
            msgtype = 0;    
    }
    msgtype ^= TCP_MSG_LIST;

    pak = PacketC ();
    PacketWrite4 (pak, 0);               /* checksum - filled in later */
    PacketWrite2 (pak, TCP_CMD_MESSAGE); /* command                    */
    PacketWrite2 (pak, TCP_MSG_X1);      /* unknown                    */
    PacketWrite2 (pak, peer->our_seq);   /* sequence number            */
    PacketWrite4 (pak, 0);               /* unknown                    */
    PacketWrite4 (pak, 0);               /* unknown                    */
    PacketWrite4 (pak, 0);               /* unknown                    */
    PacketWrite2 (pak, sub_cmd);         /* message type               */
    PacketWrite2 (pak, 0);               /* status - filled in later   */
    PacketWrite2 (pak, msgtype);         /* our status                 */
    PacketWriteStrCUW (pak, msg);        /* the message                */
    PacketWrite4 (pak, TCP_COL_FG);      /* foreground color           */
    PacketWrite4 (pak, TCP_COL_BG);      /* background color           */

    QueueEnqueueData (queue, peer, peer->our_seq--, QUEUE_TYPE_TCP_RESEND,
                      uin, time (NULL),
                      pak, strdup (MsgEllipsis (msg)), &TCPCallBackResend);

    if (!(peer->connect & CONNECT_OK))
    {
        Time_Stamp ();
        M_print (" " COLSENT "%10s" COLNONE " ... %s\n", cont->nick, MsgEllipsis (msg));
    }
    return 1;
}

/*
 * Resends TCP packets if necessary
 */
void TCPCallBackResend (struct Event *event)
{
    Packet *pak;
    Contact *cont;
    Session *peer = event->sess;
    
    ASSERT_DIRECT (peer);

    pak  = event->pak;
    cont = ContactFind (event->uin);

    if (event->attempts >= MAX_RETRY_ATTEMPTS)
        TCPClose (peer);

    if (peer->connect & CONNECT_MASK)
    {
        if (peer->connect & CONNECT_OK)
        {
            Time_Stamp ();
            M_print (" " COLCONTACT "%10s" COLNONE " %s%s\n", cont->nick, MSGTCPSENTSTR, event->info);

            event->attempts++;
            TCPSendPacket (pak, peer);
            event->due = time (NULL) + 10;
        }
        else
            event->due = time (NULL) + 1;
        QueueEnqueue (queue, event);
        return;
    }

    if (event->attempts < MAX_RETRY_ATTEMPTS && PacketReadAt2 (pak, 4) == TCP_CMD_MESSAGE)
    {
        peer->assoc->connect = CONNECT_FAIL;
        icq_sendmsg (peer->assoc->assoc, cont->uin, strdup (PacketReadAtStrN (pak, 28)), PacketReadAt2 (pak, 26));
    }
    else
        M_print (i18n (844, "TCP message %04x discarded after timeout.\n"), PacketReadAt2 (pak, 4));
    
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
    Packet *pak;
    const char *tmp;
    
    ASSERT_DIRECT (event->sess);
    
    pak = event->pak;
    cont = (Contact *) event->info;
    
    switch (PacketReadAt2 (pak, 26))
    {
        /* Requests for auto-response message */
        case TCP_CMD_GET_AWAY:
        case TCP_CMD_GET_OCC:
        case TCP_CMD_GET_NA:
        case TCP_CMD_GET_DND:
        case TCP_CMD_GET_FFC:
            M_print (i18n (814, "Sent auto-response message to %s%s%s.\n"),
                     COLCONTACT, cont->nick, COLNONE);
            Send_TCP_Ack (event->sess, PacketReadAt2 (pak, 8), PacketReadAt2 (pak, 22), TRUE);
            break;

        /* Automatically reject file xfer and chat requests
             as these are not implemented yet. */ 
        case CHAT_MESS:
        case FILE_MESS:
            Send_TCP_Ack (event->sess, PacketReadAt2 (pak, 8), PacketReadAt2 (pak, 22), FALSE);
            break;

        /* Regular messages */
        default:
            uiG.last_rcvd_uin = cont->uin;
            
            Time_Stamp ();
            M_print ("\a " CYAN BOLD "%10s" COLNONE " ", ContactFindName (cont->uin));
            
            tmp = PacketReadAtStrN (pak, 28);
            Do_Msg (event->sess, PacketReadAt2 (pak, 24), strlen (tmp),
                    tmp, cont->uin, 1);

            log_event (cont->uin, LOG_MESS, "You received a TCP message from %s\n%s\n",
                       ContactFindName (cont->uin), tmp);

            Send_TCP_Ack (event->sess, PacketReadAt2 (pak, 8),
                          PacketReadAt2 (pak, 22), TRUE);
    }
    PacketD (pak);
    free (event);
}



/*
 * Requests the auto-response message
 * from remote user.
 */
void Get_Auto_Resp (Session *sess, UDWORD uin)
{
    Contact *cont;
    UWORD sub_cmd;
    char *msg = "...";    /* seem to need something (anything!) in the msg */

    cont = ContactFind (uin);
    
    assert (cont);
    ASSERT_DIRECT (sess);

    switch (cont->status & 0x1ff)
    {
        case STATUS_NA:
        case STATUS_NA_99:
            sub_cmd = TCP_CMD_GET_NA;
            break;

        case STATUS_FREE_CHAT:
            sub_cmd = TCP_CMD_GET_FFC;
            break;

        case STATUS_OCCUPIED:
        case STATUS_OCCUPIED_MAC:
            sub_cmd = TCP_CMD_GET_OCC;
            break;
        
        case STATUS_AWAY:
            sub_cmd = TCP_CMD_GET_AWAY;
            break;
 
        case STATUS_DND:
        case STATUS_DND_99:
            sub_cmd = TCP_CMD_GET_DND;
            break;

        case STATUS_OFFLINE:
        case STATUS_ONLINE:
        case STATUS_INVISIBLE:
        default:
            sub_cmd = 0;
    }

    if (cont->connection_type != TCP_OK_FLAG)
    {
        sub_cmd = 0;
    }

    if (sub_cmd == 0)
        M_print (i18n (809, "Unable to retrieve auto-response message for %s."),
                 ContactFindName (uin));
    else
        TCPSendMsg (sess, uin, msg, sub_cmd);
}

/*
 * Acks a TCP packet.
 */
int Send_TCP_Ack (Session *sess, UWORD seq, UWORD sub_cmd, BOOL accept)
{
    Packet *pak;
    int size;
    char *msg;

    ASSERT_DIRECT (sess);
    msg = Get_Auto_Reply (sess);
 
    size = TCP_MSG_OFFSET + strlen(msg) + 1 + 8;    /* header + msg + null + tail */

    pak = PacketC ();
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
    PacketWriteStrCUW (pak, msg);        /* the message                */
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
void Encrypt_Pak (Packet *pak)
{
    UDWORD B1, M1, check, size;
    int i;
    UBYTE X1, X2, X3, *p;
    UDWORD hex, key;

    p = pak->data;
    size = pak->len;

    /* calculate verification data */
    M1 = (rand() % ((size < 255 ? size : 255)-10))+10;
    X1 = p[M1] ^ 0xFF;
    X2 = rand() % 220;
    X3 = client_check_data[X2] ^ 0xFF;
    B1 = (p[4]<<24) | (p[6]<<16) | (p[4]<<8) | (p[6]);

    /* calculate checkcode */
    check = (M1 << 24) | (X1 << 16) | (X2 << 8) | X3;
    check ^= B1;

    /* main XOR key */
    key = 0x67657268 * size + check;

    /* XORing the actual data */
    for (i = 0; i < (size+3) / 4; i += 4)
    {
        hex = key + client_check_data[i&0xFF];
        p[i+0] ^= hex & 0xFF;
        p[i+1] ^= (hex>>8) & 0xFF;
        p[i+2] ^= (hex>>16) & 0xFF;
        p[i+3] ^= (hex>>24) & 0xFF;
    }

    /* storing the checkcode */
    PacketWriteAt4 (pak, 0, check);
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
 
    for (i = 4; i < (size+3) / 4; i += 4) 
    {
        hex = key + client_check_data[i&0xFF];
        pak[i+0] ^= hex & 0xFF;
        pak[i+1] ^= (hex>>8) & 0xFF;
        pak[i+2] ^= (hex>>16) & 0xFF;
        pak[i+3] ^= (hex>>24) & 0xFF;
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
