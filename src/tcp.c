/*
 * This file handles TCP client-to-client communications.
 *
 *  Author: James Schofield (jschofield@ottawa.com) 22 Feb 2001
 *  Lots of changes from Rüdiger Kuhlmann.
 */

/*
.#include "datatype.h"
.#include "mselect.h"
.#include "msg_queue.h"
.#include "mreadline.h"
.#include "util_ui.h"
.#include "util.h"
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

const UBYTE client_check_data[] = {
    "As part of this software beta version Mirabilis is "
    "granting a limited access to the ICQ network, "
    "servers, directories, listings, information and databases (\""
    "ICQ Services and Information\"). The "
    "ICQ Service and Information may databases (\""
    "ICQ Services and Information\"). The "
    "ICQ Service and Information may\0"
};

struct msg_queue *tcp_rq = NULL, *tcp_sq = NULL;

void TCPClose (Contact *cont)
{
    close (cont->sok);
    cont->sok = 0;
    cont->sokflag = 0;
    cont->session_id = 0;
}

/* Initializes TCP socket for incoming connections on given port.
 * 0 = random port
 */
SOK_T TCPInit (int port)
{
    int sok;
    int length;
    struct sockaddr_in sin;

    msg_queue_init (&tcp_rq);
    msg_queue_init (&tcp_sq);

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
 * Handles activity on socket for given contact.
 */
void TCPHandleComm (Contact *cont)
{
    if (cont->sokflag > 0 && cont->sokflag < 10)
        TCPConnect (cont);
    else if (cont->session_id > 0)
        Recv_TCP_Init (cont->sok);
    else
        Handle_TCP_Comm (cont->uin);
}

/*
 *  Establishes a TCP connection to given contact.
 *  On error, returns -1.
 */
int TCPConnect (Contact *cont)
{
    struct sockaddr_in sin;
    int flags, rc;
/*    TCP_INIT_PTR pak;*/
    static time_t last;
    time_t now;
    
    R_undraw ();
    M_print ("Debug: entered TCPConnect\n");
    R_redraw ();
    while (1)
    {
        R_undraw ();
        M_print ("Debug: on %s sok %d sokflag %d\n", cont->nick, cont->sok, cont->sokflag);
        R_redraw ();
        switch (cont->sokflag)
        {
            case -1:
                return -1;
            case 0:
                sin.sin_family = AF_INET;
                sin.sin_port = htons (cont->port);
                sin.sin_addr.s_addr = Chars_2_DW (cont->current_ip);

                cont->sok = socket (AF_INET, SOCK_STREAM, 0);
              
                if (cont->sok <= 0)
                {
                    R_undraw ();
                    M_print (i18n (779, "Opening TCP connection to %s at %s:%d... ") , cont->nick, inet_ntoa (sin.sin_addr), cont->port);
                    M_print (i18n (780, "failure - couldn't create socket.\n"));
                    R_redraw ();
                    return -1;
                }

                flags = fcntl (cont->sok, F_GETFL, 0);
                if (flags != -1)
                    flags = fcntl (cont->sok, F_SETFL, flags | O_NONBLOCK);
                if (flags == -1)
                {
                    R_undraw ();
                    M_print (i18n (779, "Opening TCP connection to %s at %s:%d... ") , cont->nick, inet_ntoa (sin.sin_addr), cont->port);
                    M_print (i18n (828, "failure - couldn't set socket nonblocking.\n"));
                    R_redraw ();
                    close (cont->sok);
                    return -1;
                }

                last = time (NULL);
                cont->session_id = sin.sin_addr.s_addr;
                rc = connect (cont->sok, (struct sockaddr *) &sin, sizeof (struct sockaddr));
                if (rc >= 0)
                {
                    cont->sokflag = 3;
                    break;
                }
                if (errno == EINPROGRESS)
                {
                    cont->sokflag = 1;
                    return 0;
                }
                R_undraw ();
                M_print (i18n (779, "Opening TCP connection to %s at %s:%d... ") , cont->nick, inet_ntoa (sin.sin_addr), cont->port);
                M_print (i18n (829, "failure - connect failed %d (%s) (%d)\n"), errno, strerror (rc), 1);
                R_redraw ();
                close (cont->sok);
                cont->sok = 0;
                return -1;
            case 1:
                now = time (NULL);
                rc = 0;
                flags = sizeof (int);
                if (getsockopt (cont->sok, SOL_SOCKET, SO_ERROR, &rc, &flags) < 0)
                    rc = errno;
                if (!rc)
                {
                    cont->sokflag = 3;
                    break;
                }
                close (cont->sok);

                sin.sin_addr.s_addr = cont->session_id;
                R_undraw ();
                M_print (i18n (779, "Opening TCP connection to %s at %s:%d... ") , cont->nick, inet_ntoa (sin.sin_addr), cont->port);
                M_print (i18n (829, "failure - connect failed %d (%s) (%d)\n"), errno, strerror (rc), 2);
                R_redraw ();

                sin.sin_family = AF_INET;
                sin.sin_port = htons (cont->port);
                sin.sin_addr.s_addr = Chars_2_DW (cont->other_ip);

                cont->sok = socket (AF_INET, SOCK_STREAM, 0);
              
                if (cont->sok <= 0)
                {
                    R_undraw ();
                    M_print (i18n (779, "Opening TCP connection to %s at %s:%d... ") , cont->nick, inet_ntoa (sin.sin_addr), cont->port);
                    M_print (i18n (780, "failure - couldn't create socket.\n"));
                    R_redraw ();
                    cont->sok = 0;
                    return -1;
                }

                flags = fcntl (cont->sok, F_GETFL, 0);
                if (flags != -1)
                    flags = fcntl (cont->sok, F_SETFL, flags | O_NONBLOCK);
                if (flags == -1)
                {
                    R_undraw ();
                    M_print (i18n (779, "Opening TCP connection to %s at %s:%d... ") , cont->nick, inet_ntoa (sin.sin_addr), cont->port);
                    M_print (i18n (828, "failure - couldn't set socket nonblocking.\n"));
                    R_redraw ();
                    close (cont->sok);
                    cont->sok = 0;
                    return -1;
                }

                last = time (NULL);
                rc = connect (cont->sok, (struct sockaddr *) &sin, sizeof (struct sockaddr));
                if (rc >= 0)
                {
                    cont->sokflag = 3;
                    break;
                }
                if (errno == EINPROGRESS)
                {
                    cont->sokflag = 2;
                    return 0;
                }
                R_undraw ();
                M_print (i18n (779, "Opening TCP connection to %s at %s:%d... ") , cont->nick, inet_ntoa (sin.sin_addr), cont->port);
                M_print (i18n (829, "failure - connect failed %d (%s) (%d)\n"), errno, strerror (rc), 3);
                R_redraw ();
                close (cont->sok);
                cont->sok = 0;
                cont->sokflag = 0;
                return -1;
            case 2:
                now = time (NULL);
                rc = 0;
                flags = sizeof (int);
                if (getsockopt (cont->sok, SOL_SOCKET, SO_ERROR, &rc, &flags) < 0)
                    rc = errno;
                if (!rc)
                {
                    cont->sokflag = 3;
                    break;
                }
                sin.sin_addr.s_addr = cont->session_id;
                R_undraw ();
                M_print (i18n (779, "Opening TCP connection to %s at %s:%d... ") , cont->nick, inet_ntoa (sin.sin_addr), cont->port);
                M_print (i18n (829, "failure - connect failed %d (%s) (%d)\n"), errno, strerror (rc), 4);
                R_redraw ();
                close (cont->sok);
                cont->sok = 0;
                cont->sokflag = -1;
                return -1;
            case 3:
                sin.sin_addr.s_addr = cont->session_id;
                R_undraw ();
                M_print (i18n (779, "Opening TCP connection to %s at %s:%d... ") , cont->nick, inet_ntoa (sin.sin_addr), cont->port);
                M_print (i18n (785, "success.\n"));
                R_redraw ();
                Send_TCP_Init (cont->uin);
                return 0;
            case 4:
            case 5:
            default:
                assert (0);
        }
    }
}



/*
 *  Reads an incoming TCP packet and returns size of packet.
 *  Returns -1 on error. Paket must be free()d.
 */
int TCPReadPacket (SOK_T sok, void **pak)
{
    UBYTE s[2];
    int recvSize = 0;
    int offset = 0;
    UWORD size;

    if (sockread (sok, s, 2) < 0)
        return -1;

    size = Chars_2_Word (s);
    if (size <= 0 || size > 100000)
        return -1;

    *pak = malloc (size);
    if (!pak)
        return -1;
    
    while ((offset += recvSize) < size)
    {
        recvSize = sockread (sok, *pak + offset, (size - offset));
        if (recvSize < 0)
        {
            free (pak);
            return -1;
        }
    }
    return size;
}

/*
 * Sends an outgoing TCP packet.
 * Returns -1 on error, 1 on success.
 */
int TCPSendPacket (SOK_T sok, void *pak, UWORD size)
{
    UBYTE s[2];
    int sendSize = 0;
    int offset = 0;
    
    Word_2_Chars (s, size);
    if (sockwrite (sok, s, 2) < 0)
        return -1;

    if (uiG.Verbose & 4)
    {
        R_undraw ();
        Time_Stamp ();
        M_print (" \x1b«" COLCLIENT "");
        M_print (i18n (776, "Outgoing TCP packet:"));
        M_print (COLNONE "\n");
        Hex_Dump (pak, size);
        M_print ("\x1b»\n");
        R_redraw ();
    }

    while ((offset += sendSize) < size)
    {
        sendSize = sockwrite (sok, pak + offset, (size - offset));
        if (sendSize < 0)
            return -1;
    }
    return 1;
}

/*
 * Sends a TCP initialization packet.
 * Must be sent to initiate communication or
 * to respond to other side if they initiate
 */
void Send_TCP_Init (UDWORD dest_uin)
{
    Contact *cont;
    TCP_INIT_PAK pak;

    cont = ContactFind (dest_uin);

    assert (cont->sokflag >= 3);
    assert (cont->sokflag <= 4);    

    /* Form the packet */
    pak.cmd = 0xFF;
    Word_2_Chars (pak.version, TCP_VER);
    Word_2_Chars (pak.rev, TCP_VER_REV);
    DW_2_Chars (pak.dest_uin, dest_uin);
    bzero (pak.X1, 2);
    DW_2_Chars (pak.port, ssG.our_port);
    DW_2_Chars (pak.uin, ssG.UIN);
    DW_2_Chars (pak.current_ip, htonl (ssG.our_outside_ip));
    DW_2_Chars (pak.other_ip, htonl (ssG.our_ip));
    pak.connection_type = TCP_OK_FLAG;
    DW_2_Chars (pak.other_port, ssG.our_port);

    if (cont->sokflag == 3)
    {    
        /* Create a random session id, and save the value
            (to verify against the init packet the other side sends back) */
        cont->session_id = rand();
        DW_2_Chars (pak.session_id, cont->session_id);
    }
    else
    {
        /* Send the existing session id back, and reset the value
             to zero since we don't need it anymore                                    */    
        DW_2_Chars (pak.session_id, cont->session_id);
        cont->session_id = 0;
    }

    if (TCPSendPacket (cont->sok, (void *) &pak, sizeof(pak)) < 0)
    {
        R_undraw ();
        M_print (i18n (787, "Error sending TCP packet.\n"));
        R_redraw ();
        TCPClose (cont);
        return;
    }

    cont->sokflag = 5;
}

/*
 * Waits for the other side to ack
 * the init packet
 */
int Wait_TCP_Init_Ack (SOK_T sok)
{
    TCP_INIT_ACK_PTR pak;

    if (TCPReadPacket (sok, (void *) &pak) < 0)
    {
        M_print (i18n (788, "Error receiving TCP packet."));
        return -1;
    }

    if (pak->cmd != TCP_INIT_ACK)
    {
        M_print (i18n (789, "Received malformed packet."));
        close (sok);
        return -1;
    }

    return 0;
}

/*
 * Acks the init packet
 */
int TCP_Init_Ack (SOK_T sok)
{
    TCP_INIT_ACK_PAK pak;

    pak.cmd = TCP_INIT_ACK;
    bzero (pak.X1, 3);

    if (uiG.Verbose)
        M_print (i18n (790, "Sending init ACK...\n"));

    if (TCPSendPacket (sok, (void *) &pak, sizeof(pak)) < 0)
    {
        M_print (i18n (791, "Error sending TCP packet."));
        return -1;
    }
    return 1;
}

/*
 * Handles a new TCP connection.
 * For incoming connections, the flow is:
 *     micq.c calls New_TCP
 *     which creates a sok and calls Recv_TCP_Init
 *     which recvs the init pak and calls Send_TCP_Init
 *     which sends the init pak back.
 */
void New_TCP (SOK_T sok)
{
    SOK_T newSok; 
    struct sockaddr_in sin;
    int sinSize;
 
    sinSize = sizeof (sin);
    newSok = accept (sok, (struct sockaddr *)&sin, &sinSize);

    Recv_TCP_Init (newSok);
}

int Recv_TCP_Init (SOK_T sok)
{
    TCP_INIT_PTR pak;
    Contact *cont;

    if (TCPReadPacket (sok, (void *) &pak) < 0)
    {
        M_print (i18n (788, "Error receiving TCP packet."));
        return -1;
    }

    if (uiG.Verbose)
    {
        R_undraw ();
        M_print (i18n (792, "Establishing a direct TCP connection...\n"));
        //M_print("Ver : 0x%04X\n", Chars_2_Word (pak->version));
        //M_print("Rev : 0x%04X\n", Chars_2_Word (pak->rev));
        M_print (i18n (793, "Port: %lu\n") , Chars_2_DW (pak->port));
        M_print (i18n (794, "Uin : %lu\n") , Chars_2_DW (pak->uin));
        M_print (i18n (795, "SID : 0x%08X\n") , Chars_2_DW (pak->session_id));
        R_redraw ();
    }

    /* Save the socket with contact */
    cont = ContactFind (Chars_2_DW (pak->uin));
    if (cont)
    {
        cont->sok = sok;
        if (cont->session_id == 0)
        {
            /* In this case, the other side is initiating the connection,
                 so we'll save the session_id and send it back for verification. */
            cont->session_id = Chars_2_DW (pak->session_id);
            TCP_Init_Ack (sok);
            Send_TCP_Init (cont->uin);
        }
        else
        {
            /* Otherwise, WE initiated, so check verify the session_id they
                 sent back against the one we have saved.                                            */
            if (cont->session_id != Chars_2_DW (pak->session_id)) 
            {
                /* Bad session ID, close the connection */
                close (sok);
                TCPClose (cont);
                if (uiG.Verbose)
                    M_print (i18n (796, "Error initiating a TCP connection.\n"));
            }
            else
            {
                TCP_Init_Ack (sok);
            }
            cont->session_id = 0;
        }    
    }        
    else
    {
        /* Not on contact list -- kill the sok, and hope the client
             switches to communicating via server */
        close (sok);
    }
    free(pak);
    return 1;
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
    struct msg *msg_in;
    int size;
    int i = 0;
    UWORD seq_in = 0;
    struct msg *queued_msg;
     
    cont = ContactFind (uin);
    msg_in = (struct msg *) malloc (sizeof (struct msg));

    /* Recv all packets before doing anything else.
         The objective is to delete any packets CANCELLED by the remote user. */
    while (M_Is_Set (cont->sok) && i <= TCP_MSG_QUEUE)
    {
        if ((size = TCPReadPacket (cont->sok, (void **) &pak)) < 0)
        {
            if (uiG.Verbose)
            {
                R_undraw ();
                M_print (i18n (797, "User %s%s%s closed socket.\n"), COLCONTACT, cont->nick, COLNONE);
                R_redraw ();
            }
            TCPClose (cont);
            break;
        }
        else 
        {
            if (Decrypt_Pak ((UBYTE *) pak, size) < 0)
            {
                TCPClose (cont);
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
/*                M_print (" %04X %08X:%08X %04X (%s)" COLNONE "\n", Chars_2_Word (pak.head.ver),
                         Chars_2_DW (pak.head.session), Chars_2_DW (pak.head.seq),
                         Chars_2_Word (pak.head.cmd), ...); */
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
                        /* Delete packet from send queue */
                        Check_Queue (seq_in, tcp_sq);
                        if (Chars_2_Word (pak->sub_cmd) == NORM_MESS)
                        {
                            R_undraw ();
                            Time_Stamp ();
                            M_print (" " COLACK "%10s" COLNONE " " MSGTCPACKSTR "%s\n",
                                     cont->nick, (char *)(pak+1));
                            R_redraw ();
                        }
                        else if (Chars_2_Word (pak->sub_cmd) & TCP_AUTO_RESPONSE_MASK)
                        {
                            R_undraw ();
                            M_print (i18n (194, "Auto-response message for %s:\n"), ContactFindNick (cont->uin));
                            M_print (MESSCOL "%s\n" NOCOL, ((UBYTE *)pak) + TCP_MSG_OFFSET);
                            R_redraw ();
                        }
                        if (uiG.Verbose)
                        {
                            R_undraw ();
                            M_print (i18n (806, "Received ACK for message (seq %04X) from %s\n"),
                                     seq_in, cont->nick);
                            R_redraw ();
                        }
                        break;
                    
                    case TCP_CMD_CANCEL:
                            /* Delete packet from recv queue */
                        Check_Queue (seq_in, tcp_rq);
                        if (uiG.Verbose)
                        {
                            R_undraw ();
                            M_print (i18n (807, "Cancelled incoming message (seq %04X) from %s\n"),
                                     seq_in, cont->nick);
                            R_redraw ();
                        }
                        break;

                    default:
                        /* Store the msg in the recv queue for handling later */            
                        msg_in->seq = seq_in;
                        msg_in->attempts = 0;
                        msg_in->exp_time = 0;
                        msg_in->body = (UBYTE *) pak;
                        msg_in->len = size;
                        msg_queue_push (msg_in, tcp_rq);                
                        ssG.seq_tcp--;
                    break;
                }
            }
        }
        
        i++;
        
        M_set_timeout (0, 100000);
        M_select_init();
        M_Add_rsocket (cont->sok);
        M_select();
    }

    /* Set the next resend time */
    if ((queued_msg = msg_queue_peek (tcp_sq)) != NULL)
        ssG.next_tcp_resend = queued_msg->exp_time;
    else
        ssG.next_tcp_resend = INT_MAX;

    /* Now handle all packets in the recv queue */
    while ((msg_in = msg_queue_pop (tcp_rq)) != NULL)
    {
        pak = (TCP_MSG_PAK *) msg_in->body;
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
                Send_TCP_Ack (cont->sok, Chars_2_Word(pak->seq), Chars_2_Word(pak->sub_cmd), TRUE);
                break;

            /* Automatically reject file xfer and chat requests
                 as these are not implemented yet. */ 
            case CHAT_MESS:
            case FILE_MESS:
                Send_TCP_Ack (cont->sok, Chars_2_Word(pak->seq), Chars_2_Word(pak->sub_cmd), FALSE);
                break;

            /* Regular messages */
            default:
                ssG.last_recv_uin = cont->uin;
                
                Time_Stamp ();
                M_print ("\a " CYAN BOLD "%10s" COLNONE " ", ContactFindName (cont->uin));
                
/*                if (Chars_2_Word(pak->sub_cmd) & MASS_MESS_MASK)
                    M_print (" - Sofortnachricht \a ");
                else
                    M_print (" - Sofortmassennachricht \a ");
                if (uiG.Verbose)
                    M_print (i18n (808, " Typ    = %04x\t"), Chars_2_Word (pak->msg_type));*/
    
                Do_Msg (cont->sok, Chars_2_Word (pak->sub_cmd), Chars_2_Word (pak->size), 
                        &((char *)pak)[TCP_MSG_OFFSET], uin, 1);
                M_print ("\n");

                Send_TCP_Ack (cont->sok, Chars_2_Word(pak->seq),
                              Chars_2_Word(pak->sub_cmd), TRUE);
        }
        free (pak);
        free (msg_in);
        R_redraw ();
    }
}

/*
 * Sends a message via TCP.
 * Adds it to the resend queue until acked.
 */
BOOL TCPSendMsg (SOK_T srv_sok, UDWORD uin, char *msg, UWORD sub_cmd)
{
    Contact *cont;
    TCP_MSG_PTR pak;
    struct msg *q_item;
    int paksize;
    int msgtype = 0;

    cont = ContactFind (uin);
    if (!cont)
        return 0;
    if (cont->status == STATUS_OFFLINE)
        return 0;
    if (cont->sokflag < 0)
        return 0;

    paksize = sizeof(TCP_MSG_PAK) + strlen(msg) + 1 + 8; 
                    /*   header   +     msg  + null + footer */
    q_item = (struct msg *) malloc (sizeof (struct msg));

    /* Open a connection if necessary */
    if (!cont->sokflag)
    {
        if (TCPConnect (cont) < 0)
/*        Send_TCP_Init (uin);*/

        /* Did we connect? If not, send by server */
/*        if (cont->sokflag <= 0 && srv_sok > 0)*/
            return 0;
    }     

    Time_Stamp ();
    M_print (" " COLSENT "%10s" COLNONE " ... %s\n", ContactFindName (uin), MsgEllipsis (msg));
    
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
    q_item->exp_time = time (NULL);

/*    if (cont->session_id > 0)
    {*/
        /* We're still initializing, so the packet can't be sent yet.
             Set the exp_time really small so it can be sent ASAP.         */
/*        q_item->exp_time = time(NULL) + 1;
    }
    else
    {
        q_item->exp_time = time(NULL) + 10;
    }*/
    q_item->dest_uin = uin;
    q_item->len = paksize;
    q_item->body = (UBYTE *) pak;
    msg_queue_push (q_item, tcp_sq);
    if (msg_queue_peek(tcp_sq) == q_item)
        ssG.next_tcp_resend = q_item->exp_time;

    /* Are we still initializing? If so, don't try to send now. */
/*    if (cont->session_id <= 0)
    {
        if (Se nd_TCP_Pak (cont->sok, pak, paksize) < 0 && srv_sok > 0)
        {*/
            /* Yuck... send it through the server */
/*            M_print (i18n (791, "Error sending TCP packet."));
            Check_Queue (ssG.seq_tcp + 1, tcp_sq);
            return 0;
        }
    }*/
    return 1;
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
int Send_TCP_Ack (SOK_T sok, UWORD seq, UWORD sub_cmd, BOOL accept)
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
    if (TCPSendPacket (sok, pak, size) < 0)
    {
        M_print (i18n (791, "Error sending TCP packet."));
        return -1;
    }
    free (msg);
    return 1;
}

/*
 * Resends TCP packets if necessary
 */
void Do_TCP_Resend (SOK_T srv_sok)
{
    struct msg *queued_msg;
    TCP_MSG_PTR pak;
    Contact *cont;

    if ((queued_msg = msg_queue_pop (tcp_sq)) != NULL)
    {
        queued_msg->attempts++;
        pak = (TCP_MSG_PAK *) queued_msg->body;
        cont = ContactFind (queued_msg->dest_uin);

        if (queued_msg->attempts <= MAX_RETRY_ATTEMPTS)
        {
            if (uiG.Verbose)
            {
                R_undraw ();
                M_print (i18n (810, "Resending TCP message with SEQ num %04X to %s."),
                         queued_msg->seq, cont->nick);
                M_print (i18n (811, " (Attempt #%d.)\n"), queued_msg->attempts);
                R_redraw ();
            }

            if ((cont->sokflag > 0) && (cont->session_id <= 0))
            {
                if (cont->sokflag == 10)
                    TCPSendPacket (cont->sok, (UBYTE *) pak, queued_msg->len);
                queued_msg->exp_time = time (NULL) + 10;
                msg_queue_push (queued_msg, tcp_sq);
            }
            else
            {
                /* Send by the server (except for auto-response messages) */
                if ((Chars_2_Word(pak->msg_type) && TCP_AUTO_RESPONSE_MASK) == 0)
                {
                    UDPSendMsg (srv_sok, cont->uin,
                                  ((UBYTE *) pak) + TCP_MSG_OFFSET,
                                    Chars_2_Word (pak->msg_type));
                    free (queued_msg->body);
                    free (queued_msg);
                }
            }
        }
        else
        {
            if (TCP_CMD_MESSAGE == Chars_2_Word (pak->cmd)) 
            {
                 R_undraw ();
                 M_print (i18n (195, "TCP timeout; sending message to %s after %d send attempts."),
                          cont->nick, queued_msg->attempts - 1);
                 R_redraw ();

                 UDPSendMsg (srv_sok, cont->uin,
                                  ((UBYTE *) pak) + TCP_MSG_OFFSET, Chars_2_Word(pak->msg_type));
                 free(queued_msg->body);
                 free(queued_msg);
            }
        }

    }
    else
    {
        ssG.next_tcp_resend = INT_MAX;
    }
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
 * i18n (98, " ") i18n
 * i18n (59, " ") i18n
 * i18n (626, " ") i18n (627, " ") i18n
 * i18n (634, " ") i18n
 * i18n (762, " ") i18n
 */
