
#ifndef CLIMM_IO_OPENSSL_H
#define CLIMM_IO_OPENSSL_H 1

#if ENABLE_OPENSSL
io_ssl_err_t IOOpenSSLSupported (void);
io_ssl_err_t IOOpenSSLOpen (Connection *conn, char is_client);
const char  *IOOpenSSLInitError (void);
#else
#define IOOpenSSLSupported() IO_SSL_NOLIB
#define IOOpenSSLOpen(c,i)   IO_SSL_NOLIB
#define IOOpenSSLInitError(c) ""
#endif

#endif /* CLIMM_IO_TCP_H */
