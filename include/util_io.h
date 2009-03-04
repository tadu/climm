/* $Id$ */

#ifndef CLIMM_UTIL_IO_H
#define CLIMM_UTIL_IO_H

void    UtilIOConnectTCP (Connection *conn DEBUGPARAM);
void    UtilIOConnectF   (Connection *conn);
int     UtilIOError      (Connection *conn);
void    UtilIOSocksAccept(Connection *conn);
Packet *UtilIOReceiveTCP (Connection *conn);
Packet *UtilIOReceiveF   (Connection *conn);
void    UtilIOSendTCP    (Connection *conn, Packet *pak);
strc_t  UtilIOReadline   (FILE *fd);

void    UtilIOSelectInit (int sec, int usec);
void    UtilIOSelectAdd  (FD_T sok, int nr);
BOOL    UtilIOSelectIs   (FD_T sok, int nr);
void    UtilIOSelect     (void);

#define READFDS   1
#define WRITEFDS  2
#define EXCEPTFDS 4

#define UtilIOConnectTCP(c) UtilIOConnectTCP(c DEBUGARGS)

#endif /* CLIMM_UTIL_IO_H */
