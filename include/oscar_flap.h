/* $Id$ */

#ifndef MICQ_ICQV8_FLAP_H
#define MICQ_ICQV8_FLAP_H

#define CLI_HELLO 1

#define FLAP_VER_MAJOR       5
#define FLAP_VER_MINOR      37
#define FLAP_VER_LESSER      1
#define FLAP_VER_BUILD    3828
#define FLAP_VER_SUBBUILD   85

void FlapCliHello (Connection *conn);
void FlapCliIdent (Connection *conn);
void FlapCliCookie (Connection *conn, const char *cookie, UWORD len);
void FlapCliGoodbye (Connection *conn);
void FlapCliKeepalive (Connection *conn);

void SrvCallBackFlap (Event *event);

Packet *FlapC (UBYTE channel);
void    FlapSend (Connection *conn, Packet *pak);
void    FlapPrint (Packet *pak);
void    FlapSave (Packet *pak, BOOL in);

#endif /* MICQ_ICQV8_FLAP_H */
