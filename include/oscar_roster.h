
#ifndef MICQ_OSCAR_ROSTER_H
#define MICQ_OSCAR_ROSTER_H

jump_snac_f SnacSrvReplylists, SnacSrvReplyroster, SnacSrvUpdateack,
    SnacSrvRosterok, SnacSrvAuthreq, SnacSrvAuthreply, SnacSrvAddedyou;

void SnacCliCheckroster (Connection *serv);
void SnacCliRosteradd (Connection *serv, ContactGroup *cg, Contact *cont);
void SnacCliRosterupdate (Connection *serv, ContactGroup *cg, Contact *cont);
void SnacCliSetvisibility (Connection *serv);
void SnacCliRosterdelete (Connection *serv, ContactGroup *cg, Contact *cont);
void SnacCliAddstart (Connection *serv);
void SnacCliAddend (Connection *serv);
void SnacCliGrantauth (Connection *serv, Contact *cont);
void SnacCliReqauth (Connection *serv, Contact *cont, const char *msg);
void SnacCliAuthorize (Connection *serv, Contact *cont, BOOL accept, const char *msg);

#define SnacCliReqlists(serv)     SnacSend (serv, SnacC (serv, 19, 2, 0, 0))
#define SnacCliReqroster(serv)    SnacSend (serv, SnacC (serv, 19, 4, 0, 0))
#define SnacCliRosterack(serv)    SnacSend (serv, SnacC (serv, 19, 7, 0, 0))

#endif
