
#ifndef MICQ_SESSION_H
#define MICQ_SESSION_H

struct Session_s;

typedef struct Session_s {
                                                /* the connection */
        char   *server;         /* the remote server name                             */
        UDWORD  server_port;    /* the port the server is listening on                */
        char   *passwd;         /* the password for this user                         */
        UDWORD  uin;            /* current user identification number                 */
        UBYTE   ver;            /* protocol version in this session; either 5, 7 or 8 */
        SOK_T   sok;            /* socket for connection to server                    */

                                                /* we ourselves */
        UDWORD  our_local_ip;   /* what we consider our IP; LAN-internal              */
        UDWORD  our_outside_ip; /* the IP address the server sees from us             */
        UDWORD  our_port;       /* the port to make TCP connections on                */
       
                                                /* session stuff */
        UDWORD  our_session;    /* session ID                                         */
        UWORD   our_seq;        /* current primary sequence number                    */
        UWORD   our_seq2;       /* current secondary sequence number                  */
#ifdef TCP_COMM
                                /* Note: TCP is going to be it's own session!!
                                        which will obsolete the below                 */
        SOK_T   tcpsok;         /* TCP: listening socket                              */
        UWORD   seq_tcp;        /* TCP: tcp sequence number                           */
#endif
                                                /* statistics */
        UDWORD  stat_real_pak_sent;
        UDWORD  stat_real_pak_rcvd;
        UDWORD  stat_pak_sent;
        UDWORD  stat_pak_rcvd;

        struct Session_s *assoc; /* associated UDP <-> TCP or parent TCP session */

        UDWORD set_status;
        BOOL Done_Login;       /* this session logged in?                           */
        unsigned int away_time, away_time_prev;
        char*  last_message_sent;
        UDWORD last_message_sent_type;
        UDWORD last_recv_uin;

        BOOL Quit;             /* This doesn't belong here */
} Session;

#endif
