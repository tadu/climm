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
#include "sendmsg.h"
#include "network.h"
#include "cmd_user.h"
#include "icq_response.h"
#include "server.h"
#include "contact.h"
#include "tcp.h"

#define BACKLOG 10

#ifdef TCP_COMM

static int  TCPReceivePacket  (tcpsock_t *sok, void **pak);
static void TCPSendPacket     (tcpsock_t *sok, void *pak, UWORD size);
static void TCPClose          (tcpsock_t *sok);

static void TCPSendInit       (tcpsock_t *sok);
static void TCPSendInitAck    (tcpsock_t *sok);
static void TCPReceiveInit    (tcpsock_t *sok);
static void TCPReceiveInitAck (tcpsock_t *sok);

static void TCPCallBackResend (int srvsok, struct Event *event);


const UBYTE client_check_data[] = {
    "As part of this software beta version Mirabilis is "
    "granting a limited access to the ICQ network, "
    "servers, directories, listings, information and databases (\""
    "ICQ Services and Information\"). The "
    "ICQ Service and Information may databases (\""
    "ICQ Services and Information\"). The "
    "ICQ Service and Information may\0"
};

struct Queue *tcp_rq = NULL;

/* Initializes TCP socket for incoming connections on given port.
 * 0 = random port
 */
SOK_T TCPInit (int port)
{
    int sok;
    int length;
    struct sockaddr_in sin;

    QueueInit (&tcp_rq);

    sok = socket (AF_INET, SOCK_STREAM, 0);
    if (sok == -1)
    {
        perror ("Error creating TCP socket.");
        exit (1);
    }

    sin.sin_family = AF_INET;
    sin.sin_port = htons (port);
    sin.sin_addr.s_addr = INADDR_ANY;

    if (bind (sok, (struct sockaddr*)&sin, sizeof (struct sockaddr)) < 0)
    {
        perror ("Can't bind socket to free port.");
        return (-1);
    }

    if (listen (sok, BACKLOG) < 0)
    {
        perror ("Unable to listen on TCP socket");
        return (-1);
    }

    /* Get the port used -- needs to be sent in login packet to ICQ server */
    length = sizeof (struct sockaddr);
    getsockname (sok, (struct sockaddr *) &sin, &length);
    ssG.our_port = ntohs (sin.sin_port);
    M_print (i18n (777, "The port that will be used for tcp (partially implemented) is %d\n"),
             ssG.our_port);

    return sok;
}


/*
 *  Starts establishing a TCP connection to given contact.
 */
void TCPDirectOpen (Contact *cont)
{
    if (cont->sok.state == 10)
        return;

    TCPClose (&cont->sok);
    TCPConnect (&cont->sok, 0);
}

/*
 * Closes TCP message connection to given contact.
 */
void TCPDirectClose (Contact *cont)
{
    TCPClose (&cont->sok);
}


/*
 *  Does the next step establishing a TCP connection to given contact.
 *  On error, returns -1.
 */
int TCPConnect (tcpsock_t *sok, int mode)
{
    struct sockaddr_in sin;
    int flags, rc;
    static time_t last;
    time_t now;
    
    while (1)
    {
        switch (sok->state)
        {
            case -1:
                return -1;
            case 0:
                sin.sin_family = AF_INET;
                sin.sin_port = htons (sok->cont->port);
                sin.sin_addr.s_addr = Chars_2_DW (sok->cont->current_ip);

                sok->sok = socket (AF_INET, SOCK_STREAM, 0);
              
                if (sok->sok <= 0)
                {
                    if (uiG.Verbose)
                    {
                        rc = errno;
                        R_undraw ();
                        M_print (i18n (779, "Opening TCP connection to %s at %s:%d... ") , sok->cont->nick, inet_ntoa (sin.sin_addr), sok->cont->port);
                        M_print (i18n (780, "failure - couldn't create socket: %s (%d).\n"), strerror (rc), rc);
                        R_redraw ();
                    }
                    return -1;
                }

                flags = fcntl (sok->sok, F_GETFL, 0);
                if (flags != -1)
                    flags = fcntl (sok->sok, F_SETFL, flags | O_NONBLOCK);
                if (flags == -1)
                {
                    rc = errno;
                    R_undraw ();
                    M_print (i18n (779, "Opening TCP connection to %s at %s:%d... ") , sok->cont->nick, inet_ntoa (sin.sin_addr), sok->cont->port);
                    M_print (i18n (828, "failure - couldn't set socket nonblocking: %s (%d).\n"), strerror (rc), rc);
                    R_redraw ();
                    close (sok->sok);
                    return -1;
                }

                last = time (NULL);
                sok->sid = sin.sin_addr.s_addr;
                rc = connect (sok->sok, (struct sockaddr *) &sin, sizeof (struct sockaddr));
                if (rc >= 0)
                {
                    sok->state = 3;
                    break;
                }
                if ((rc = errno) == EINPROGRESS)
                {
                    sok->state = 1;
                    return 0;
                }
                R_undraw ();
                M_print (i18n (779, "Opening TCP connection to %s at %s:%d... ") , sok->cont->nick, inet_ntoa (sin.sin_addr), sok->cont->port);
                M_print (i18n (829, "failure - connect failed %d (%s) (%d)\n"), rc, strerror (rc), 1);
                R_redraw ();
                close (sok->sok);
                sok->sok = 0;
                sok->state = -1;
                return -1;
            case 1:
                now = time (NULL);
                rc = 0;
                flags = sizeof (int);
                if (getsockopt (sok->sok, SOL_SOCKET, SO_ERROR, &rc, &flags) < 0)
                    rc = errno;
                if (!rc)
                {
                    sok->state = 3;
                    break;
                }
                close (sok->sok);

                if (uiG.Verbose)
                {
                    sin.sin_addr.s_addr = sok->sid;
                    R_undraw ();
                    M_print (i18n (779, "Opening TCP connection to %s at %s:%d... ") , sok->cont->nick, inet_ntoa (sin.sin_addr), sok->cont->port);
                    M_print (i18n (829, "failure - connect failed %d (%s) (%d)\n"), errno, strerror (rc), 2);
                    R_redraw ();
                }

                sin.sin_family = AF_INET;
                sin.sin_port = htons (sok->cont->port);
                sin.sin_addr.s_addr = Chars_2_DW (sok->cont->other_ip);

                sok->sok = socket (AF_INET, SOCK_STREAM, 0);
              
                if (sok->sok <= 0)
                {
                    rc = errno;
                    R_undraw ();
                    M_print (i18n (779, "Opening TCP connection to %s at %s:%d... ") , sok->cont->nick, inet_ntoa (sin.sin_addr), sok->cont->port);
                    M_print (i18n (780, "failure - couldn't create socket: %s (%d).\n"), strerror (rc), rc);
                    R_redraw ();
                    sok->sok = 0;
                    sok->state = 0;
                    return -1;
                }

                flags = fcntl (sok->sok, F_GETFL, 0);
                if (flags != -1)
                    flags = fcntl (sok->sok, F_SETFL, flags | O_NONBLOCK);
                if (flags == -1)
                {
                    rc = errno;
                    R_undraw ();
                    M_print (i18n (779, "Opening TCP connection to %s at %s:%d... ") , sok->cont->nick, inet_ntoa (sin.sin_addr), sok->cont->port);
                    M_print (i18n (828, "failure - couldn't set socket nonblocking: %s (%d).\n"), strerror (rc), rc);
                    R_redraw ();
                    TCPClose (sok);
                    return -1;
                }

                last = time (NULL);
                rc = connect (sok->sok, (struct sockaddr *) &sin, sizeof (struct sockaddr));
                if (rc >= 0)
                {
                    sok->state = 3;
                    break;
                }
                if (errno == EINPROGRESS)
                {
                    sok->state = 2;
                    return 0;
                }
                R_undraw ();
                M_print (i18n (779, "Opening TCP connection to %s at %s:%d... ") , sok->cont->nick, inet_ntoa (sin.sin_addr), sok->cont->port);
                M_print (i18n (829, "failure - connect failed %d (%s) (%d)\n"), errno, strerror (rc), 3);
                R_redraw ();
                close (sok->sok);
                sok->sok = 0;
                sok->state = 0;
                return -1;
            case 2:
                now = time (NULL);
                rc = 0;
                flags = sizeof (int);
                if (getsockopt (sok->sok, SOL_SOCKET, SO_ERROR, &rc, &flags) < 0)
                    rc = errno;
                if (!rc)
                {
                    sok->state = 3;
                    break;
                }
                sin.sin_addr.s_addr = sok->sid;
                R_undraw ();
                M_print (i18n (779, "Opening TCP connection to %s at %s:%d... ") , sok->cont->nick, inet_ntoa (sin.sin_addr), sok->cont->port);
                M_print (i18n (829, "failure - connect failed %d (%s) (%d)\n"), errno, strerror (rc), 4);
                R_redraw ();
                close (sok->sok);
                sok->sok = 0;
                sok->state = -1;
                return -1;
            case 3:
                sin.sin_addr.s_addr = sok->sid;
                if (uiG.Verbose)
                {
                    R_undraw ();
                    M_print (i18n (779, "Opening TCP connection to %s at %s:%d... ") , sok->cont->nick, inet_ntoa (sin.sin_addr), sok->cont->port);
                    M_print (i18n (785, "success.\n"));
                    R_redraw ();
                }
                flags = fcntl (sok->sok, F_GETFL, 0);
                if (flags != -1)
                    flags = fcntl (sok->sok, F_SETFL, flags & ~O_NONBLOCK);
                if (flags == -1)
                {
                    R_undraw ();
                    M_print (i18n (779, "Opening TCP connection to %s at %s:%d... ") , sok->cont->nick, inet_ntoa (sin.sin_addr), sok->cont->port);
                    M_print (i18n (832, "failure - couldn't set socket blocking.\n"));
                    R_redraw ();
                    TCPClose (sok);
                    return -1;
                }
                sok->state = 4;
                TCPSendInit (sok);
                return 0;
            case 4:
                if (!mode)
                {
                    TCPClose (sok);
                    return -1;
                }
                sok->state = 5;
                TCPReceiveInitAck (sok);
                return 0;
            case 5:
                if (!mode)
                {
                    TCPClose (sok);
                    return -1;
                }
                sok->state = 9;
                TCPReceiveInit (sok);
                TCPSendInitAck (sok);
                break;
            case 6:
                sok->state = 7;
                TCPSendInitAck (sok);
                TCPSendInit (sok);
                return 0;
            case 7:
                if (!mode)
                {
                    TCPClose (sok);
                    return -1;
                }
                sok->state = 9;
                TCPReceiveInitAck (sok);
                break;
            case 9:
                R_undraw ();
                Time_Stamp ();
                M_print (" ");
                M_print (i18n (833, "Client-to-client TCP connection to %s established.\n"), sok->cont->nick);
                R_redraw ();
                sok->state = 10;
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
int TCPReceivePacket (tcpsock_t *sok, void **data)
{
    UBYTE buf[2];
    int rc, size, todo, bytesread = 0;
    void *get;

    if (sok->state <= 0)
        return 0;

    for (;;)
    {
        if (sockread (sok->sok, buf, 2) < 0)
            break;

        size = Chars_2_Word (buf);
        if (size <= 0)
        {
            errno = ENOMEM;
            break;
        }

        *data = malloc (size);
        if (!data)
        {
            errno = ENOMEM;
            break;
        }
        
        for (get = *data, todo = size; todo > 0; todo -= bytesread, get += bytesread)
        {
            bytesread = sockread (sok->sok, get, todo);
            if (bytesread < 0)
            {
                free (*data);
                break;
            }
        }
        if (bytesread < 0)
            break;

        if (uiG.Verbose & 4)
        {
            R_undraw ();
            Time_Stamp ();
            M_print (" \x1b«" COLSERV "");
            M_print (i18n (778, "Incoming TCP packet:"));
            M_print (COLNONE "*\n");
            Hex_Dump (*data, size);
            M_print ("\x1b»\n");
            R_redraw ();
        }

        return size;
    }
    
    if (uiG.Verbose)
    {
        rc = errno;
        R_undraw ();
        Time_Stamp ();
        M_print (" ");
        M_print (i18n (834, "Error while reading from socket - %s (%d)\n"),
                 strerror (rc), rc);
        R_redraw ();
    }
    TCPClose (sok);
    return 0;
}

/*
 * Sends an outgoing TCP packet.
 * Resets socket on error.
 */
void TCPSendPacket (tcpsock_t *sok, void *data, UWORD size)
{
    UBYTE buf[2];
    int rc, todo, bytessend = 0;
    
    if (sok->state <= 0)
        return;

    for (;;)
    {
        Word_2_Chars (buf, size);
        if (sockwrite (sok->sok, buf, 2) < 0)
            break;

        if (uiG.Verbose & 4)
        {
            R_undraw ();
            Time_Stamp ();
            M_print (" \x1b«" COLCLIENT "");
            M_print (i18n (776, "Outgoing TCP packet:"));
            M_print (COLNONE "*\n");
            Hex_Dump (data, size);
            M_print ("\x1b»\n");
            R_redraw ();
        }

        for (todo = size; todo > 0; todo -= bytessend, data += bytessend)
        {
            bytessend = sockwrite (sok->sok, data, todo);
            if (bytessend < 0)
                break;
        }
        if (bytessend < 0)
            break;
        return;
    }
    
    if (uiG.Verbose)
    {
        rc = errno;
        R_undraw ();
        Time_Stamp ();
        M_print (" ");
        M_print (i18n (835, "Error while writing to socket - %s (%d)\n"),
                 strerror (rc), rc);
        R_redraw ();
    }
    TCPClose (sok);
}

/*
 * Sends a TCP initialization packet.
 */
void TCPSendInit (tcpsock_t *sok)
{
    TCP_INIT_PAK pak;

    if (sok->state <= 0)
        return;

    if (!sok->sid)
        sok->sid = rand ();

    if (uiG.Verbose)
    {
        R_undraw ();
        M_print (i18n (836, "Sending TCP direct connection initialization packet.\n"));
        R_redraw ();
    }

    pak.cmd = 0xFF;
    Word_2_Chars (pak.version, TCP_VER);
    Word_2_Chars (pak.rev, TCP_VER_REV);
    DW_2_Chars (pak.dest_uin, sok->cont->uin);
    bzero (pak.X1, 2);
    DW_2_Chars (pak.port, ssG.our_port);
    DW_2_Chars (pak.uin, ssG.UIN);
    DW_2_Chars (pak.current_ip, htonl (ssG.our_outside_ip));
    DW_2_Chars (pak.other_ip, htonl (ssG.our_ip));
    pak.connection_type = TCP_OK_FLAG;
    DW_2_Chars (pak.other_port, ssG.our_port);
    DW_2_Chars (pak.session_id, sok->sid);

    TCPSendPacket (sok, (void *) &pak, sizeof(pak));
}

/*
 * Sends the initialization packet
 */
void TCPSendInitAck (tcpsock_t *sok)
{
    TCP_INIT_ACK_PAK pak;

    if (sok->state <= 0)
        return;

    pak.cmd = TCP_INIT_ACK;
    bzero (pak.X1, 3);

    if (uiG.Verbose)
    {
        R_undraw ();
        M_print (i18n (837, "Acknowledging TCP direct connection initialization packet.\n"));
        R_redraw ();
    }

    TCPSendPacket (sok, (void *) &pak, sizeof(pak));
}

void TCPReceiveInit (tcpsock_t *sok)
{
    TCP_INIT_PTR pak;
    Contact *cont;
    UDWORD sid, size;

    if (sok->state <= 0)
        return;

    size = TCPReceivePacket (sok, (void *) &pak);
    
    if (!size)
        return;

    for (;;)
    {
        if (size < 26)
            break;
    
        cont = ContactFind (Chars_2_DW (pak->uin));
        sid  = Chars_2_DW (pak->session_id);

        if (sok->cont && sok->cont != cont)
            break;

        if (!cont)
            break;

        cont->sok = *sok;
        sok = &cont->sok;
        
        if (!sok->sid)
            sok->sid = sid;

        if (sok->sid != sid)
            break;

        if (uiG.Verbose)
        {
            R_undraw ();
            M_print (i18n (838, "Received direct connection initialization.\n"));
            M_print ("    \x1b«");
            M_print (i18n (839, "Version %04x:%04x, Port %d, UIN %d, session %08x\n"),
                     Chars_2_Word (pak->version), Chars_2_Word (pak->rev),
                     Chars_2_DW (pak->port), Chars_2_DW (pak->uin),
                     Chars_2_DW (pak->session_id));
            M_print ("\x1b»\n");
            R_redraw ();
        }

        free (pak);
        return;
    }
    if (uiG.Verbose)
    {
        R_undraw ();
        M_print (i18n (840, "Received malformed direct connection initialization.\n"));
        R_redraw ();
    }
    TCPClose (sok);
}

/*
 * Receives the acknowledge packet for the initialization packet.
 */
void TCPReceiveInitAck (tcpsock_t *sok)
{
    TCP_INIT_ACK_PTR pak;
    int size;

    if (sok->state <= 0)
        return;

    size = TCPReceivePacket (sok, (void *)&pak);

    if (size && (size != 4 || pak->cmd != TCP_INIT_ACK))
    {
        R_undraw ();
        M_print (i18n (841, "Received malformed initialization acknowledgement packet."));
        R_redraw ();
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
        R_undraw ();
        Time_Stamp ();
        M_print (" ");
        if (sok->cont)
            M_print (i18n (842, "Closing socket %d to %s.\n"), sok->sok, sok->cont->nick);
        else
            M_print (i18n (843, "Closing socket %d.\n"), sok->sok);
        R_redraw ();
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
void TCPDirectReceive (SOK_T sok)
{
    tcpsock_t sock;
    struct sockaddr_in sin;
    int tmp;
 
    tmp = sizeof (sin);

    sock.sok   = accept (sok, (struct sockaddr *)&sin, &tmp);
    
    if (sock.sok <= 0)
        return;
    
    sock.state = 6;
    sock.sid   = 0;
    sock.cont  = NULL;
    
    TCPReceiveInit (&sock);
}

/*
 * Handles activity on socket for given contact.
 */
void TCPHandleComm (Contact *cont, int mode)
{
    if (cont->sok.state > 0 && cont->sok.state < 10)
        TCPConnect (&cont->sok, mode);
    else
        Handle_TCP_Comm (cont->uin);
}

/*
 * Sends a message via TCP.
 * Adds it to the resend queue until acked.
 */
BOOL TCPSendMsg (SOK_T srvsok, UDWORD uin, char *msg, UWORD sub_cmd)
{
    Contact *cont;
    TCP_MSG_PTR pak;
    struct Event *q_item;
    int paksize;
    int msgtype = 0;

    cont = ContactFind (uin);

    if (!cont)
        return 0;
    if (cont->status == STATUS_OFFLINE)
        return 0;
    if (cont->sok.state < 0)
        return 0;

    if (!cont->sok.state)
        TCPDirectOpen (cont);

    paksize = sizeof(TCP_MSG_PAK) + strlen(msg) + 1 + 8; 
                    /*   header   +     msg  + null + footer */
    q_item = (struct Event *) malloc (sizeof (struct Event));

    /* Make the packet */
    pak = (TCP_MSG_PTR) malloc (paksize);

    bzero (&(pak->checksum), 4);
    Word_2_Chars (pak->cmd, TCP_CMD_MESSAGE);
    DW_2_Chars (pak->X1, TCP_MSG_X1);
    Word_2_Chars (pak->seq, ssG.seq_tcp--);
    bzero (&(pak->X2), 12);                /* X2 X3 X4 all null */
    Word_2_Chars (pak->sub_cmd, sub_cmd);
    bzero (&(pak->status), 2);
    
    switch (uiG.Current_Status)
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
    Word_2_Chars (pak->msg_type, msgtype);

    Word_2_Chars (pak->size, strlen(msg) + 1);
    strcpy (((char *) pak) + sizeof(TCP_MSG_PAK), msg);
    DW_2_Chars (((char *) pak) + paksize - 8, COL_FG);
    DW_2_Chars (((char *) pak) + paksize - 4, COL_BG);

    Encrypt_Pak ((UBYTE *) pak, paksize);

    /* Queue the message */
    q_item->seq = ssG.seq_tcp + 1;
    q_item->attempts = 1;
    q_item->due = time (NULL);
    q_item->uin = cont->uin;
    q_item->len = paksize;
    q_item->body = (UBYTE *) pak;
    q_item->info = strdup (MsgEllipsis (msg));
    q_item->callback = &TCPCallBackResend;
    q_item->type = QUEUE_TYPE_TCP_RESEND;
    QueueEnqueue (queue, q_item);

    if (cont->sok.state != 10)
    {
        Time_Stamp ();
        M_print (" " COLSENT "%10s" COLNONE " ... %s\n", ContactFindName (cont->uin), MsgEllipsis (msg));
    }
    return 1;
}

/*
 * Resends TCP packets if necessary
 */
void TCPCallBackResend (int srv_sok, struct Event *event)
{
    TCP_MSG_PTR pak;
    Contact *cont;
    tcpsock_t *sok;

    pak  = (TCP_MSG_PAK *) event->body;
    cont = ContactFind (event->uin);
    sok  = &cont->sok;

    if (event->attempts >= MAX_RETRY_ATTEMPTS)
        TCPClose (sok);

    if (sok->state > 0)
    {
        if (sok->state == 10)
        {
            R_undraw ();
            Time_Stamp ();
            M_print (" " COLCONTACT "%10s" COLNONE " %s%s\n", cont->nick, MSGTCPSENTSTR, event->info);
            R_redraw ();

            event->attempts++;
            TCPSendPacket (sok, event->body, event->len);
        }
        event->due = time (NULL) + 1;
        QueueEnqueue (queue, event);
        return;
    }

    if (event->attempts >= MAX_RETRY_ATTEMPTS && Chars_2_Word (pak->cmd) != TCP_CMD_MESSAGE)
    {
        R_undraw ();
        M_print (i18n (844, "TCP message %04x discarded after timeout.\n"), Chars_2_Word (pak->cmd));
        R_redraw ();
    }
    if (event->attempts < MAX_RETRY_ATTEMPTS && Chars_2_Word (pak->cmd) == TCP_CMD_MESSAGE
        && !(Chars_2_Word (pak->msg_type) & TCP_AUTO_RESPONSE_MASK))
    {
        UDPSendMsg (srv_sok, cont->uin, event->body + TCP_MSG_OFFSET,
                    Chars_2_Word (pak->msg_type));
    }
    free (event->info);
    free (event->body);
    free (event);
}



/*
 * Handles all incoming TCP traffic.
 * Queues incoming packets, ignoring resends
 * and deleting CANCEL requests from the queue.
 */
void Handle_TCP_Comm (UDWORD uin)
{
    Contact *cont;
    TCP_MSG_PTR pak;
    struct Event *event;
    int size;
    int i = 0;
    UWORD seq_in = 0;
     
    cont = ContactFind (uin);
    event = (struct Event *) malloc (sizeof (struct Event));

    /* Recv all packets before doing anything else.
         The objective is to delete any packets CANCELLED by the remote user. */
    while (M_Is_Set (cont->sok.sok) && i <= TCP_MSG_QUEUE)
    {
        if ((size = TCPReceivePacket (&cont->sok, (void **) &pak)) < 0)
        {
            if (uiG.Verbose)
            {
                R_undraw ();
                M_print (i18n (797, "User %s%s%s closed socket.\n"), COLCONTACT, cont->nick, COLNONE);
                R_redraw ();
            }
            TCPClose (&cont->sok);
            break;
        }
        else 
        {
            if (Decrypt_Pak ((UBYTE *) pak, size) < 0)
            {
                TCPClose (&cont->sok);
                R_undraw ();
                M_print (i18n (789, "Received malformed packet."));
                R_redraw ();
                break;
            }
    
            if (uiG.Verbose & 4)
            {
                R_undraw ();
                Time_Stamp ();
                M_print (" \x1b«" COLSERV "");
                M_print (i18n (778, "Incoming TCP packet:"));
                M_print (COLNONE "\n");
                Hex_Dump (pak, size);
                M_print ("\x1b»\n");
                R_redraw ();
            }
            
            /* Make sure this isn't a resend */
            if ((seq_in == 0) || (Chars_2_Word (pak->seq) < seq_in))
            { 
                seq_in = Chars_2_Word (pak->seq);

                /* Deal with CANCEL and ACK packets now */
                switch (Chars_2_Word (pak->cmd))
                {
                    case TCP_CMD_ACK:
                        {
                            struct Event *event = QueueDequeue (queue, seq_in, QUEUE_TYPE_TCP_RESEND);
                            if (!event)
                                break;
                            if (Chars_2_Word (pak->sub_cmd) == NORM_MESS)
                            {
                                R_undraw ();
                                Time_Stamp ();
                                M_print (" " COLACK "%10s" COLNONE " " MSGTCPACKSTR "%s\n",
                                         cont->nick, event->info);
                                R_redraw ();
                            }
                            else if (Chars_2_Word (pak->sub_cmd) & TCP_AUTO_RESPONSE_MASK)
                            {
                                R_undraw ();
                                M_print (i18n (194, "Auto-response message for %s:\n"), ContactFindNick (cont->uin));
                                M_print (MESSCOL "%s\n" NOCOL, ((UBYTE *)pak) + TCP_MSG_OFFSET);
                                R_redraw ();
                            }
                            if (uiG.Verbose && Chars_2_Word (pak->sub_cmd) != NORM_MESS)
                            {
                                R_undraw ();
                                M_print (i18n (806, "Received ACK for message (seq %04X) from %s\n"),
                                         seq_in, cont->nick);
                                R_redraw ();
                            }
                            free (event->info);
                            free (event->body);
                            free (event);
                        }
                        break;
                    
                    case TCP_CMD_CANCEL:
                        {
                        struct Event *event = QueueDequeue (tcp_rq, seq_in, QUEUE_TYPE_TCP_RESEND);
                        if (event) {
                        free (event->info);
                        free (event->body);
                        free (event);
                        }}
                        if (uiG.Verbose)
                        {
                            R_undraw ();
                            M_print (i18n (807, "Cancelled incoming message (seq %04X) from %s\n"),
                                     seq_in, cont->nick);
                            R_redraw ();
                        }
                        break;

                    default:
                        /* Store the event in the recv queue for handling later */            
                        event->seq = seq_in;
                        event->attempts = 0;
                        event->due = 0;
                        event->body = (UBYTE *) pak;
                        event->info = NULL;
                        event->len = size;
                        event->callback = &TCPCallBackResend;
                        event->type = QUEUE_TYPE_TCP_RESEND;
                        QueueEnqueue (tcp_rq, event);                
                        ssG.seq_tcp--;
                    break;
                }
            }
        }
        
        i++;
        
        M_set_timeout (0, 100000);
        M_select_init();
        M_Add_rsocket (cont->sok.sok);
        M_select();
    }

    /* Now handle all packets in the recv queue */
    while ((event = QueuePop (tcp_rq)) != NULL)
    {
        pak = (TCP_MSG_PAK *) event->body;
        R_undraw ();
        switch (Chars_2_Word (pak->sub_cmd))
        {
            /* Requests for auto-response message */
            case TCP_CMD_GET_AWAY:
            case TCP_CMD_GET_OCC:
            case TCP_CMD_GET_NA:
            case TCP_CMD_GET_DND:
            case TCP_CMD_GET_FFC:
                M_print (i18n (814, "Sent auto-response message to %s%s%s.\n"),
                         COLCONTACT, cont->nick, COLNONE);
                Send_TCP_Ack (&cont->sok, Chars_2_Word(pak->seq), Chars_2_Word(pak->sub_cmd), TRUE);
                break;

            /* Automatically reject file xfer and chat requests
                 as these are not implemented yet. */ 
            case CHAT_MESS:
            case FILE_MESS:
                Send_TCP_Ack (&cont->sok, Chars_2_Word(pak->seq), Chars_2_Word(pak->sub_cmd), FALSE);
                break;

            /* Regular messages */
            default:
                ssG.last_recv_uin = cont->uin;
                
                Time_Stamp ();
                M_print ("\a " CYAN BOLD "%10s" COLNONE " ", ContactFindName (cont->uin));
                
                Do_Msg (cont->sok.sok, Chars_2_Word (pak->sub_cmd), Chars_2_Word (pak->size), 
                        &((char *)pak)[TCP_MSG_OFFSET], uin, 1);

                Send_TCP_Ack (&cont->sok, Chars_2_Word(pak->seq),
                              Chars_2_Word(pak->sub_cmd), TRUE);
        }
        free (pak);
        free (event);
        R_redraw ();
    }
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
int Send_TCP_Ack (tcpsock_t *sok, UWORD seq, UWORD sub_cmd, BOOL accept)
{
    TCP_MSG_PTR pak;
    int size;
    char *msg;

    msg = Get_Auto_Reply ();
 
    size = TCP_MSG_OFFSET + strlen(msg) + 1 + 8;    /* header + msg + null + tail */
    pak = (TCP_MSG_PAK *) malloc (size);

    Word_2_Chars (pak->cmd, TCP_CMD_ACK);
    Word_2_Chars (pak->X1, TCP_MSG_X1);
    Word_2_Chars (pak->seq, seq);
    bzero (&(pak->X2), 12);        /* X2 X3 X4 are all null */
    Word_2_Chars (pak->sub_cmd, sub_cmd);
    Word_2_Chars (pak->status, (accept ? TCP_STAT_ONLINE : TCP_STAT_REFUSE)); 
    Word_2_Chars (pak->msg_type, TCP_MSG_AUTO);
    Word_2_Chars (pak->size, strlen(msg) + 1);
    strcpy (&((char *) pak)[TCP_MSG_OFFSET], msg);    
    DW_2_Chars (&((char *) pak)[TCP_MSG_OFFSET + strlen(msg) + 1], COL_FG);
    DW_2_Chars (&((char *) pak)[TCP_MSG_OFFSET + strlen(msg) + 5], COL_BG);

    Encrypt_Pak ((UBYTE *) pak, size);
    TCPSendPacket (sok, pak, size);
    free (msg);
    return 1;
}

/*
 * Encrypts/Decrypts TCP packets
 * (leeched from licq) 
 */
void Encrypt_Pak (UBYTE *pak, UDWORD size)
{
    UDWORD B1, M1, check;
    int i;
    UBYTE X1, X2, X3;
    UDWORD hex, key;

    /* calculate verification data */
    M1 = (rand() % ((size < 255 ? size : 255)-10))+10;
    X1 = pak[M1] ^ 0xFF;
    X2 = rand() % 220;
    X3 = client_check_data[X2] ^ 0xFF;
    B1 = (pak[4]<<24) | (pak[6]<<16) | (pak[4]<<8) | (pak[6]);

    /* calculate checkcode */
    check = (M1 << 24) | (X1 << 16) | (X2 << 8) | X3;
    check ^= B1;

    /* main XOR key */
    key = 0x67657268 * size + check;

    /* XORing the actual data */
    for (i = 0; i < (size+3) / 4; i += 4)
    {
        hex = key + client_check_data[i&0xFF];
        pak[i+0] ^= hex & 0xFF;
        pak[i+1] ^= (hex>>8) & 0xFF;
        pak[i+2] ^= (hex>>16) & 0xFF;
        pak[i+3] ^= (hex>>24) & 0xFF;
    }

    /* storing the checkcode */
    DW_2_Chars (pak, check);
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
 * i18n (59, " ") i18n
 * i18n (626, " ") i18n (627, " ") i18n
 * i18n (634, " ") i18n (808, " ") i18n (810, " ") i18n (811, " ") i18n
 * i18n (762, " ") i18n (787, " ")
 * i18n (618, " ") i18n (788, " ") i18n (790, " ") i18n (792, " ") i18n (793, " ") i18n
 * i18n (794, " ") i18n (795, " ") i18n (796, " ") i18n
 * i18n (831, " ") i18n (619, " ") i18n (791, " ") i18n
 */
