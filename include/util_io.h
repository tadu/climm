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
char   *UtilIOReadline (FILE *fd);

#endif /* MICQ_UTIL_IO_H */
