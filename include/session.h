/* $Id$ */

#ifndef MICQ_UTIL_CONNECTION_H
#define MICQ_UTIL_CONNECTION_H

#if ENABLE_SSL
typedef enum {
    SSL_STATUS_NA,       /* unknown / nothing done yet       */
    SSL_STATUS_FAILED,   /* SSL handshake with peer failed   */
    SSL_STATUS_OK,       /* SSL up and running               */
    SSL_STATUS_INIT,     /* SSL handshake may start          */
    SSL_STATUS_CLOSE,    /* SSL session to be terminated     */
    SSL_STATUS_REQUEST,  /* SSL session has been requested   */
    SSL_STATUS_HANDSHAKE /* SSL session handshake is ongoing */
} ssl_status_t;
#if ENABLE_GNUTLS
#include <gnutls/gnutls.h>
#else
#include <openssl/ssl.h>
#endif
#endif

typedef void (jump_conn_f)(Connection *conn);
typedef BOOL (jump_conn_err_f)(Connection *conn, UDWORD rc, UDWORD flags);

struct Connection_s
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
        void     *tlv;            /* temporary during v8 connect              */

        SOK_T     sok;            /* socket for connection to server          */
        UWORD     connect;        /* connection setup status                  */
        Packet   *incoming;       /* packet we're receiving                   */
        Packet   *outgoing;       /* packet we're sending                     */
        
        ContactGroup *contacts;   /* The contacts for this connection         */

#if ENABLE_SSL
#if ENABLE_GNUTLS
        gnutls_session ssl;       /* The SSL data structure for GnuTLS        */
#else
        SSL *ssl;                 /* SSL session struct for OpenSSL           */
#endif
        ssl_status_t ssl_status;  /* SSL status (INIT,OK,FAILED,...)          */
#endif

        UDWORD    our_local_ip;   /* LAN-internal IP (host byte order)        */
        UDWORD    our_outside_ip; /* the IP address the server sees from us   */

        UDWORD    our_session;    /* session ID                               */
        UWORD     our_seq_dc;     /* sequence number for dc and type-2        */
        UWORD     our_seq;        /* current primary sequence number          */
        UWORD     our_seq2;       /* current secondary sequence number        */
        UDWORD    our_seq3;       /* current old-ICQ sequence number          */
        
        UDWORD    len;            /* used for file transfer                   */
        UDWORD    done;           /* used for file transfer                   */

        PreferencesConnection *spref;  /* preferences for this session */
        Connection            *assoc;  /* associated session           */
        Connection            *parent; /* parent session               */
        
        UDWORD    stat_real_pak_sent;
        UDWORD    stat_real_pak_rcvd;
        UDWORD    stat_pak_sent;
        UDWORD    stat_pak_rcvd;

        jump_conn_f *open;         /* function to call to open        */
        jump_conn_f *dispatch;     /* function to call on select()    */
        jump_conn_f *reconnect;    /* function to call for reconnect  */
        jump_conn_err_f *error;    /* function to call for i/o errors */
        jump_conn_f *close;        /* function to call to close       */

        jump_conn_f *utilio;       /* private to util_io.c            */
};

#define CONNECT_MASK       0x00ff
#define CONNECT_OK         0x0080
#define CONNECT_FAIL       0x0100
#define CONNECT_SELECT_R   0x0200
#define CONNECT_SELECT_W   0x0400
#define CONNECT_SELECT_X   0x0800
#define CONNECT_SOCKS_ADD  0x1000
#define CONNECT_SOCKS      0xf000

Connection    *ConnectionC      (void);
Connection    *ConnectionClone  (Connection *conn, UWORD type);
Connection    *ConnectionNr     (int i);
Connection    *ConnectionFind   (UWORD type, UDWORD uin, const Connection *parent);
UDWORD         ConnectionFindNr (Connection *conn);
void           ConnectionClose  (Connection *conn);
const char    *ConnectionType   (Connection *conn);

/*
                            TYPE_SERVER
                           ^ |       ^
                         p/  a       p\
                         /   V         \
                TYPE_MSGLISTENER    TYPE_FILELISTENER
                        ^                 ^
                       p|                p|
                        |                 |
              TYPE_MSGDIRECT(uin)   TYPE_FILEDIRECT(uin)
                                          |  ^
                                         a| p|
                                          V  |
                                        TYPE_FILE

*/


#define TYPEF_ANY_SERVER    1  /* any server connection  */
#define TYPEF_SERVER_OLD    2  /* " && ver == 5          */
#define TYPEF_SERVER        4  /* " && ver > 6           */
#define TYPEF_ANY_PEER      8  /* any peer connection    */
#define TYPEF_ANY_DIRECT   16  /* " && established       */
#define TYPEF_ANY_LISTEN   32  /* " && listening         */
#define TYPEF_ANY_MSG      64  /* " && for messages      */
#define TYPEF_ANY_FILE    128  /* " && for file transfer */
#define TYPEF_ANY_CHAT    256  /* " && for chat          */
#define TYPEF_FILE        512  /* any file io            */
#define TYPEF_REMOTE     1024  /* remote control (socket)*/

/* any conn->type may be only any of those values:
 * do not use the flags above unless you _really_ REALLY know what you're doing
 */
#define TYPE_SERVER_OLD   (TYPEF_ANY_SERVER | TYPEF_SERVER_OLD)
#define TYPE_SERVER       (TYPEF_ANY_SERVER | TYPEF_SERVER)
#define TYPE_MSGLISTEN    (TYPEF_ANY_PEER | TYPEF_ANY_MSG  | TYPEF_ANY_LISTEN)
#define TYPE_MSGDIRECT    (TYPEF_ANY_PEER | TYPEF_ANY_MSG  | TYPEF_ANY_DIRECT)
#define TYPE_FILELISTEN   (TYPEF_ANY_PEER | TYPEF_ANY_FILE | TYPEF_ANY_LISTEN)
#define TYPE_FILEDIRECT   (TYPEF_ANY_PEER | TYPEF_ANY_FILE | TYPEF_ANY_DIRECT)
#define TYPE_CHATLISTEN   (TYPEF_ANY_PEER | TYPEF_ANY_CHAT | TYPEF_ANY_LISTEN)
#define TYPE_CHATDIRECT   (TYPEF_ANY_PEER | TYPEF_ANY_CHAT | TYPEF_ANY_DIRECT)
#define TYPE_FILE         TYPEF_FILE
#define TYPE_REMOTE       TYPEF_REMOTE

#define ASSERT_ANY_SERVER(s)  (assert (s), assert ((s)->type & TYPEF_ANY_SERVER))
#define ASSERT_SERVER(s)      (assert (s), assert ((s)->type == TYPE_SERVER))
#define ASSERT_ANY_LISTEN(s)  (assert (s), assert ((s)->type & TYPEF_ANY_LISTEN), ASSERT_ANY_SERVER ((s)->parent))
#define ASSERT_ANY_DIRECT(s)  (assert (s), assert ((s)->type & TYPEF_ANY_DIRECT), ASSERT_ANY_LISTEN ((s)->parent))
#define ASSERT_MSGLISTEN(s)   (assert (s), assert ((s)->type == TYPE_MSGLISTEN), assert ((s)->parent->assoc == (s)), ASSERT_ANY_SERVER ((s)->parent))
#define ASSERT_MSGDIRECT(s)   (assert (s), assert ((s)->type == TYPE_MSGDIRECT), ASSERT_MSGLISTEN ((s)->parent))
#define ASSERT_FILELISTEN(s)  (assert (s), assert ((s)->type == TYPE_FILELISTEN), ASSERT_ANY_SERVER ((s)->parent))
#define ASSERT_FILEDIRECT(s)  (assert (s), assert ((s)->type == TYPE_FILEDIRECT), ASSERT_FILELISTEN ((s)->parent))
#define ASSERT_FILE(s)        (assert (s), assert ((s)->type == TYPE_FILE), assert ((s)->parent->assoc == (s)), ASSERT_FILEDIRECT ((s)->parent))

#define CONNERR_WRITE       1
#define CONNERR_READ        2

#endif /* MICQ_UTIL_CONNECTION_H */
