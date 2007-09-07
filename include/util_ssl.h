/* $Id$ */

typedef struct ssl_md5ctx_s ssl_md5ctx_t;

#ifdef ENABLE_SSL
#ifndef CLIMM_UTIL_SSL_H
#define CLIMM_UTIL_SSL_H

/* Number of bits climm uses for Diffie-Hellman key exchange of
 * incoming direct connections. You may change this value.
 */
#define DH_OFFER_BITS       768
/* Number of bits climm enforces as a minimum for DH of outgoing 
 * direct connections. 
 * This value must be less than or equal to 512 in order to work with licq.
 */
#define DH_EXPECT_BITS      512

#define ssl_errno_t int

int SSLInit ();
ssl_errno_t ssl_write (Connection *conn, UBYTE *data, UWORD len);
ssl_errno_t ssl_read (Connection *conn, UBYTE *data, UWORD len);
void ssl_close (Connection *conn DEBUGPARAM);
void ssl_disconnect (Connection *conn DEBUGPARAM);
int ssl_supported (Connection *conn DEBUGPARAM);
int ssl_connect (Connection *conn, BOOL is_client DEBUGPARAM);
int ssl_handshake (Connection *conn DEBUGPARAM);
const char *ssl_strerror (Connection *conn, ssl_errno_t se, int e);
BOOL TCPSendSSLReq (Connection *list, Contact *cont);

ssl_md5ctx_t *ssl_md5_init  (void);
void          ssl_md5_write (ssl_md5ctx_t *ctx, char *buf, size_t len);
int           ssl_md5_final (ssl_md5ctx_t *ctx, char *buf);

#define ssl_close(c)      ssl_close(c DEBUGARGS)
#define ssl_disconnect(c) ssl_disconnect(c DEBUGARGS)
#define ssl_supported(c)  ssl_supported(c DEBUGARGS)
#define ssl_connect(c,i)  ssl_connect(c,i DEBUGARGS)
#define ssl_handshake(c)  ssl_handshake(c DEBUGARGS)

#define LICQ_WITHSSL        0x7D800000  /* taken from licq 1.2.7 */

#define dc_write    ssl_write
#define dc_read     ssl_read
#define dc_close    ssl_close
#define dc_strerror ssl_strerror

#endif /* CLIMM_UTIL_SSL_H */
#else
#define ssl_md5_init()       NULL
#define ssl_md5_write(c,b,l) { }
#define ssl_md5_final(c,b)   -1
#define ssl_errno_t int
#define dc_write(c,d,l)    sockwrite((c)->sok,d,l)
#define dc_read(c,d,l)     sockread((c)->sok,d,l)
#define dc_close(c)        sockclose((c)->sok)
#define dc_strerror(c,s,e) strerror(e)
#endif
