
#ifndef MICQ_OSCAR_ICBM_H
#define MICQ_OSCAR_ICBM_H

jump_snac_f SnacSrvIcbmerr, SnacSrvReplyicbm, SnacSrvRecvmsg,
    SnacSrvAckmsg, SnacSrvSrvackmsg;

void SnacCliSeticbm (Connection *serv);
UBYTE SnacCliSendmsg (Connection *serv, Contact *cont, const char *text, UDWORD type, UBYTE format);
UBYTE SnacCliSendmsg2 (Connection *serv, Contact *cont, Opt *opt);

void SrvMsgAdvanced (Packet *pak, UDWORD seq, UWORD type,
                     UWORD status, UDWORD deststatus, UWORD flags, const char *msg);
void SrvMsgGreet (Packet *pak, UWORD cmd, const char *reason, UWORD port, UDWORD len, const char *msg);
void SrvReceiveAdvanced (Connection *serv, Event *inc_event, Packet *inc_pak, Event *ack_event);

#define SnacCliReqicbm(serv)     SnacSend (serv, SnacC (serv, 4, 4, 0, 0))

#endif
