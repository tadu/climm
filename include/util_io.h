/* $Id$ */

#ifndef MICQ_UTIL_IO_H
#define MICQ_UTIL_IO_H

void    UtilIOConnectUDP (Connection *conn);
void    UtilIOConnectTCP (Connection *conn);
void    UtilIOConnectF   (Connection *conn);
int     UtilIOError      (Connection *conn);
void    UtilIOSocksAccept(Connection *conn);
Packet *UtilIOReceiveUDP (Connection *conn);
Packet *UtilIOReceiveTCP (Connection *conn);
Packet *UtilIOReceiveF   (Connection *conn);
BOOL    UtilIOSendTCP    (Connection *conn, Packet *pak);
void    UtilIOSendUDP    (Connection *conn, Packet *pak);

void    M_fdprint   (FD_T fd, const char *str, ...) __attribute__ ((format (__printf__, 2, 3)));
int     M_fdnreadln (FILE *fd, char *buf, size_t len);

#endif /* MICQ_UTIL_IO_H */
