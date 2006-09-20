
#ifndef MICQ_OSCAR_SERVICE_H
#define MICQ_OSCAR_SERVICE_H

jump_snac_f SnacSrvServiceerr, SnacSrvFamilies, SnacSrvRates, SnacSrvRateexceeded,
    SnacServerpause, SnacSrvReplyinfo, SnacSrvFamilies2, SnacSrvMotd;

void SnacCliReady (Connection *serv);
void SnacCliFamilies (Connection *serv);
void SnacCliSetstatus (Connection *serv, status_t status, UWORD action);

#define SnacCliRatesrequest(serv) SnacSend (serv, SnacC (serv, 1, 6, 0, 0))
#define SnacCliReqinfo(serv)      SnacSend (serv, SnacC (serv, 1, 14, 0, 0))

#endif
