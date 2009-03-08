/* $Id$ */

#ifndef CLIMM_UTIL_IO_H
#define CLIMM_UTIL_IO_H

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

typedef struct Dispatcher_s Dispatcher;
                            
typedef struct Conn_Func_s
{
    int   (*f_read) (Connection *c, Dispatcher *d, char *buf, size_t count);
    int   (*f_write)(Connection *c, Dispatcher *d, const char *buf, size_t count);
    void  (*f_close)(Connection *c, Dispatcher *d);
    char *(*f_err)  (Connection *c, Dispatcher *d);
} Conn_Func;

struct Dispatcher_s
{
/* io_tcp + io_gnutls */
    size_t err;
    int    d_errno;
    char  *lasterr;
/* io_tcp */
    UWORD  flags;
    size_t outlen;
    char  *outbuf;
/* io_gnutls */
#if ENABLE_SSL
    Connection *conn;
    Dispatcher *next;
    Conn_Func *next_funcs;
    gnutls_session ssl;       /* The SSL data structure                   */
/*    ssl_status_t ssl_status;  / * SSL status (INIT,OK,FAILED,...)          */
#endif
};

enum io_err {
    IO_OK = 0,

    IO_NO_PARAM = -1000,
    IO_NO_SOCKET,
    IO_NO_NONBLOCK,
    IO_NO_HOSTNAME,
    IO_NO_MEM,
    IO_NO_CONN,
    IO_CONN_TO,
    IO_CONNECTED,
    IO_CLOSED,
    IO_RW,
    
    IO_MAX
};

void    UtilIOConnectTCP (Connection *conn DEBUGPARAM);
void    UtilIOConnectF   (Connection *conn);
int     UtilIOError      (Connection *conn);
void    UtilIOSocksAccept(Connection *conn);
Packet *UtilIOReceiveTCP (Connection *conn);
Packet *UtilIOReceiveF   (Connection *conn);
void    UtilIOSendTCP    (Connection *conn, Packet *pak);
strc_t  UtilIOReadline   (FILE *fd);

void    UtilIOShowDisconnect (Connection *conn, int rc);
int     UtilIOFinishConnect (Connection *conn);

void    UtilIOSelectInit (int sec, int usec);
void    UtilIOSelectAdd  (FD_T sok, int nr);
BOOL    UtilIOSelectIs   (FD_T sok, int nr);
void    UtilIOSelect     (void);

#define READFDS   1
#define WRITEFDS  2
#define EXCEPTFDS 4

#define UtilIOConnectTCP(c) UtilIOConnectTCP(c DEBUGARGS)

#endif /* CLIMM_UTIL_IO_H */
