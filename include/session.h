/* $Id$ */

#ifndef MICQ_SESSION_H
#define MICQ_SESSION_H

typedef void (jump_sess_f)(Session *sess);

struct Session_s
{
        UWORD     type;           /* connection type - TYPE_*                 */
        UBYTE     flags;          /* connection flags                         */
        UBYTE     ver;            /* protocol version in this session         */
        UDWORD    uin;            /* current user identification number       */
        UDWORD    status;         /* status of uin                            */
        char     *server;         /* the remote server name                   */
        UDWORD    port;           /* the port the server is listening on      */
        char     *passwd;         /* the password for this user               */
        UDWORD    ip;             /* the remote ip (host byte order)          */

        SOK_T     sok;            /* socket for connection to server          */
        UWORD     connect;        /* connection setup status                  */
        Packet   *incoming;       /* packet we're receiving                   */

        UDWORD    our_local_ip;   /* LAN-internal IP (host byte order)        */
        UDWORD    our_outside_ip; /* the IP address the server sees from us   */

        UDWORD    our_session;    /* session ID                               */
        UWORD     our_seq;        /* current primary sequence number          */
        UWORD     our_seq2;       /* current secondary sequence number        */
        UDWORD    our_seq3;       /* current old-ICQ sequence number          */

        UDWORD    stat_real_pak_sent;
        UDWORD    stat_real_pak_rcvd;
        UDWORD    stat_pak_sent;
        UDWORD    stat_pak_rcvd;

        PreferencesSession *spref; /* preferences for this session */
        Session            *assoc; /* associated UDP <-> TCP or parent TCP session */
        Session            *parent;/* parent session */
        
        jump_sess_f *dispatch;     /* function to call on select() */
        jump_sess_f *reconnect;    /* function to call for reconnect */
        jump_sess_f *utilio;       /* private to util_io.c */
};

#define CONNECT_MASK       0x00ff
#define CONNECT_OK         0x0080
#define CONNECT_FAIL       0x0100
#define CONNECT_SELECT_R   0x0200
#define CONNECT_SELECT_W   0x0400
#define CONNECT_SELECT_X   0x0800
#define CONNECT_SOCKS_ADD  0x1000
#define CONNECT_SOCKS      0xf000

Session    *SessionC     (void);
Session    *SessionClone (Session *sess);
void        SessionInit  (Session *sess);
Session    *SessionNr    (int i);
Session    *SessionFind  (UWORD type, UDWORD uin, const Session *parent);
void        SessionClose (Session *sess);
const char *SessionType  (Session *sess);

/*
                            TYPE_SERVER
                           ^ |       ^
                          /  a       |
                         p   |       p
                        /    V       |
                   TYPE_LISTENER   TYPE_FILELISTENER
                        ^                 ^
                        |                 |
                        p                 p
                        |                 |
                TYPE_CIRECT(uin)   TYPE_FILEDIRECT(uin)
                                          |  ^
                                          a  |
                                          |  p
                                          V  |
                                        TYPE_FILE

*/


#define TYPEF_ANY_SERVER    1  /* any server connection  */
#define TYPEF_SERVER_OLD    2  /* " && ver == 5          */
#define TYPEF_SERVER        4  /* " && var > 6           */
#define TYPEF_ANY_PEER      8  /* any peer connection    */
#define TYPEF_ANY_DIRECT   16  /* " && established       */
#define TYPEF_ANY_LISTEN   32  /* " && listening         */
#define TYPEF_ANY_MSG      64  /* " && for messages      */
#define TYPEF_ANY_FILE    128  /* " && for file transfer */
#define TYPEF_ANY_CHAT    256  /* " && for chat          */
#define TYPEF_FILE        512  /* any file io            */

/* any sess->type may be only any of those values:
 * do not use the flags above unless you _really_ REALLY know what you're doing
 */
#define TYPE_SERVER_OLD   (TYPEF_ANY_SERVER | TYPEF_SERVER_OLD)
#define TYPE_SERVER       (TYPEF_ANY_SERVER | TYPEF_SERVER)
#define TYPE_LISTEN       (TYPEF_ANY_PEER | TYPEF_ANY_MSG  | TYPEF_ANY_LISTEN)
#define TYPE_DIRECT       (TYPEF_ANY_PEER | TYPEF_ANY_MSG  | TYPEF_ANY_DIRECT)
#define TYPE_FILELISTEN   (TYPEF_ANY_PEER | TYPEF_ANY_FILE | TYPEF_ANY_LISTEN)
#define TYPE_FILEDIRECT   (TYPEF_ANY_PEER | TYPEF_ANY_FILE | TYPEF_ANY_DIRECT)
#define TYPE_CHATLISTEN   (TYPEF_ANY_PEER | TYPEF_ANY_CHAT | TYPEF_ANY_LISTEN)
#define TYPE_CHATDIRECT   (TYPEF_ANY_PEER | TYPEF_ANY_CHAT | TYPEF_ANY_DIRECT)
#define TYPE_FILE         TYPEF_FILE

#define ASSERT_LISTEN(s)      (assert (s), assert ((s)->type == TYPE_LISTEN))
#define ASSERT_DIRECT(s)      (assert (s), assert ((s)->type == TYPE_DIRECT))
#define ASSERT_DIRECT_FILE(s) (assert (s), assert ((s)->type == TYPE_FILE || (s)->type == TYPE_DIRECT))
#define ASSERT_FILE(s)        (assert (s), assert ((s)->type == TYPE_FILE))
#define ASSERT_SERVER(s)      (assert (s), assert ((s)->type == TYPE_SERVER))
#define ASSERT_ANY_LISTEN(s)  (assert (s), assert ((s)->type & TYPEF_ANY_LISTEN))
#define ASSERT_ANY_SERVER(s)  (assert (s), assert ((s)->type & TYPEF_ANY_SERVER))

#endif
