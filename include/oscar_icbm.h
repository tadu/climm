
#ifndef MICQ_OSCAR_ICBM_H
#define MICQ_OSCAR_ICBM_H

jump_snac_f SnacSrvIcbmerr, SnacSrvReplyicbm, SnacSrvRecvmsg,
    SnacSrvAckmsg, SnacSrvSrvackmsg;

void SnacCliSeticbm (Connection *serv);
UBYTE SnacCliSendmsg (Connection *serv, Contact *cont, const char *text, UDWORD type, UBYTE format);
UBYTE SnacCliSendmsg2 (Connection *serv, Contact *cont, Opt *opt);

#define SnacCliReqicbm(serv)     SnacSend (serv, SnacC (serv, 4, 4, 0, 0))

#endif
