
#ifndef MICQ_TCP_H
#define MICQ_TCP_H

#include "contact.h"

#define TCP_STATE_WAITING      32

void SessionInitPeer   (Session *sess);

void TCPDirectOpen     (Session *sess, UDWORD uin);
void TCPDirectClose    (               UDWORD uin);
void TCPDirectOff      (               UDWORD uin);
BOOL TCPSendMsg        (Session *sess, UDWORD uin, char *msg, UWORD sub_cmd);

Session *TCPPeer (UDWORD uin);

#endif
