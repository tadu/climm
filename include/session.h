
#ifndef MICQ_SESSION_H
#define MICQ_SESSION_H

typedef void (jump_sess_f)(Session *sess);

struct Session_s
{
        UBYTE   type;           /* connection type - TYPE_{SERVER{,_OLD},PEER,DIRECT}*/
        UBYTE   ver;            /* protocol version in this session; either 5,6 or 8 */
        UDWORD  uin;            /* current user identification number                */
        UDWORD  status;         /* status of uin                                     */
        char   *server;         /* the remote server name                            */
        UDWORD  port;           /* the port the server is listening on               */
        char   *passwd;         /* the password for this user                        */
        UDWORD  ip;             /* the remote ip (host byte order)                   */

        SOK_T   sok;            /* socket for connection to server                   */
        UWORD   connect;        /* connection setup status                           */
        Packet *incoming;       /* packet we're receiving                            */

        UDWORD  our_local_ip;   /* LAN-internal IP (host byte order)                 */
        UDWORD  our_outside_ip; /* the IP address the server sees from us            */

        UDWORD  our_session;    /* session ID                                        */
        UWORD   our_seq;        /* current primary sequence number                   */
        UWORD   our_seq2;       /* current secondary sequence number                 */

        UDWORD  stat_real_pak_sent;
        UDWORD  stat_real_pak_rcvd;
        UDWORD  stat_pak_sent;
        UDWORD  stat_pak_rcvd;

        PreferencesSession *spref; /* preferences for this session */
        Session            *assoc; /* associated UDP <-> TCP or parent TCP session */
        
        jump_sess_f *dispatch;
};

#define CONNECT_MASK      255
#define CONNECT_OK        128
#define CONNECT_FAIL      256
#define CONNECT_SELECT_R 1024
#define CONNECT_SELECT_W 2048
#define CONNECT_SELECT_X 4096

Session *SessionC     (void);
Session *SessionClone (Session *sess);
void     SessionInit  (Session *sess);
Session *SessionNr    (int i);
Session *SessionFind  (UBYTE type, UDWORD uin);
void     SessionClose (Session *sess);


#endif
