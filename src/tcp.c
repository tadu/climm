/*
 * This file handles TCP client-to-client communications.
 *
 *  Author: James Schofield (jschofield@ottawa.com) 22 Feb 2001
 *  Lots of changes from Rüdiger Kuhlmann.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

#include "micq.h"
#include "util_ui.h"
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

#define BACKLOG 10

#ifdef TCP_COMM

static Packet    *TCPReceivePacket  (tcpsock_t *sok);
static void       TCPSendPacket     (Packet *pak, tcpsock_t *sok);
static void       TCPClose          (tcpsock_t *sok);

static void       TCPSendInit       (Session *sess, tcpsock_t *sok);
static void       TCPSendInitAck    (tcpsock_t *sok);
static tcpsock_t *TCPReceiveInit    (tcpsock_t *sok);
static void       TCPReceiveInitAck (tcpsock_t *sok);

static void       TCPCallBackResend  (struct Event *event);
static void       TCPCallBackReceive (struct Event *event);

static void       Encrypt_Pak        (Packet *pak);
static int        Decrypt_Pak        (UBYTE *pak, UDWORD size);


void SessionInitPeer (Session *sess)
{
    /* NOTE: we still (ab)use the UDP (server) session!!! */
    TCPInit (sess->assoc, sess->spref->port);
}

/*
 * Callback that triggers "login" - ehm, listening.
 */
void CallBackLoginTCP (struct Event *event)
{
    TCPInit (event->sess, TCP_PORT);
    free (event);
}

/* Initializes TCP socket for incoming connections on given port.
 * 0 = random port
 */
void TCPInit (Session *sess, int port)
{
    int length;
    struct sockaddr_in sin;

    sess->tcpsok = socket (AF_INET, SOCK_STREAM, 0);
    if (sess->tcpsok == -1)
    {
        perror ("Error creating TCP socket.");
        exit (1);
    }

    sin.sin_family = AF_INET;
    sin.sin_port = htons (port);
    sin.sin_addr.s_addr = INADDR_ANY;
    sess->seq_tcp = -1;

    if (bind (sess->tcpsok, (struct sockaddr*)&sin, sizeof (struct sockaddr)) < 0)
    {
        perror ("Can't bind socket to free port.");
        sess->tcpsok = -1;
        return;
    }

    if (listen (sess->tcpsok, BACKLOG) < 0)
    {
        perror ("Unable to listen on TCP socket");
        sess->tcpsok = -1;
        return;
    }

    /* At least copy the socket to the right Session, so it is listened to */
    sess->assoc->sok = sess->tcpsok;
    /* Get the port used -- needs to be sent in login packet to ICQ server */
    length = sizeof (struct sockaddr);
    getsockname (sess->tcpsok, (struct sockaddr *) &sin, &length);
    sess->our_port = ntohs (sin.sin_port);
    M_print (i18n (777, "The port that will be used for tcp (partially implemented) is %d\n"),
             sess->our_port);
}

/*
 * Adds sockets to the list of checked sockets.
 */
void TCPAddSockets (Session *sess)
{
    Contact *cont;

    for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
        if (cont->sok.state > 0)
        {
            if (cont->sok.state < TCP_STATE_CONNECTED)
                M_Add_wsocket (cont->sok.sok);
            else
                M_Add_rsocket (cont->sok.sok);
            M_Add_xsocket (cont->sok.sok);
        }
}

/*
 * Dispatches on active sockets.
 */
void TCPDispatch (Session *sess)
{
    Contact *cont;

    if (sess && M_Is_Set (sess->tcpsok))
        TCPDirectReceive (sess);
    for (cont = ContactStart (); ContactHasNext (cont); cont = ContactNext (cont))
        if (cont->sok.state > 0)
            if (M_Is_Set (cont->sok.sok))
                TCPHandleComm (sess, cont, M_Is_Set (cont->sok.sok));
}

/*
 *  Starts establishing a TCP connection to given contact.
 */
void TCPDirectOpen (Session *sess, Contact *cont)
{
    if (cont->sok.state == TCP_STATE_ESTABLISHED)
        return;

    TCPClose (&cont->sok);
    TCPConnect (sess, &cont->sok, 0);
}

/*
 * Closes TCP message connection to given contact.
 */
void TCPDirectClose (Contact *cont)
{
    TCPClose (&cont->sok);
}

/*
 * Handles timeout on TCP connect/send/read/whatever
 */
void TCPCallBackTimeout (struct Event *event)
{
    tcpsock_t *sok;
    struct sockaddr_in sin;

    assert (event->type == QUEUE_TYPE_TCP_TIMEOUT);
    
    sok = (tcpsock_t *)event->pak;
    sin.sin_addr.s_addr = sok->ip;

    if (sok->state == TCP_STATE_ESTABLISHED)
    {
        M_print (i18n (850, "Timeout on connection with %s at %s:%d\n"), sok->cont->nick, inet_ntoa (sin.sin_addr), sok->cont->port);
        TCPClose (sok);
    }
    else
        TCPConnect (event->sess, sok, 0);
}

/*
 * Creates a nonblocked TCP socket.
 */
int TCPSocket (tcpsock_t *sok)
{
    int rc, flags;

    sok->sok = socket (AF_INET, SOCK_STREAM, 0);
    if (sok->sok <= 0)
    {
        rc = errno;
        M_print (i18n (780, "Couldn't create socket: %s (%d).\n"), strerror (rc), rc);
        sok->state = 0;
        return 0;
    }

    flags = fcntl (sok->sok, F_GETFL, 0);
    if (flags != -1)
        flags = fcntl (sok->sok, F_SETFL, flags | O_NONBLOCK);
    if (flags == -1 && prG->verbose)
    {
        rc = errno;
        M_print (i18n (828, "Warning: couldn't set socket nonblocking: %s (%d).\n"), strerror (rc), rc);
    }
    return 1;
}

/*
 *  Does the next step establishing a TCP connection to given contact.
 *  On error, returns -1.
 */
int TCPConnect (Session *sess, tcpsock_t *sok, int mode)
{
    struct sockaddr_in sin;
    int flags, rc;
    
    while (1)
    {
        if (prG->verbose)
            M_print ("debug: TCPConnect; Nick: %s mode: %d state: %d\n", sok->cont ? sok->cont->nick : "", mode, sok->state);
        switch (sok->state)
        {
            case -1:
                return -1;
            case 0:
                if (!TCPSocket (sok))
                    return -1;

                sin.sin_family = AF_INET;
                sin.sin_port = htons (sok->cont->port);
                sin.sin_addr.s_addr = sok->ip = Chars_2_DW (sok->cont->current_ip);

                rc = connect (sok->sok, (struct sockaddr *) &sin, sizeof (struct sockaddr));
                if (rc >= 0)
                {
                    sok->state = TCP_STATE_CONNECTED;
                    break;
                }
                if ((rc = errno) == EINPROGRESS)
                {
                    QueueEnqueueData (queue, sess, sok->ip, QUEUE_TYPE_TCP_TIMEOUT,
                                      sok->cont->uin, time (NULL) + 10,
                                      (Packet *)sok, NULL, &TCPCallBackTimeout);
                    sok->state = 1;
                    return 0;
                }
                sok->state = 2;
                break;
            case 1:
                rc = 0;
                flags = sizeof (int);
                if (getsockopt (sok->sok, SOL_SOCKET, SO_ERROR, &rc, &flags) < 0)
                    rc = errno;
                if (!mode)
                    rc = ETIMEDOUT;
                if (!rc)
                {
                    sok->state = TCP_STATE_CONNECTED;
                    break;
                }
                QueueDequeue (queue, sok->ip, QUEUE_TYPE_TCP_TIMEOUT);
            case 2:
                close (sok->sok);
                if (prG->verbose)
                {
                    M_print (i18n (829, "Opening TCP connection to %s%s%s at %s:%d failed: %d (%s) (%d)\n"),
                             COLCONTACT, sok->cont->nick, COLNONE, inet_ntoa (sin.sin_addr), sok->cont->port, rc, strerror (rc), 1);
                }

                if (!TCPSocket (sok))
                    return -1;

                sin.sin_family = AF_INET;
                sin.sin_port = htons (sok->cont->port);
                sin.sin_addr.s_addr = sok->ip = Chars_2_DW (sok->cont->other_ip);

                rc = connect (sok->sok, (struct sockaddr *) &sin, sizeof (struct sockaddr));
                if (rc >= 0)
                {
                    sok->state = TCP_STATE_CONNECTED;
                    break;
                }
                if ((rc = errno) == EINPROGRESS)
                {
                    QueueEnqueueData (queue, sess, sok->ip, QUEUE_TYPE_TCP_TIMEOUT,
                                      sok->cont->uin, time (NULL) + 10,
                                      (Packet *)sok, NULL, &TCPCallBackTimeout);
                    sok->state = 3;
                    return 0;
                }
                sok->state = 4;
                break;
            case 3:
                rc = 0;
                flags = sizeof (int);
                if (getsockopt (sok->sok, SOL_SOCKET, SO_ERROR, &rc, &flags) < 0)
                    rc = errno;
                if (!mode)
                    rc = ETIMEDOUT;
                if (!rc)
                {
                    sok->state = TCP_STATE_CONNECTED;
                    break;
                }
                QueueDequeue (queue, sok->ip, QUEUE_TYPE_TCP_TIMEOUT);
            case 4:
            {
                CmdPktCmdTCPRequest (sess, sok->cont->uin, sok->cont->port);
                QueueEnqueueData (queue, sess, sok->ip, QUEUE_TYPE_TCP_TIMEOUT,
                                  sok->cont->uin, time (NULL) + 30,
                                  (Packet *)sok, NULL, &TCPCallBackTimeout);
                close (sok->sok);
                sok->sok = 0;
                sok->state = TCP_STATE_WAITING;
                return 0;
            }
            case TCP_STATE_WAITING:               
                QueueDequeue (queue, sok->ip, QUEUE_TYPE_TCP_TIMEOUT);
                M_print (i18n (855, "TCP connection to %s at %s:%d failed.\n") , sok->cont->nick, inet_ntoa (sin.sin_addr), sok->cont->port);
                sok->state = -1;
                sok->sok = 0;
                return -1;
            case TCP_STATE_CONNECTED:
                QueueDequeue (queue, sok->ip, QUEUE_TYPE_TCP_TIMEOUT);
                sin.sin_addr.s_addr = sok->ip;
                if (prG->verbose)
                {
                    M_print (i18n (779, "Opening TCP connection to %s at %s:%d... ") , sok->cont->nick, inet_ntoa (sin.sin_addr), sok->cont->port);
                    M_print (i18n (785, "success.\n"));
                }
                flags = fcntl (sok->sok, F_GETFL, 0);
                if (flags != -1)
                    flags = fcntl (sok->sok, F_SETFL, flags & ~O_NONBLOCK);
                if (flags == -1)
                {
                    M_print (i18n (779, "Opening TCP connection to %s at %s:%d... ") , sok->cont->nick, inet_ntoa (sin.sin_addr), sok->cont->port);
                    M_print (i18n (832, "failure - couldn't set socket blocking.\n"));
                    TCPClose (sok);
                    return -1;
                }
                sok->state = 7;
                TCPSendInit (sess, sok);
                return 0;
            case 7:
                if (!mode)
                {
                    TCPClose (sok);
                    return -1;
                }
                sok->state = 8;
                TCPReceiveInitAck (sok);
                return 0;
            case 8:
                if (!mode)
                {
                    TCPClose (sok);
                    return -1;
                }
                sok->state = 31;
                TCPReceiveInit (sok);
                TCPSendInitAck (sok);
                break;
            case 10:
                sok = TCPReceiveInit (sok);
                if (!sok)
                    return -1;
            case 11:
                sok->state = 12;
                TCPSendInitAck (sok);
                TCPSendInit (sess, sok);
                return 0;
            case 12:
                if (!mode)
                {
                    TCPClose (sok);
                    return -1;
                }
                sok->state = 31;
                TCPReceiveInitAck (sok);
                break;
            case 31:
                Time_Stamp ();
                M_print (" ");
                M_print (i18n (833, "Client-to-client TCP connection to %s established.\n"), sok->cont->nick);
                sok->state = TCP_STATE_ESTABLISHED;
                return 0;
            default:
                assert (0);
        }
    }
}

/*
 *  Receives an incoming TCP packet and returns size of packet.
 *  Resets socket on error. Paket must be free()d.
 */
Packet *TCPReceivePacket (tcpsock_t *sok)
{
    UBYTE buf[2];
    int rc, size, todo, bytesread = 0;
    void *get;
    Packet *pak;

    if (sok->state <= 0)
        return 0;

    for (;;)
    {
        errno = 0;
        pak = NULL;

        bytesread = sockread (sok->sok, buf, 2);
        if (bytesread < 2)
            break;

        size = Chars_2_Word (buf);
        if (size < 0 || size > PacketMaxData)
        {
            errno = ENOMEM;
            break;
        }
        
        pak = PacketC ();

        for (get = pak->data, todo = size; todo > 0; todo -= bytesread, get += bytesread)
        {
            bytesread = sockread (sok->sok, get, todo);
            if (bytesread <= 0)
                break;
        }
        if (bytesread <= 0)
            break;

        if (prG->verbose & 4)
        {
            Time_Stamp ();
            M_print (" \x1b«" COLSERV "");
            M_print (i18n (778, "Incoming TCP packet:"));
            M_print (COLNONE "*\n");
            Hex_Dump (pak->data, size);
            M_print ("\x1b»\n");
        }

        pak->len = size;
        return pak;
    }
    
    if (pak)
        PacketD (pak);

    if (prG->verbose)
    {
        rc = errno;
        Time_Stamp ();
        M_print (" ");
        M_print (i18n (834, "Error while reading from socket - %s (%d)\n"),
                 strerror (rc), rc);
    }
    TCPClose (sok);
    return NULL;
}

/*
 * Encrypts and sends a TCP packet.
 * Resets socket on error.
 */
void TCPSendPacket (Packet *pak, tcpsock_t *sok)
{
    Packet *tpak;
    void *data;
    UBYTE buf[2];
    int rc, todo, bytessend = 0;
    
    if (sok->state <= 0)
        return;

    if (prG->verbose & 4)
    {
        Time_Stamp ();
        M_print (" \x1b«" COLCLIENT "");
        M_print (i18n (776, "Outgoing TCP packet:"));
        M_print (COLNONE "\n");
        Hex_Dump (pak->data, pak->len);
        M_print ("\x1b»\n");
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
        if (sockwrite (sok->sok, buf, 2) < 2)
            break;

        if (prG->verbose & 4)
        {
            Time_Stamp ();
            M_print (" \x1b«" COLCLIENT "");
            M_print (i18n (776, "Outgoing TCP packet:"));
            M_print (COLNONE "*\n");
            Hex_Dump (data, tpak->len);
            M_print ("\x1b»\n");
        }

        for (todo = tpak->len; todo > 0; todo -= bytessend, data += bytessend)
        {
            bytessend = sockwrite (sok->sok, data, todo);
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
    TCPClose (sok);
}

/*
 * Sends a TCP initialization packet.
 */
void TCPSendInit (Session *sess, tcpsock_t *sok)
{
    Packet *pak;

    if (sok->state <= 0)
        return;

    if (!sok->sid)
        sok->sid = rand ();

    if (prG->verbose)
        M_print (i18n (836, "Sending TCP direct connection initialization packet.\n"));

    pak = PacketC ();
    PacketWrite1 (pak, TCP_CMD_INIT);                 /* command          */
    PacketWrite2 (pak, TCP_VER);                      /* TCP version      */
    PacketWrite2 (pak, TCP_VER_REV);                  /* TCP revision     */
    PacketWrite4 (pak, sok->cont->uin);               /* destination UIN  */
    PacketWrite2 (pak, 0);                            /* unknown - zero   */
    PacketWrite4 (pak, sess->our_port);               /* our port         */
    PacketWrite4 (pak, sess->uin);                    /* our UIN          */
    PacketWrite4 (pak, htonl (sess->our_outside_ip)); /* our (remote) IP  */
    PacketWrite4 (pak, htonl (sess->our_local_ip));   /* our (local)  IP  */
    PacketWrite1 (pak, TCP_OK_FLAG);                  /* connection type  */
    PacketWrite4 (pak, sess->our_port);               /* our (other) port */
    PacketWrite4 (pak, sok->sid);                     /* session id       */

    TCPSendPacket (pak, sok);
}

/*
 * Sends the initialization packet
 */
void TCPSendInitAck (tcpsock_t *sok)
{
    Packet *pak;

    if (sok->state <= 0)
        return;

    if (prG->verbose)
        M_print (i18n (837, "Acknowledging TCP direct connection initialization packet.\n"));

    pak = PacketC ();
    PacketWrite2 (pak, TCP_CMD_INIT_ACK);
    PacketWrite2 (pak, 0);
    TCPSendPacket (pak, sok);
}

tcpsock_t *TCPReceiveInit (tcpsock_t *sok)
{
    Packet *pak;
    Contact *cont;
    UDWORD sid, size;

    if (sok->state <= 0)
        return sok;

    pak = TCPReceivePacket (sok);
    
    if (!pak)
        return sok;

    size = pak->len;

    for (;;)
    {
        if (size < 26)
            break;
    
        cont = ContactFind (PacketReadAt4 (pak, 15));
        sid  = PacketReadAt4 (pak, 32);

        if (sok->cont && sok->cont != cont)
            break;

        if (!cont)
            break;

        /* assume new connection is spoofed */
        if (cont->sok.state == TCP_STATE_ESTABLISHED)
        {
            TCPClose (sok);
            return NULL;
        }
        
        if (cont->sok.state == TCP_STATE_WAITING)
            QueueDequeue (queue, cont->sok.ip, QUEUE_TYPE_TCP_TIMEOUT);

        cont->sok = *sok;
        sok = &cont->sok;
        
        if (!sok->sid)
        sok->cont = cont;
        sok->sid = sid;

        if (sok->sid != sid)
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

        PacketD (pak);
        return sok;
    }
    if (prG->verbose)
        M_print (i18n (840, "Received malformed direct connection initialization.\n"));

    TCPClose (sok);
    return sok;
}

/*
 * Receives the acknowledge packet for the initialization packet.
 */
void TCPReceiveInitAck (tcpsock_t *sok)
{
    Packet *pak;

    if (sok->state <= 0)
        return;

    pak = TCPReceivePacket (sok);

    if (pak && (pak->len != 4 || PacketReadAt1 (pak, 0) != TCP_CMD_INIT_ACK))
    {
        M_print (i18n (841, "Received malformed initialization acknowledgement packet.\n"));
        TCPClose (sok);
    }
}

/*
 * Close socket and mark as inactive. If verbose, complain.
 */
void TCPClose (tcpsock_t *sok)
{
    if (sok->state > 0)
    {
        Time_Stamp ();
        M_print (" ");
        if (sok->cont)
            M_print (i18n (842, "Closing socket %d to %s.\n"), sok->sok, sok->cont->nick);
        else
            M_print (i18n (843, "Closing socket %d.\n"), sok->sok);
    }

    if (sok->state > 0)
        close (sok->sok);
    sok->sok   = 0;
    sok->state = 0;
    sok->sid   = 0;
}

/*
 * Accepts a new direct connection.
 */
void TCPDirectReceive (Session *sess)
{
    tcpsock_t sock;
    struct sockaddr_in sin;
    int tmp;
 
    tmp = sizeof (sin);

    sock.sok   = accept (sess->tcpsok, (struct sockaddr *)&sin, &tmp);
    
    if (sock.sok <= 0)
        return;
    
    sock.state = 10;
    sock.sid   = 0;
    sock.cont  = NULL;
    
    TCPConnect (sess, &sock, 0);
}

/*
 * Handles activity on socket for given contact.
 */
void TCPHandleComm (Session *sess, Contact *cont, int mode)
{
    if (cont->sok.state > 0 && cont->sok.state < TCP_STATE_ESTABLISHED)
        TCPConnect (sess, &cont->sok, mode);
    else
    {
        if (prG->verbose) M_print ("Debug: TCPHandleComm: Nick: %s, mode: %d\n", cont?cont->nick:"",mode);
        if (mode&1)
        Handle_TCP_Comm (sess, cont->uin);
    }
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

    cont = ContactFind (uin);

    if (!cont)
        return 0;
    if (cont->status == STATUS_OFFLINE)
        return 0;
    if (cont->sok.state < 0)
        return 0;

    if (!cont->sok.state)
        TCPDirectOpen (sess, cont);

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
    PacketWrite2 (pak, sess->seq_tcp);   /* sequence number            */
    PacketWrite4 (pak, 0);               /* unknown                    */
    PacketWrite4 (pak, 0);               /* unknown                    */
    PacketWrite4 (pak, 0);               /* unknown                    */
    PacketWrite2 (pak, sub_cmd);         /* message type               */
    PacketWrite2 (pak, 0);               /* status - filled in later   */
    PacketWrite2 (pak, msgtype);         /* our status                 */
    PacketWriteStrCUW (pak, msg);        /* the message                */
    PacketWrite4 (pak, TCP_COL_FG);      /* foreground color           */
    PacketWrite4 (pak, TCP_COL_BG);      /* background color           */

    QueueEnqueueData (queue, sess, sess->seq_tcp--, QUEUE_TYPE_TCP_RESEND,
                      cont->uin, time (NULL),
                      pak, strdup (MsgEllipsis (msg)), &TCPCallBackResend);

    if (cont->sok.state != TCP_STATE_ESTABLISHED)
    {
        Time_Stamp ();
        M_print (" " COLSENT "%10s" COLNONE " ... %s\n", ContactFindName (cont->uin), MsgEllipsis (msg));
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
    tcpsock_t *sok;

    pak  = event->pak;
    cont = ContactFind (event->uin);
    sok  = &cont->sok;

    if (event->attempts >= MAX_RETRY_ATTEMPTS)
        TCPClose (sok);

    if (sok->state && sok->state != TCP_STATE_FAILED)
    {
        if (sok->state == TCP_STATE_ESTABLISHED)
        {
            Time_Stamp ();
            M_print (" " COLCONTACT "%10s" COLNONE " %s%s\n", cont->nick, MSGTCPSENTSTR, event->info);

            event->attempts++;
            TCPSendPacket (pak, sok);
            event->due = time (NULL) + 10;
        }
        else
            event->due = time (NULL) + 1;
        QueueEnqueue (queue, event);
        return;
    }

    if (event->attempts < MAX_RETRY_ATTEMPTS && PacketReadAt2 (pak, 4) == TCP_CMD_MESSAGE)
        CmdPktCmdSendMessage (event->sess, cont->uin, PacketReadAtStrN (event->pak, 28),
                              PacketReadAt2 (pak, 26));
    else
        M_print (i18n (844, "TCP message %04x discarded after timeout.\n"), PacketReadAt2 (pak, 4));
    
    PacketD (event->pak);
    free (event->info);
    free (event);
}



/*
 * Handles all incoming TCP traffic.
 * Queues incoming packets, ignoring resends
 * and deleting CANCEL requests from the queue.
 */
void Handle_TCP_Comm (Session *sess, UDWORD uin)
{
    Contact *cont;
    Packet *pak;
    struct Event *event;
    int size;
    int i = 0;
    UWORD seq_in = 0;
     
    cont = ContactFind (uin);

    /* Recv all packets before doing anything else.
         The objective is to delete any packets CANCELLED by the remote user. */
    while (M_Is_Set (cont->sok.sok) && i++ <= TCP_MSG_QUEUE)
    {
        if (!(pak = TCPReceivePacket (&cont->sok)))
            break;

        size = pak->len;

        if (Decrypt_Pak ((UBYTE *) &pak->data, size) < 0)
        {
            M_print (i18n (789, "Received malformed packet."));
            TCPClose (&cont->sok);
            break;
        }

        if (prG->verbose & 4)
        {
            Time_Stamp ();
            M_print (" \x1b«" COLSERV "");
            M_print (i18n (778, "Incoming TCP packet:"));
            M_print (COLNONE "\n");
            Hex_Dump (pak->data, size);
            M_print ("\x1b»\n");
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
                        M_print (i18n (194, "Auto-response message for %s:\n"), ContactFindNick (cont->uin));
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

                    sess->seq_tcp--;
                break;
            }
        }
        M_select_init();
        M_set_timeout (0, 100000);
        M_Add_rsocket (cont->sok.sok);
        M_select();
    }
}

/*
 * Handles a just received packet.
 */
void TCPCallBackReceive (struct Event *event)
{
    Contact *cont;
    Packet *pak;
    const char *tmp;
    
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
            Send_TCP_Ack (event->sess, &cont->sok, PacketReadAt2 (pak, 8), PacketReadAt2 (pak, 22), TRUE);
            break;

        /* Automatically reject file xfer and chat requests
             as these are not implemented yet. */ 
        case CHAT_MESS:
        case FILE_MESS:
            Send_TCP_Ack (event->sess, &cont->sok, PacketReadAt2 (pak, 8), PacketReadAt2 (pak, 22), FALSE);
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

            Send_TCP_Ack (event->sess, &cont->sok, PacketReadAt2 (pak, 8),
                          PacketReadAt2 (pak, 22), TRUE);
    }
    PacketD (pak);
    free (event);
}



/*
 * Requests the auto-response message
 * from remote user.
 */
void Get_Auto_Resp (UDWORD uin)
{
    Contact *cont;
    UWORD sub_cmd;
    char *msg = "...";    /* seem to need something (anything!) in the msg */

    cont = ContactFind (uin);
    
    assert (cont);

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
        TCPSendMsg (0, uin, msg, sub_cmd);
}

/*
 * Acks a TCP packet.
 */
int Send_TCP_Ack (Session *sess, tcpsock_t *sok, UWORD seq, UWORD sub_cmd, BOOL accept)
{
    Packet *pak;
    int size;
    char *msg;

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

    TCPSendPacket (pak, sok);

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

/* i18n (798, " ") i18n (783, " ") i18n (786, " ") i18n 
 * i18n (784, " ") i18n (497, " ") i18n (617, " ") i18n
 * i18n (799, " ") i18n (781, " ") i18n (782, " ") i18n
 * i18n (812, " ") i18n (813, " ") i18n
 * i18n (602, " ") i18n (603, " ") i18n (604, " ") i18n (605, " ") i18n
 * i18n (620, " ") i18n (621, " ") i18n (624, " ") i18n (625, " ") i18n (630, " ") i18n
 * i18n (631, " ") i18n (636, " ") i18n (637, " ") i18n (638, " ") i18n
 * i18n (743, " ") i18n (744, " ") i18n (745, " ") i18n (746, " ") i18n (747, " ") i18n
 * i18n (748, " ") i18n (749, " ") i18n (750, " ") i18n (751, " ") i18n (752, " ") i18n
 * i18n (753, " ") i18n (754, " ") i18n (755, " ") i18n
 * i18n (32, " ") i18n (33, " ") i18n (48, " ") i18n (49, " ") i18n (51, " ") i18n
 * i18n (98, " ") i18n (195, " ") i18n
 * i18n (59, " ") i18n (797, " ") i18n
 * i18n (626, " ") i18n (627, " ") i18n
 * i18n (634, " ") i18n (808, " ") i18n (810, " ") i18n (811, " ") i18n
 * i18n (762, " ") i18n (787, " ")
 * i18n (618, " ") i18n (788, " ") i18n (790, " ") i18n (792, " ") i18n (793, " ") i18n
 * i18n (794, " ") i18n (795, " ") i18n (796, " ") i18n
 * i18n (831, " ") i18n (619, " ") i18n (791, " ") i18n
 */
