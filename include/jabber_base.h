
#ifndef MICQ_XMPP_BASE_H
#define MICQ_XMPP_BASE_H

Event *ConnectionInitXMPPServer (Connection *serv);

UBYTE XMPPSendmsg (Connection *serv, Contact *cont, const char *text, UDWORD type);
void XMPPSetstatus (Connection *serv, Contact *cont, status_t status, const char *msg);

#endif
