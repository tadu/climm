
#ifndef MICQ_SESSION_H
#define MICQ_SESSION_H

/* session global state variables */
typedef struct {
        UDWORD uin;            /* current user identification number                 */
        UBYTE  ver;            /* protocol version in this session; either 5, 7 or 8 */

        SOK_T  sok;            /* socket for connection to server                    */
        UDWORD our_session;    /* session ID                                         */
        UWORD  seq_num;        /* current primary sequence number                    */
        UWORD  seq_num2;       /* current secondary sequence number                  */
#ifdef TCP_COMM
        SOK_T  tcpsok;         /* TCP: socket to server                              */
        UWORD  seq_tcp;        /* TCP: tcp sequence number                           */
#endif
        UDWORD our_ip;         /* what we consider our IP; LAN-internal              */
        UDWORD our_outside_ip; /* the IP address tghe server sees from us            */
        UDWORD our_port;       /* the port to make TCP connections on                */
        UDWORD remote_port;    /* the port the server is listening on                */
        char server[100];      /* the server's host name                             */

        char passwd[100];      /* the password for this user                         */
        UDWORD set_status;

        BOOL Done_Login;       /* this session logged in?                           */
        BOOL Quit;             /* This doesn't belong here */
        
/******* should use & 0x3ff in the indexing of all references to last_cmd */
        UWORD last_cmd[1024];  /* command issued for the first */
                               /* 1024 SEQ #'s                 */

        BOOL serv_mess[1024];  /* used so that we don't get duplicate */
                               /* messages with the same SEQ          */

/* statistics */
        unsigned int away_time, away_time_prev;
        UDWORD real_packs_sent;
        UDWORD real_packs_recv;
        UDWORD Packets_Sent;
        UDWORD Packets_Recv;
        char*  last_message_sent;
        UDWORD last_message_sent_type;
        UDWORD last_recv_uin;
} Session;

#endif
