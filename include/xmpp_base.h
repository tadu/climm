
#ifndef CLIMM_XMPP_BASE_H
#define CLIMM_XMPP_BASE_H

#include "im_request.h"

typedef enum { p_list = 1, p_active, p_default, p_show, p_set, p_edit, p_list_quiet, p_show_quiet } xmpp_priv_t;

Event *XMPPLogin (Server *serv);
void   XMPPLogout (Server *serv);


UBYTE XMPPSendmsg    (Server *serv, Contact *cont, Message *msg);
void  XMPPSetstatus  (Server *serv, Contact *cont, status_t status, const char *msg);
void  XMPPAuthorize  (Server *serv, Contact *cont, auth_t how, const char *msg);
UBYTE XMPPSendIq     (Server *serv, int8_t which, const char *full, const char *msg);
void  XMPPGoogleMail (Server *serv, time_t since, const char *query);
void  XMPPPrivacy    (Server *serv, xmpp_priv_t type, const char *list, const char *edit);

#endif
