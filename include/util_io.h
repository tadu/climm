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

typedef enum io_err_e {
    IO_NO_PARAM = -12,
    IO_NO_MEM,
    IO_NO_SOCKET,
    IO_NO_NONBLOCK,

    IO_NO_BIND,
    IO_NO_LISTEN,

    IO_NO_HOSTNAME,
    IO_NO_CONN,
    IO_CONN_TO,
    IO_CONNECTED,

    IO_CLOSED,
    IO_RW,
    
    IO_OK = 0
} io_err_t;

typedef struct Conn_Func_s
{
    int      (*f_accept)(Connection *c, Dispatcher *d, Connection *newc);
    int      (*f_read)  (Connection *c, Dispatcher *d, char *buf, size_t count);
    io_err_t (*f_write) (Connection *c, Dispatcher *d, const char *buf, size_t count);
    void     (*f_close) (Connection *c, Dispatcher *d);
    char    *(*f_err)   (Connection *c, Dispatcher *d);
} Conn_Func;

#include "io/io_gnutls.h"

struct Dispatcher_s
{
/* io_tcp + io_gnutls */
    int    d_errno;
    char  *lasterr;
/* io_tcp */
    io_err_t err;
    UWORD  flags;
    size_t outlen;
    char  *outbuf;
/* io_gnutls */
#if ENABLE_SSL
    io_gnutls_err_t gnutls_err;
    Connection *conn;
    Dispatcher *next;
    Conn_Func *next_funcs;
    gnutls_session ssl;       /* The SSL data structure                   */
/*    ssl_status_t ssl_status;  / * SSL status (INIT,OK,FAILED,...)          */
#endif
};

void    UtilIOConnectF   (Connection *conn);
int     UtilIOError      (Connection *conn);
Packet *UtilIOReceiveF   (Connection *conn);
strc_t  UtilIOReadline   (FILE *fd);

void     UtilIOShowDisconnect (Connection *conn, io_err_t rc);
io_err_t UtilIOShowError (Connection *conn, io_err_t rc);

void    UtilIOSelectInit (int sec, int usec);
void    UtilIOSelectAdd  (FD_T sok, int nr);
BOOL    UtilIOSelectIs   (FD_T sok, int nr);
void    UtilIOSelect     (void);

#define READFDS   1
#define WRITEFDS  2
#define EXCEPTFDS 4

#endif /* CLIMM_UTIL_IO_H */
