/* $Id$ */

#ifndef CLIMM_UTIL_IO_H
#define CLIMM_UTIL_IO_H

typedef enum {
    SSL_STATUS_NA,       /* unknown / nothing done yet       */
    SSL_STATUS_FAILED,   /* SSL handshake with peer failed   */
    SSL_STATUS_OK,       /* SSL up and running               */
    SSL_STATUS_INIT,     /* SSL handshake may start          */
    SSL_STATUS_CLOSE,    /* SSL session to be terminated     */
    SSL_STATUS_REQUEST,  /* SSL session has been requested   */
    SSL_STATUS_HANDSHAKE /* SSL session handshake is ongoing */
} ssl_status_t;

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

void        UtilIOConnectTCP (Connection *conn);
void        UtilIOListenTCP  (Connection *conn);

int         UtilIOAccept (Connection *conn, Connection *newc);
int         UtilIORead   (Connection *conn, char *buf, size_t count);
io_err_t    UtilIOWrite  (Connection *conn, const char *buf, size_t count);
void        UtilIOClose  (Connection *conn);
const char *UtilIOErr    (Connection *conn);

void    UtilIOConnectF   (Connection *conn);
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
