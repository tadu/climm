/* $Id$ */

#ifndef CLIMM_UTIL_CONNECTION_H
#define CLIMM_UTIL_CONNECTION_H

#include "util_opts.h"
#if ENABLE_IKS
#include <iksemel.h>
#endif
#include "util_io.h"

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
#define gnutls_session SSL *
#endif
#endif

typedef void (jump_conn_f)(Connection *conn);

struct Connection_s
{
    Server   *serv;           /* parent session               */
    UWORD     type;           /* connection type - TYPE_*                 */
    UBYTE     version;        /* protocol version in this session         */

    Contact  *cont;           /* the user this connection is for          */
    char     *server;         /* the remote server name                   */
    UDWORD    port;           /* the port the server is listening on      */
    UDWORD    ip;             /* the remote ip (host byte order)          */
    UDWORD    our_local_ip;   /* LAN-internal IP (host byte order)        */
    UDWORD    our_outside_ip; /* the IP address the server sees from us   */
    SOK_T     sok;            /* socket for connection to server          */
    UWORD     connect;        /* connection setup status                  */
    Packet   *incoming;       /* packet we're receiving                   */
    Packet   *outgoing;       /* packet we're sending                     */

    Conn_Func  *funcs;        /* functions to call  for i/o               */
    Dispatcher *dispatcher;   /* pointer to extra data for dispatching    */
    
#if ENABLE_SSL
    gnutls_session ssl;       /* The SSL data structure                   */
    ssl_status_t ssl_status;  /* SSL status (INIT,OK,FAILED,...)          */
#endif

    Connection       *oscar_file;  /* associated file saving session           */
    UDWORD    oscar_our_session;    /* session ID                               */
    UDWORD    oscar_file_len;            /* used for file transfer                   */
    UDWORD    oscar_file_done;           /* used for file transfer                   */
    UWORD     oscar_dc_seq;

    void *xmpp_file_private;   /* XMPP file transfer private */
#define xmpp_file oscar_file
#define xmpp_file_len    oscar_file_len            /* used for file transfer                   */
#define xmpp_file_done   oscar_file_done           /* used for file transfer                   */

    jump_conn_f *dispatch;     /* function to call on select()    */
    jump_conn_f *reconnect;    /* function to call for reconnect  */
    jump_conn_f *close;        /* function to call to close       */
    jump_conn_f *utilio;       /* private to util_io.c            */

    UDWORD    stat_pak_sent;
    UDWORD    stat_pak_rcvd;
};

struct Server_s
{
    UWORD     type;           /* connection type - TYPE_*                 */
    UBYTE     pref_version;        /* protocol version in this session         */
    Connection            *oscar_dc;  /* associated session           */

    char     *screen;
    status_t  status;         /* own status                               */
    UBYTE     flags;          /* connection flags                         */
    status_t  pref_status;
    char     *pref_server;
    UDWORD    pref_port;
    char     *pref_passwd;
    char     *passwd;         /* the password for this user               */
    SOK_T     logfd;
    ContactGroup *contacts;   /* The contacts for this connection         */
    Connection            *conn;   /* main I/O connection          */
    UDWORD    nativestatus;   /* own ICQ extended status                  */
    idleflag_t idle_flag;     /* the idle status                          */
    Opt       copts;

#if ENABLE_XMPP
    void     *xmpp_private;   /* private data for XMPP connections        */
    UWORD     xmpp_sequence;
#endif
#if ENABLE_IKS
    iksparser*xmpp_parser;
    char     *xmpp_stamp;
    iksid    *xmpp_id;
    iksfilter*xmpp_filter;
    char     *xmpp_gmail_new_newertid;
    char     *xmpp_gmail_newertid;
    char     *xmpp_gmail_query;
    int64_t   xmpp_gmail_new_newer;
    int64_t   xmpp_gmail_newer;
#endif

    void     *oscar_tlv;            /* temporary during v8 connect              */
    UDWORD    oscar_uin;            /* the uin of this server connection        */
    UWORD     oscar_type2_seq;     /* sequence number for dc and type-2        */
    UWORD     oscar_snac_seq;       /* current secondary sequence number        */
#define xmpp_file_seq oscar_snac_seq
    UDWORD    oscar_icq_seq;       /* current old-ICQ sequence number          */
    UWORD     oscar_seq;        /* current primary sequence number          */
    UWORD     oscar_privacy_tag;         /* F*cking ICQ needs to change the value */
    UBYTE     oscar_privacy_value;       /* when switching between visible and invisible */
};

#define CONNECT_MASK       0x00ff
#define CONNECT_OK         0x0080
#define CONNECT_FAIL       0x0100
#define CONNECT_SELECT_A   0x1000
#define CONNECT_SELECT_R   0x2000
#define CONNECT_SELECT_W   0x4000
#define CONNECT_SELECT_X   0x8000
#define CONNECT_SOCKS_ADD  0x0100
#define CONNECT_SOCKS      0x0f00


Server        *ServerC           (UWORD type DEBUGPARAM);
void           ServerD           (Server *serv DEBUGPARAM);
Server        *ServerNr          (int i);
UDWORD         ServerFindNr      (const Server *serv);
Server        *ServerFindScreen  (UWORD type, const char *screen);
Connection    *ServerChild       (Server *serv, Contact *cont, UWORD type DEBUGPARAM);
Connection    *ServerFindChild   (const Server *parent, const Contact *cont, UWORD type);
const char    *ServerStrType     (Server *conn);

Connection    *ConnectionC       (UWORD type DEBUGPARAM);
void           ConnectionD       (Connection *conn DEBUGPARAM);
Connection    *ConnectionNr      (int i);
const char    *ConnectionStrType (Connection *conn);
const char    *ConnectionServerType  (UWORD type);
UWORD          ConnectionServerNType (const char *type, char del);
val_t          ConnectionPrefVal (Server *conn, UDWORD flag);
const char    *ConnectionPrefStr (Server *conn, UDWORD flag);

#define ConnectionC(t)       ConnectionC (t DEBUGARGS)
#define ConnectionD(c)       ConnectionD (c DEBUGARGS)
#define ServerC(t)           ServerC (t DEBUGARGS)
#define ServerD(c)           ServerD (c DEBUGARGS)
#define ServerChild(s,f,t)   ServerChild (s,f,t DEBUGARGS)

/*
                            TYPE_SERVER
                           ^ |  ^ ^ ^ ^
                         p/ dc  | | | p\
                         /   V  | | |   \
                 TYPE_MSGLISTEN | | | TYPE_FILELISTEN
                                | | |
                               p| | |p
                                | | |
              TYPE_MSGDIRECT(uin) | TYPE_FILEDIRECT(uin)
                                  |   |
                                  |   |file
                                  |   V
                                 TYPE_FILE(uin)

*/


#define TYPEF_ANY_SERVER    1  /* any server connection  */
#define TYPEF_OSCAR         4  /* ICQ server connection  */
#define TYPEF_ANY_PEER      8  /* any peer connection    */
#define TYPEF_ANY_DIRECT   16  /* " && established       */
#define TYPEF_ANY_LISTEN   32  /* " && listening         */
#define TYPEF_ANY_MSG      64  /* " && for messages      */
#define TYPEF_ANY_FILE    128  /* " && for file transfer */
#define TYPEF_ANY_CHAT    256  /* " && for chat          */
#define TYPEF_FILE        512  /* any file io            */
#define TYPEF_REMOTE     1024  /* remote control (socket)*/
#define TYPEF_MSN        2048  /* MSN: connection        */
#define TYPEF_MSN_CHAT   4096  /* MSN: chat connection   */
#define TYPEF_XMPP       8192  /* XMPP(Jabber) connection*/
#define TYPEF_HAVEUIN   16384  /* server session uses numeric UINs */

/* any conn->type may be only any of those values:
 * do not use the flags above unless you _really_ REALLY know what you're doing
 */
#define TYPE_SERVER        (TYPEF_ANY_SERVER | TYPEF_OSCAR | TYPEF_HAVEUIN)
#define TYPE_XMPP_SERVER   (TYPEF_ANY_SERVER | TYPEF_XMPP)
#define TYPE_MSN_SERVER    (TYPEF_ANY_SERVER | TYPEF_MSN)

#define TYPE_MSGLISTEN     (TYPEF_ANY_PEER | TYPEF_ANY_MSG  | TYPEF_ANY_LISTEN)
#define TYPE_MSGDIRECT     (TYPEF_ANY_PEER | TYPEF_ANY_MSG  | TYPEF_ANY_DIRECT)
#define TYPE_FILELISTEN    (TYPEF_ANY_PEER | TYPEF_ANY_FILE | TYPEF_ANY_LISTEN)
#define TYPE_FILEDIRECT    (TYPEF_ANY_PEER | TYPEF_ANY_FILE | TYPEF_ANY_DIRECT)
#define TYPE_CHATLISTEN    (TYPEF_ANY_PEER | TYPEF_ANY_CHAT | TYPEF_ANY_LISTEN)
#define TYPE_CHATDIRECT    (TYPEF_ANY_PEER | TYPEF_ANY_CHAT | TYPEF_ANY_DIRECT)
#define TYPE_XMPPDIRECT    (TYPEF_ANY_PEER | TYPEF_ANY_FILE | TYPEF_XMPP)
#define TYPE_FILE          (TYPEF_FILE | TYPEF_OSCAR)
#define TYPE_FILEXMPP      (TYPEF_FILE | TYPEF_XMPP)
#define TYPE_REMOTE        TYPEF_REMOTE
#define TYPE_MSN_TEMP      (TYPEF_ANY_PEER | TYPEF_MSN)
#define TYPE_MSN_CHAT      (TYPEF_MSN | TYPEF_MSN_CHAT)

#define ASSERT_SERVER(s)      { Server *__s = (s); assert (__s); assert (__s->type == TYPE_SERVER); }
#define ASSERT_SERVER_CONN(c) { Connection *__c = (c); assert (__c); ASSERT_SERVER (__c->serv); }

#define CONN_AUTOLOGIN   1
#define CONN_WIZARD      2
#define CONN_INITWP      8

#endif /* CLIMM_UTIL_CONNECTION_H */
