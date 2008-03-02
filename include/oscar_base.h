/* $Id$ */

#ifndef CLIMM_ICQV8_FLAP_H
#define CLIMM_ICQV8_FLAP_H

#define CLI_HELLO 1

#define FLAP_VER_MAJOR       5
#define FLAP_VER_MINOR      37
#define FLAP_VER_LESSER      1
#define FLAP_VER_BUILD    3828
#define FLAP_VER_SUBBUILD   85

void FlapCliHello (Connection *serv);
void FlapCliIdent (Connection *serv);
void FlapCliCookie (Connection *serv, const char *cookie, UWORD len);
void FlapCliGoodbye (Connection *serv);
void FlapCliKeepalive (Connection *serv);
void FlapChannel4 (Connection *conn, Packet *pak);

void SrvCallBackFlap (Event *event);

Packet *FlapC (UBYTE channel);
void    FlapSend (Connection *serv, Packet *pak);
void    FlapPrint (Packet *pak);

Event *ConnectionInitServer (Connection *serv);
Connection *SrvRegisterUIN (Connection *serv, const char *pass);

status_t     IcqToStatus   (UDWORD status);
UDWORD       IcqFromStatus (status_t status);
statusflag_t IcqToFlags    (UDWORD status);
UDWORD       IcqFromFlags  (statusflag_t flags);
UDWORD       IcqIsUIN      (const char *screen);

#endif
