/* $Id$ */

#ifndef MICQ_OSCAR_H
#define MICQ_OSCAR_H

Event *ConnectionInitServer (Connection *conn);
Connection *SrvRegisterUIN (Connection *conn, const char *pass);

void SrvMsgAdvanced (Packet *pak, UDWORD seq, UWORD type,
                     UWORD status, UDWORD deststatus, UWORD flags, const char *msg);
void SrvMsgGreet (Packet *pak, UWORD cmd, const char *reason, UWORD port, UDWORD len, const char *msg);
void SrvReceiveAdvanced (Connection *serv, Event *inc_event, Packet *inc_pak, Event *ack_event);

#endif
