
#ifndef CLIMM_OSCAR_LOCATION_H
#define CLIMM_OSCAR_LOCATION_H

jump_snac_f SnacSrvLocationerr, SnacSrvReplylocation, SnacSrvUserinfo;

#define SnacCliReqlocation(serv)  SnacSend (serv, SnacC (serv, 2, 2, 0, 0))

void SnacCliSetuserinfo (Server *serv);
void SnacCliRequserinfo (Server *serv, Contact *victim, UWORD type);

#endif
