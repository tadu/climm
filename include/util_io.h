
size_t SOCKREAD (Session *sess, void *ptr, size_t len);
SOK_T UtilIOConnectUDP (char *hostname, int port, FD_T aux);
void  UtilIOSend       (Session *sess, Packet *pak);
Packet *UtilIORecvUDP  (Session *sess);
