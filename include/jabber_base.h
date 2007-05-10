
#ifndef MICQ_XMPP_BASE_H
#define MICQ_XMPP_BASE_H

#include "im_request.h"

Event *ConnectionInitXMPPServer (Connection *serv);

UBYTE XMPPSendmsg   (Connection *serv, Contact *cont, UDWORD type, const char *text);
void  XMPPSetstatus (Connection *serv, Contact *cont, status_t status, const char *msg);
void  XMPPAuthorize (Connection *serv, Contact *cont, auth_t how, const char *msg);

#endif
