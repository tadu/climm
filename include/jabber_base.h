
#ifndef MICQ_JABBER_BASE_H
#define MICQ_JABBER_BASE_H

Event *ConnectionInitJabberServer (Connection *serv);

UBYTE JabberSendmsg (Connection *serv, Contact *cont, const char *text, UDWORD type);

#endif
