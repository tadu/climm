
#ifndef MICQ_SESSION_H
#define MICQ_SESSION_H

typedef void (jump_sess_f)(Session *sess);

struct Session_s
{
        UBYTE   ver;            /* protocol version in this session; either 5 or 8 */
        UDWORD  uin;            /* current user identification number              */
        UDWORD  status;         /* status of uin                                   */
        char   *server;         /* the remote server name                          */
        UDWORD  server_port;    /* the port the server is listening on             */
        char   *passwd;         /* the password for this user                      */

        SOK_T   sok;            /* socket for connection to server                 */
        UWORD   connect;        /* connection setup status                         */
        Packet *incoming;       /* packet we're receiving                          */

        UDWORD  our_local_ip;   /* LAN-internal IP (host byte order)               */
        UDWORD  our_outside_ip; /* the IP address the server sees from us          */
        UDWORD  our_port;       /* the port to make TCP connections on             */

        UDWORD  our_session;    /* session ID                                      */
        UWORD   our_seq;        /* current primary sequence number                 */
        UWORD   our_seq2;       /* current secondary sequence number               */
#ifdef TCP_COMM
                                /* Note: TCP is going to be it's own session!!
                                        which will obsolete the below              */
        SOK_T   tcpsok;         /* TCP: listening socket                           */
        UWORD   seq_tcp;        /* TCP: tcp sequence number                        */
#endif
                                                /* statistics */
        UDWORD  stat_real_pak_sent;
        UDWORD  stat_real_pak_rcvd;
        UDWORD  stat_pak_sent;
        UDWORD  stat_pak_rcvd;

        PreferencesSession *spref; /* preferences for this session */
        Session            *assoc; /* associated UDP <-> TCP or parent TCP session */
        
        jump_sess_f *dispatch;
};

#define CONNECT_MASK      127
#define CONNECT_OK        128
#define CONNECT_TCP       256
#define CONNECT_SELECT_R 1024
#define CONNECT_SELECT_W 2048
#define CONNECT_SELECT_X 4096

Session *SessionC (void);
void     SessionClose (Session *sess);
Session *SessionFind (UBYTE type, UDWORD uin);

Session *SessionNr (int i);

#endif
