/* $Id: util_io.h 2727 2009-03-10 23:07:39Z kuhlmann $ */

#ifndef CLIMM_IO_PRIVATE_H
#define CLIMM_IO_PRIVATE_H

#if ENABLE_GNUTLS
#include <gnutls/gnutls.h>
#endif
#if ENABLE_OPENSSL
#include <openssl/ssl.h>
#endif

#include "io/io_gnutls.h"
#include "io/io_openssl.h"

io_err_t io_any_appendbuf (Connection *conn, Dispatcher *d, const char *buf, size_t count);
io_err_t io_util_accept (Connection *conn, Dispatcher *d, Connection *newc);
io_err_t io_util_read (Connection *conn, Dispatcher *d, char *buf, size_t count);
io_err_t io_util_write (Connection *conn, Dispatcher *d, const char *buf, size_t count);

typedef struct Conn_Func_s
{
    int         (* const f_accept)(Connection *c, Dispatcher *d, Connection *newc);
    io_err_t    (* const f_cting) (Connection *c, Dispatcher *d);
    int         (* const f_read)  (Connection *c, Dispatcher *d, char *buf, size_t count);
    io_err_t    (* const f_write) (Connection *c, Dispatcher *d, const char *buf, size_t count);
    void        (* const f_close) (Connection *c, Dispatcher *d);
    const char *(* const f_err)   (Connection *c, Dispatcher *d);
} Conn_Func;

enum io_dispatcher_flags {
    FLAG_CLOSED,
    FLAG_OPEN,
    FLAG_CONNECTED,
    FLAG_CONNECTING,
    FLAG_TIMEOUT,
    
    FLAG_METHODS_SENT, /* socks5 */
    FLAG_SEND_CRED,
    FLAG_CRED_SENT,
    FLAG_SEND_REQ,
    FLAG_REQ_SENT,
    FLAG_SEND_REQ_NOPORT,
    FLAG_REQ_NOPORT_SENT
};

struct Dispatcher_s
{
    Conn_Func *funcs;
    Connection *conn; /* io_gnutls + io_openssl */
    Dispatcher *next; /* io_gnutls + io_openssl + io_socks5*/
    size_t outlen;
    char  *outbuf;
    enum io_dispatcher_flags flags;
/* io_tcp + io_gnutls + io_openssl */
    int    d_errno;
    char  *lasterr;
/* io_tcp */
    io_err_t err;
#if ENABLE_SSL
/* io_gnutls */
#if ENABLE_GNUTLS
    io_ssl_err_t gnutls_err;
    gnutls_certificate_credentials cred;
    gnutls_session ssl;       /* The SSL data structure                   */
#endif
/* io_openssl */
#if ENABLE_OPENSSL
    io_ssl_err_t openssl_err;
    SSL *openssl;
#endif
/*    ssl_status_t ssl_status;  / * SSL status (INIT,OK,FAILED,...)          */
#endif
/* io_socks5 */
    char buf[10];
    char read;
};

#endif
