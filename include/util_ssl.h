/* $Id$ */

#ifdef ENABLE_SSL
#ifndef MICQ_UTIL_SSL_H
#define MICQ_UTIL_SSL_H

/* Number of bits mICQ uses for Diffie-Hellman key exchange of
 * incoming direct connections. You may change this value.
 */
#define DH_OFFER_BITS       768
/* Number of bits mICQ enforces as a minimum for DH of outgoing 
 * direct connections. 
 * This value must be less than or equal to 512 in order to work with licq.
 */
#define DH_EXPECT_BITS      512

int SSLInit ();
int ssl_sockwrite (Connection *c, UBYTE *data, UWORD len);
int ssl_sockread (Connection *c, UBYTE *data, UWORD len);
void ssl_sockclose (Connection *c);
void ssl_disconnect (Connection *conn);
int ssl_supported (Connection *c);
int ssl_connect (Connection *c, BOOL is_client);
int ssl_handshake (Connection *conn);
const char *ssl_strerror (int error);
BOOL TCPSendSSLReq (Connection *list, Contact *cont);

#define LICQ_WITHSSL        0x7D800000  /* taken from licq 1.2.7 */

#endif /* MICQ_UTIL_SSL_H */
#endif /* ENABLE_SSL */
