/*
 * This file handles TCP client-to-client communications.
 *
 *  Author: James Schofield (jschofield@ottawa.com) 22 Feb 2001
 */

/*
.#include "datatype.h"
.#include "mselect.h"
.#include "msg_queue.h"
.#include "mreadline.h"
.#include "util_ui.h"
.#include "util.h"
*/

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
#include "tcp.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

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

/*
 *  Establishes a TCP connection 
 *  On error, returns -1.
 */
int Open_TCP_Connection (CONTACT_PTR cont)
{
    struct sockaddr_in sin;

    cont->sok = socket(AF_INET, SOCK_STREAM, 0);
  
    if (uiG.Verbose)
        M_print (i18n (779, "Opening TCP connection to %s.\n") , cont->nick);

    if (cont->sok <= 0)
    {
        M_print (i18n (780, "Error creating socket.\n"));
        return -1;
    }

    sin.sin_family = AF_INET;
    sin.sin_port = htons (cont->port);

    /* Try connecting to the external (current) IP first. */
    sin.sin_addr.s_addr = Chars_2_DW (cont->current_ip);

    if (uiG.Verbose)
        M_print (i18n (781, "Trying %s:%d..."), inet_ntoa (sin.sin_addr), cont->port);

    if (connect (cont->sok, (struct sockaddr *) &sin, 
                          sizeof (struct sockaddr)) < 0)
    {
        /* Try the internal (other) IP now */
        cont->sok = socket (AF_INET, SOCK_STREAM, 0);
        if (cont->sok <= 0)
        {
            M_print (i18n (780, "Error creating socket.\n"));
            return -1;
        }
    
        sin.sin_family = AF_INET;
        sin.sin_port = htons (cont->port);
        sin.sin_addr.s_addr = Chars_2_DW (cont->other_ip);

        if (uiG.Verbose)
            M_print (i18n (782, " failure.\nTrying %s:%d..."), inet_ntoa (sin.sin_addr), cont->port);

        if (connect (cont->sok, (struct sockaddr *) &sin,
                            sizeof (struct sockaddr)) < 0)
        {
             /* Internal didn't work either.. :(  */
             if (uiG.Verbose)
                 M_print (i18n (783, " failure.\n"));
             M_print (i18n (784, "Error connecting to remote user.\n"));
             return -1;
        }
    }
    if (uiG.Verbose)
        M_print (i18n (785, " success.\n"));
    return 0;
}

/*
 *  Receives a TCP packet 
 *  Returns size of packet received.
 *  On error, returns -1 and closes socket.
 */
int Recv_TCP_Pak (SOK_T sok, void **pak)
{
    UBYTE s[2];
    int recvSize = 0;
    int offset = 0;
    UWORD size;

    /* Read size of packet */
    if (sockread (sok, s, 2) < 0)
    {
        /* error */
        close (sok);
        return (-1);
    }

    size = Chars_2_Word (s);
    if (size <= 0)
    {
        close (sok);
        return (-1);
    }

    /* Read packet */
    *pak = malloc (size);
    do
    {
        recvSize = sockread (sok, *pak + offset, (size - offset));
        if (recvSize < 0)
        {
            /* error */
            close (sok);
            return (-1);
        }
    } while ((offset += recvSize) < size);
    /* ensure we get entire packet */
    return size;
    
}

/*
 * Sends a TCP packet
 * Returns 1 on success.
 * On error, returns -1 and closes socket
 */
int Send_TCP_Pak (SOK_T sok, void *pak, UWORD size)
{
    UBYTE s[2];
    int sendSize = 0;
    int offset = 0;
    
    /* Send size of packet */
    Word_2_Chars (s, size);
    if (sockwrite (sok, s, 2) < 0)
    {
        /* error */
        close (sok);
        return (-1);
    }

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

    /* Send Packet */
    do
    {
        sendSize = sockwrite (sok, pak + offset, (size - offset));
        if (sendSize < 0)
        {
            /* error */
            close (sok);
            return (-1);
        }
    } while ((offset += sendSize) < size);
    /* ensure entire packet is sent */

    return (1);
}

/*
 * Saves a "dead" sok with the contact member.
 * To be called after the sok is closed.
 */
void Save_Dead_Sok (CONTACT_PTR cont)
{
    cont->sok = -1;
    cont->session_id = 0;    
}

/*
 * Sends a TCP initialization packet.
 * Must be sent to initiate communication or
 * to respond to other side if they initiate
 */
void Send_TCP_Init (UDWORD dest_uin)
{
    CONTACT_PTR cont;
    TCP_INIT_PAK pak;

    cont = UIN2Contact (dest_uin);
    
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

    if (cont->session_id <= 0)
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

    /* If we are initiating a connection must be established first */
    if (cont->sok <= 0)
    {
        if (Open_TCP_Connection (cont) == -1)
        {
            M_print (i18n (786, "Error establishing a direct TCP connection.\n"));
            return;
        }
    }

    if (Send_TCP_Pak (cont->sok, (void *) &pak, sizeof(pak)) < 0)
    {
        /* error; sok is dead */
        M_print (i18n (787, "Error sending TCP packet.\n"));
        Save_Dead_Sok (cont);
        return;
    }            

    /* Other side should ack this immediately */
    if (Wait_TCP_Init_Ack (cont->sok) < 0)
    {
        Save_Dead_Sok (cont);
    }

}

/*
 * Waits for the other side to ack
 * the init packet
 */
int Wait_TCP_Init_Ack (SOK_T sok)
{
    TCP_INIT_ACK_PTR pak;

    if (Recv_TCP_Pak (sok, (void *) &pak) < 0)
    {
        M_print (i18n (788, "Error receiving TCP packet."));
        return (-1);
    }

    if (pak->cmd != TCP_INIT_ACK)
    {
        M_print (i18n (789, "Received malformed packet."));
        close (sok);
        return (-1);
    }

    return 0;
}

/*
 * Acks the init packet
 */
void TCP_Init_Ack (SOK_T sok)
{
    TCP_INIT_ACK_PAK pak;

    pak.cmd = TCP_INIT_ACK;
    bzero (pak.X1, 3);

    if (uiG.Verbose)
        M_print (i18n (790, "Sending init ACK...\n"));

    if (Send_TCP_Pak (sok, (void *) &pak, sizeof(pak)) < 0)
    {
        M_print (i18n (791, "Error sending TCP packet."));
    }
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

void Recv_TCP_Init (SOK_T sok)
{
    TCP_INIT_PTR pak;
    CONTACT_PTR cont;

    if (Recv_TCP_Pak (sok, (void *) &pak) < 0)
    {
        M_print (i18n (788, "Error receiving TCP packet."));
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
    cont = UIN2Contact (Chars_2_DW (pak->uin));
    if (cont != (CONTACT_PTR) NULL)
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
                Save_Dead_Sok (cont);
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
}

/*
 * Handles all incoming TCP traffic.
 * Queues incoming packets, ignoring resends
 * and deleting CANCEL requests from the queue.
 */
void Handle_TCP_Comm (UDWORD uin)
{
    CONTACT_PTR cont;
    TCP_MSG_PTR pak;
    struct msg *msg_in;
    int size;
    int i = 0;
    UWORD seq_in = 0;
    struct msg *queued_msg;
     
    cont = UIN2Contact (uin);
    msg_in = (struct msg *) malloc (sizeof (struct msg));

    /* Recv all packets before doing anything else.
         The objective is to delete any packets CANCELLED by the remote user. */
    while (M_Is_Set (cont->sok) && i <= TCP_MSG_QUEUE)
    {
        if ((size = Recv_TCP_Pak (cont->sok, (void *) &pak)) <0)
        {
            /* Remote user disconnected */
            if (uiG.Verbose)
            {
                R_undraw ();
                M_print (i18n (797, "User %s%s%s closed socket.\n"), COLCONTACT, cont->nick, COLNONE);
                R_redraw ();
            }
            Save_Dead_Sok (cont);
            return;
        }
        else 
        {
            if (Decrypt_Pak ((UBYTE *) pak, size) < 0)
            {
                /* Decryption error - wrong TCP version? */
                R_undraw ();
                M_print (i18n (789, "Received malformed packet."));
                close (cont->sok);
                Save_Dead_Sok (cont);
                R_redraw ();
                return;
            }
    
            if (uiG.Verbose & 4)
            {
                R_undraw ();
                Time_Stamp ();
                M_print (" \x1b«" COLSERV "");
                M_print (i18n (778, "Incoming TCP packet:"));
/*                M_print (" %04X %08X:%08X %04X (", Chars_2_Word (pak.head.ver),
                         Chars_2_DW (pak.head.session), Chars_2_DW (pak.head.seq),
                         Chars_2_Word (pak.head.cmd));
                Print_CMD (Chars_2_Word (pak.head.cmd));*/
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
                            M_print (i18n (798, "Message sent direct to %s%s%s!\n"),
                                     COLCONTACT, cont->nick, COLNONE);
                            R_redraw ();
                        }
                        else if (Chars_2_Word (pak->sub_cmd) & TCP_AUTO_RESPONSE_MASK)
                        {
                            R_undraw ();
                            M_print (i18n (799, "Auto-response message for "));
                            Print_UIN_Name (cont->uin);
                            M_print (":\n" MESSCOL "%s\n" NOCOL, ((UBYTE *)pak) + TCP_MSG_OFFSET);
                            R_redraw ();
                        }
                        if (uiG.Verbose)
                        {
                            R_undraw ();
                            M_print (i18n (806, "Received ACK for message (seq %04X) from %s"),
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
                            M_print (i18n (807, "Cancelled incoming message (seq %04X) from %s"),
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
                M_print ("\a " CYAN BOLD "%10s" COLNONE " ", UIN2Name (cont->uin));
                
/*                if (Chars_2_Word(pak->sub_cmd) & MASS_MESS_MASK)
                    M_print (" - Sofortnachricht \a ");
                else
                    M_print (" - Sofortmassennachricht \a ");
                if (uiG.Verbose)
                    M_print (i18n (808, " Typ    = %04x\t"), Chars_2_Word (pak->msg_type));*/
    
                Do_Msg (cont->sok, Chars_2_Word (pak->sub_cmd), Chars_2_Word (pak->size), 
                        &((char *)pak)[TCP_MSG_OFFSET], uin);
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
void Send_TCP_Msg (SOK_T srv_sok, UDWORD uin, char *msg, UWORD sub_cmd)
{
    CONTACT_PTR cont;
    TCP_MSG_PTR pak;
    struct msg *q_item;
    int paksize;
    int msgtype = 0;

    paksize = sizeof(TCP_MSG_PAK) + strlen(msg) + 1 + 8; 
                    /*   header   +     msg  + null + footer */
    q_item = (struct msg *) malloc (sizeof (struct msg));

    cont = UIN2Contact (uin);
    if (cont == NULL) return;

    Time_Stamp ();
    M_print (" " COLSENT "%10s" COLNONE " " MSGTCPSENTSTR "%s\n", UIN2Name (uin), MsgEllipsis (msg));
    
    /* Open a connection if necessary */
    if (cont->sok <= 0)
    { 
        Send_TCP_Init (uin);

        /* Did we connect? If not, send by server */
        if (cont->sok <= 0 && srv_sok > 0)
        {
            icq_sendmsg_srv (srv_sok, uin, msg, sub_cmd);
            return;
        }
    }     

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

    if (cont->session_id > 0)
    {
        /* We're still initializing, so the packet can't be sent yet.
             Set the exp_time really small so it can be sent ASAP.         */
        q_item->exp_time = time(NULL) + 1;
    }
    else
    {
        q_item->exp_time = time(NULL) + 10;
    }
    q_item->dest_uin = uin;
    q_item->len = paksize;
    q_item->body = (UBYTE *) pak;
    msg_queue_push (q_item, tcp_sq);
    if (msg_queue_peek(tcp_sq) == q_item)
        ssG.next_tcp_resend = q_item->exp_time;

    /* Are we still initializing? If so, don't try to send now. */
    if (cont->session_id <= 0)
    {
        if (Send_TCP_Pak (cont->sok, pak, paksize) < 0 && srv_sok > 0)
        {
            /* Yuck... send it through the server */
            M_print (i18n (791, "Error sending TCP packet."));
            Check_Queue (ssG.seq_tcp + 1, tcp_sq);
            icq_sendmsg_srv (srv_sok, uin, msg, sub_cmd);
        }
    }
}

/*
 * Requests the auto-response message
 * from remote user.
 */
void Get_Auto_Resp (UDWORD uin)
{
    CONTACT_PTR cont;
    UWORD sub_cmd;
    char *msg = "...";    /* seem to need something (anything!) in the msg */

    cont = UIN2Contact (uin);

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
    {
        M_print (i18n (809, "Unable to retrieve auto-response message for "));
        Print_UIN_Name (uin);
        M_print (" finden.\n");
    }
    else
    {
        Send_TCP_Msg (0, uin, msg, sub_cmd);
    }
}

/*
 * Acks a TCP packet.
 */
void Send_TCP_Ack (SOK_T sok, UWORD seq, UWORD sub_cmd, BOOL accept)
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
    Word_2_Chars (pak->status, (accept ? TCP_STAT_OK : TCP_STAT_REFUSE)); 
    Word_2_Chars (pak->msg_type, TCP_MSG_AUTO);
    Word_2_Chars (pak->size, strlen(msg) + 1);
    strcpy (&((char *) pak)[TCP_MSG_OFFSET], msg);    
    DW_2_Chars (&((char *) pak)[TCP_MSG_OFFSET + strlen(msg) + 1], COL_FG);
    DW_2_Chars (&((char *) pak)[TCP_MSG_OFFSET + strlen(msg) + 5], COL_BG);

    Encrypt_Pak ((UBYTE *) pak, size);
    if (Send_TCP_Pak (sok, pak, size) < 0)
        M_print (i18n (791, "Error sending TCP packet."));

    free (msg);
}

/*
 * Resends TCP packets if necessary
 */
void Do_TCP_Resend (SOK_T srv_sok)
{
    struct msg *queued_msg;
    TCP_MSG_PTR pak;
    CONTACT_PTR cont;

    if ((queued_msg = msg_queue_pop (tcp_sq)) != NULL)
    {
        queued_msg->attempts++;
        pak = (TCP_MSG_PAK *) queued_msg->body;
        cont = UIN2Contact (queued_msg->dest_uin);

        if (queued_msg->attempts <= MAX_RETRY_ATTEMPTS)
        {
            if (uiG.Verbose)
            {
                R_undraw ();
                M_print (i18n (810, "\nResending TCP message with SEQ num %04X to %s.\t"),
                         queued_msg->seq, cont->nick);
                M_print (i18n (811, " (Attempt #%d.)"), queued_msg->attempts);
                R_redraw ();
            }

            if ((cont->sok >= 0) && (cont->session_id <= 0))
            {
                Send_TCP_Pak (cont->sok, (UBYTE *) pak, queued_msg->len);
                queued_msg->exp_time = time(NULL) + 10;
                msg_queue_push (queued_msg, tcp_sq);
            }
            else
            {
                /* Send by the server (except for auto-response messages) */
                if ((Chars_2_Word(pak->msg_type) && TCP_AUTO_RESPONSE_MASK) == 0)
                {
                    icq_sendmsg_srv (srv_sok, cont->uin,
                                  ((UBYTE *) pak) + TCP_MSG_OFFSET,
                                    Chars_2_Word(pak->msg_type));
                    free (queued_msg->body);
                    free (queued_msg);
                }
            }
        }
        else
        {
            if (TCP_CMD_MESSAGE    == Chars_2_Word (pak->cmd)) 
            {
                 R_undraw ();
                 M_print (i18n (812, "\nTCP timeout; sending message to "));
                 Print_UIN_Name (cont->uin);
                 M_print (i18n (813, " after %d send attempts."), queued_msg->attempts - 1);
                 R_redraw ();

                 icq_sendmsg_srv (srv_sok, cont->uin,
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
