
size_t SOCKREAD (Session *sess, void *ptr, size_t len);
SOK_T UtilIOConnectUDP (char *hostname, int port);
void  UtilIOSend       (Session *sess, Packet *pak);
Packet *UtilIORecvUDP  (Session *sess);

void   M_fdprint (FD_T fd, const char *str, ...);
int    M_fdnreadln (FILE *fd, char *buf, size_t len);
