
#ifndef CLIMM_OSCAR_REGISTER_H
#define CLIMM_OSCAR_REGISTER_H

jump_snac_f SnacSrvReplylocation, SnacSrvRegrefused, SnacSrvNewuin,
    SnacSrvReplylogin, SnacSrvLoginkey;

void SnacCliRegisteruser (Connection *serv);
void SnacCliReqlogin (Connection *serv);

#endif
