
#ifndef CLIMM_OSCAR_ICBM_H
#define CLIMM_OSCAR_ICBM_H

jump_snac_f SnacSrvIcbmerr, SnacSrvReplyicbm, SnacSrvRecvmsg,
    SnacSrvAckmsg, SnacSrvSrvackmsg;

void SnacCliSeticbm (Connection *serv);
UBYTE SnacCliSendmsg (Connection *serv, Contact *cont, UBYTE format, Message *msg);

void SrvMsgAdvanced (Packet *pak, UDWORD seq, UWORD type,
                     status_t status, status_t deststatus, UWORD flags, const char *msg);
void SrvMsgGreet (Packet *pak, UWORD cmd, const char *reason, UWORD port, UDWORD len, const char *msg);
void SrvReceiveAdvanced (Connection *serv, Event *inc_event, Packet *inc_pak, Event *ack_event);

#define SnacCliReqicbm(serv)     SnacSend (serv, SnacC (serv, 4, 4, 0, 0))

#endif
