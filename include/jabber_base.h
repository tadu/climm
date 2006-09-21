
#ifndef MICQ_JABBER_BASE_H
#define MICQ_JABBER_BASE_H

Event *ConnectionInitJabberServer (Connection *serv);

UBYTE JabberSendmsg (Connection *serv, Contact *cont, const char *text, UDWORD type);
void JabberSetstatus (Connection *serv, Contact *cont, status_t status, const char *msg);

#endif
