
#ifndef CLIMM_IO_OPENSSL_H
#define CLIMM_IO_OPENSSL_H 1


#include "connection.h"

typedef enum io_openssl_err_e {
    IO_OPENSSL_UNINIT = -1,
    IO_OPENSSL_OK = 0,
    IO_OPENSSL_NOMEM,
    IO_OPENSSL_NOLIB,
    IO_OPENSSL_INIT
} io_openssl_err_t;

#if ENABLE_OPENSSL
io_openssl_err_t IOOpenSSLSupported (void);
io_openssl_err_t IOOpenSSLOpen (Connection *conn, char is_client);
const char      *IOOpenSSLInitError (void);
#else
#define IOOpenSSLSupported() IO_OPENSSL_NOLIB
#define IOOpenSSLOpen(c,i)
#define IOOpenSSLLastError(c) ""
#endif

#endif /* CLIMM_IO_TCP_H */
