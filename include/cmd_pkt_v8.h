/* $Id$ */

#ifndef MICQ_ICQV8_H
#define MICQ_ICQV8_H

void ConnectionInitServer (Connection *conn);
Connection *SrvRegisterUIN (Connection *conn, const char *pass);
void SrvMsgAdvanced (Packet *pak, UDWORD seq, UWORD type,
                     UWORD flags, UWORD status, const char *msg);
void SrvMsgGreet (Packet *pak, UWORD cmd, const char *reason, UWORD port, UDWORD len, const char *msg);
UBYTE SrvReceiveAdvanced (Connection *serv, Contact *cont, Packet *pak, Packet *ack, Extra *extra);

#endif /* MICQ_ICQV8_H */
