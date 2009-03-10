/* $Id$ */

#ifndef CLIMM_UTIL_SSL_H
#define CLIMM_UTIL_SSL_H

#ifdef ENABLE_SSL
/* Number of bits climm uses for Diffie-Hellman key exchange of
 * incoming direct connections. You may change this value.
 */
#define DH_OFFER_BITS       768
/* Number of bits climm enforces as a minimum for DH of outgoing 
 * direct connections. 
 * This value must be less than or equal to 512 in order to work with licq.
 */
#define DH_EXPECT_BITS      512

int ssl_supported (Connection *conn DEBUGPARAM);

BOOL TCPSendSSLReq (Connection *list, Contact *cont);

#define ssl_supported(c)  ssl_supported(c DEBUGARGS)

#define LICQ_WITHSSL        0x7D800000  /* taken from licq 1.2.7 */
#endif

#define ssl_errno_t int
#endif
