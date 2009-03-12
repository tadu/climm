
#ifndef CLIMM_XMPP_BASE_H
#define CLIMM_XMPP_BASE_H

#include "im_request.h"

Event *XMPPLogin (Server *serv);
void   XMPPLogout (Server *serv);

UBYTE XMPPSendmsg   (Server *serv, Contact *cont, Message *msg);
void  XMPPSetstatus (Server *serv, Contact *cont, status_t status, const char *msg);
void  XMPPAuthorize (Server *serv, Contact *cont, auth_t how, const char *msg);
void  XMPPGoogleMail (Server *serv, time_t since, const char *query);

#endif
