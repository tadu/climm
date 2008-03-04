
#ifndef CLIMM_OSCAR_CONTACT_H
#define CLIMM_OSCAR_CONTACT_H

jump_snac_f SnacSrvContacterr, SnacSrvReplybuddy, SnacSrvUseronline,
    SnacSrvContrefused, SnacSrvUseroffline;

void SnacCliAddcontact (Server *serv, Contact *cont, ContactGroup *cg);
void SnacCliRemcontact (Server *serv, Contact *cont, ContactGroup *cg);

#define SnacCliReqbuddy(serv)     SnacSend (serv, SnacC (serv, 3, 2, 0, 0))

#endif
