/* $Id$ */

#ifndef MICQ_ICQV8_H
#define MICQ_ICQV8_H

void ConnectionInitServer (Connection *conn);
Connection *SrvRegisterUIN (Connection *conn, const char *pass);

void SrvMsgAdvanced (Packet *pak, UDWORD seq, UWORD type,
                     UWORD flags, UWORD status, const char *msg);
void SrvMsgGreet (Packet *pak, UWORD cmd, const char *reason, UWORD port, UDWORD len, const char *msg);
void SrvReceiveAdvanced (Connection *serv, Event *inc_event, Event *ack_event);

#endif /* MICQ_ICQV8_H */
