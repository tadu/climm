
#ifndef CLIMM_IO_GNUTLS_H
#define CLIMM_IO_GNUTLS_H 1


#include "connection.h"

enum io_gnutls_supported {
    IO_GNUTLS_OK = 0,
    IO_GNUTLS_NOMEM,
    IO_GNUTLS_NOLIB,
    IO_GNUTLS_INIT
};

#if ENABLE_GNUTLS
char        IOGnuTLSSupported (void);
int         IOGnuTLSOpen (Connection *conn, char is_client);
const char *IOGnuTLSInitError (void);
#else
#define IOGnuTLSSupported() IO_GNUTLS_NOLIB
#define IOGnuTLSOpen(c,i)
#define IOGnuTLSLastError(c) ""
#endif

#endif /* CLIMM_IO_TCP_H */
