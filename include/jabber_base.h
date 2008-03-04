
#ifndef CLIMM_XMPP_BASE_H
#define CLIMM_XMPP_BASE_H

#include "im_request.h"

Event *ConnectionInitXMPPServer (Server *serv);

UBYTE XMPPSendmsg   (Server *serv, Contact *cont, Message *msg);
void  XMPPSetstatus (Server *serv, Contact *cont, status_t status, const char *msg);
void  XMPPAuthorize (Server *serv, Contact *cont, auth_t how, const char *msg);

#endif
