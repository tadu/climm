/* $Id$ */

#ifndef MICQ_ICQV8_H
#define MICQ_ICQV8_H

void ConnectionInitServer (Connection *conn);
Connection *SrvRegisterUIN (Connection *conn, const char *pass);
void SrvMsgAdvanced (Packet *pak, UDWORD seq, UWORD type,
                     UWORD flags, UWORD status, const char *msg);


#endif /* MICQ_ICQV8_H */
