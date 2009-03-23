
#ifndef CLIMM_IO_GNUTLS_H
#define CLIMM_IO_GNUTLS_H 1

#if ENABLE_GNUTLS
io_ssl_err_t IOGnuTLSSupported (void);
io_ssl_err_t IOGnuTLSOpen (Connection *conn, char is_client);
const char  *IOGnuTLSInitError (void);
#else
#define IOGnuTLSSupported() IO_SSL_NOLIB
#define IOGnuTLSOpen(c,i)
#define IOGnuTLSInitError(c) ""
#endif

#endif /* CLIMM_IO_TCP_H */
