
#ifndef CLIMM_IO_GNUTLS_H
#define CLIMM_IO_GNUTLS_H 1

typedef enum io_gnutls_err_e {
    IO_GNUTLS_UNINIT = -1,
    IO_GNUTLS_OK = 0,
    IO_GNUTLS_NOMEM,
    IO_GNUTLS_NOLIB,
    IO_GNUTLS_INIT
} io_gnutls_err_t;

#if ENABLE_GNUTLS
io_gnutls_err_t IOGnuTLSSupported (void);
io_gnutls_err_t IOGnuTLSOpen (Connection *conn, char is_client);
const char     *IOGnuTLSInitError (void);
#else
#define IOGnuTLSSupported() IO_GNUTLS_NOLIB
#define IOGnuTLSOpen(c,i)
#define IOGnuTLSInitError(c) ""
#endif

#endif /* CLIMM_IO_TCP_H */
