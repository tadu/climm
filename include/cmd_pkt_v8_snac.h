/* $Id$ */

#include "cmd_pkt_cmd_v5.h"

#ifndef MICQ_ICQV8_SNAC_H
#define MICQ_ICQV8_SNAC_H

void SnacCallback (Event *event);
const char *SnacName (UWORD fam, UWORD cmd);
void SnacPrint (Packet *pak);

void   SnacCliFamilies     (Connection *conn);
void   SnacCliRatesrequest (Connection *conn);
void   SnacCliReqinfo      (Connection *conn);
void   SnacCliReqlocation  (Connection *conn);
void   SnacCliReqbuddy     (Connection *conn);
void   SnacCliReqicbm      (Connection *conn);
void   SnacCliReqbos       (Connection *conn);
void   SnacCliReqlists     (Connection *conn);
void   SnacCliSetuserinfo  (Connection *conn);
void   SnacCliSetstatus    (Connection *conn, UDWORD status, UWORD action);
void   SnacCliReady        (Connection *conn);
void   SnacCliSeticbm      (Connection *conn);
void   SnacCliAddcontact   (Connection *conn, Contact *cont);
void   SnacCliRemcontact   (Connection *conn, Contact *cont);
void   SnacCliReqroster    (Connection *conn);
void   SnacCliGrantauth    (Connection *conn, Contact *cont);
void   SnacCliReqauth      (Connection *conn, Contact *cont, const char *msg);
void   SnacCliAuthorize    (Connection *conn, Contact *cont, BOOL accept, const char *msg);
void   SnacCliAddvisible   (Connection *conn, Contact *cont);
void   SnacCliRemvisible   (Connection *conn, Contact *cont);
void   SnacCliAddinvis     (Connection *conn, Contact *cont);
void   SnacCliReminvis     (Connection *conn, Contact *cont);
UBYTE  SnacCliSendmsg      (Connection *conn, Contact *cont, const char *text, UDWORD type, UBYTE format);
void   SnacCliReqofflinemsgs  (Connection *conn);
void   SnacCliAckofflinemsgs  (Connection *conn);
void   SnacCliRegisteruser    (Connection *conn);
UDWORD SnacCliMetareqinfo     (Connection *conn, Contact *cont);
UDWORD SnacCliMetareqmoreinfo (Connection *conn, Contact *cont);
void   SnacCliMetasetabout    (Connection *conn, const char *text);
void   SnacCliMetasetmore     (Connection *conn, Contact *cont);
void   SnacCliMetasetgeneral  (Connection *conn, Contact *cont);
void   SnacCliMetasetpass     (Connection *conn, const char *newpass);
void   SnacCliSendsms         (Connection *conn, const char *target, const char *text);
void   SnacCliSearchbypersinf (Connection *conn, const char *email, const char *nick, const char *name, const char *surname);
void   SnacCliSearchbymail    (Connection *conn, const char *email);
void   SnacCliSearchwp        (Connection *conn, const MetaWP *wp);
UDWORD SnacCliSearchrandom    (Connection *conn, UWORD group);
void   SnacCliSetrandom       (Connection *conn, UWORD group);

UBYTE  SnacCliSendmsg2     (Connection *conn, Contact *cont, Extra *extra);

#endif /* MICQ_ICQV8_SNAC_H */
