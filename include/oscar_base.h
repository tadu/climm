/* $Id$ */

#ifndef CLIMM_ICQV8_FLAP_H
#define CLIMM_ICQV8_FLAP_H

#define CLI_HELLO 1

#define FLAP_VER_MAJOR       5
#define FLAP_VER_MINOR      37
#define FLAP_VER_LESSER      1
#define FLAP_VER_BUILD    3828
#define FLAP_VER_SUBBUILD   85

void FlapCliHello (Server *serv);
void FlapCliIdent (Server *serv);
void FlapCliCookie (Server *serv, const char *cookie, UWORD len);
void FlapCliGoodbye (Server *serv);
void FlapCliKeepalive (Server *serv);
void FlapChannel4 (Server *serv, Packet *pak);

void SrvCallBackFlap (Event *event);

Packet *FlapC (UBYTE channel);
void    FlapSend (Server *serv, Packet *pak);
void    FlapPrint (Packet *pak);

Event *ConnectionInitServer (Server *serv);
Server *SrvRegisterUIN (Server *serv, const char *pass);

status_t     IcqToStatus   (UDWORD status);
UDWORD       IcqFromStatus (status_t status);
statusflag_t IcqToFlags    (UDWORD status);
UDWORD       IcqFromFlags  (statusflag_t flags);
UDWORD       IcqIsUIN      (const char *screen);

#endif
