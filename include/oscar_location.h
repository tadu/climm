
#ifndef MICQ_OSCAR_LOCATION_H
#define MICQ_OSCAR_LOCATION_H

jump_snac_f SnacSrvReplylocation;

#define SnacCliReqlocation(serv)  SnacSend (serv, SnacC (serv, 2, 2, 0, 0))

void SnacCliSetuserinfo (Connection *serv);

#endif
