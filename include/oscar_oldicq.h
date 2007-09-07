
#ifndef CLIMM_OSCAR_OLDICQ_H
#define CLIMM_OSCAR_OLDICQ_H

#include "oldicq_compat.h"

jump_snac_f SnacSrvToicqerr, SnacSrvFromicqsrv;

void SnacCliReqofflinemsgs (Connection *serv);
void SnacCliAckofflinemsgs (Connection *serv);
void SnacCliMetasetgeneral (Connection *serv, Contact *cont);
void SnacCliMetasetabout (Connection *serv, const char *text);
void SnacCliMetasetmore (Connection *serv, Contact *cont);
void SnacCliMetasetpass (Connection *serv, const char *newpass);
UDWORD SnacCliMetareqmoreinfo (Connection *serv, Contact *cont);
UDWORD SnacCliMetareqinfo (Connection *serv, Contact *cont);
void SnacCliSearchbypersinf (Connection *serv, const char *email, const char *nick, const char *name, const char *surname);
void SnacCliSearchbymail (Connection *serv, const char *email);
UDWORD SnacCliSearchrandom (Connection *serv, UWORD group);
void SnacCliSetrandom (Connection *serv, UWORD group);
void SnacCliSearchwp (Connection *serv, const MetaWP *wp);
void SnacCliSendsms (Connection *serv, const char *target, const char *text);

#endif
