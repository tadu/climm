/* $Id$ */

size_t SOCKREAD (Session *sess, void *ptr, size_t len);
SOK_T       UtilIOConnectUDP (char *hostname, int port);
void        UtilIOConnectTCP (Session *sess);
int         UtilIOError      (Session *sess);
void        UtilIOSocksAccept(Session *sess);
Packet     *UtilIOReceiveUDP (Session *sess);
Packet     *UtilIOReceiveTCP (Session *sess);
BOOL        UtilIOSendTCP    (Session *sess, Packet *pak);
void        UtilIOSend       (Session *sess, Packet *pak);

void   M_fdprint (FD_T fd, const char *str, ...);
int    M_fdnreadln (FILE *fd, char *buf, size_t len);
