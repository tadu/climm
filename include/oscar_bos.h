
#ifndef CLIMM_OSCAR_BOS_H
#define CLIMM_OSCAR_BOS_H

jump_snac_f SnacSrvBoserr, SnacSrvReplybos;

#define SnacCliReqbos(serv)       SnacSend (serv, SnacC (serv, 9, 2, 0, 0))

void SnacCliAddvisible (Server *serv, Contact *cont);
void SnacCliRemvisible (Server *serv, Contact *cont);
void SnacCliAddinvis (Server *serv, Contact *cont);
void SnacCliReminvis (Server *serv, Contact *cont);

#endif
