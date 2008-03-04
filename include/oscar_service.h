
#ifndef CLIMM_OSCAR_SERVICE_H
#define CLIMM_OSCAR_SERVICE_H

jump_snac_f SnacSrvServiceerr, SnacSrvFamilies, SnacSrvRates, SnacSrvRateexceeded,
    SnacServerpause, SnacSrvReplyinfo, SnacSrvFamilies2, SnacSrvMotd;

void SnacCliReady (Server *serv);
void SnacCliFamilies (Server *serv);
void SnacCliSetstatus (Server *serv, status_t status, UWORD action);
void CliFinishLogin (Server *serv);

#define SnacCliRatesrequest(serv) SnacSend (serv, SnacC (serv, 1, 6, 0, 0))
#define SnacCliReqinfo(serv)      SnacSend (serv, SnacC (serv, 1, 14, 0, 0))

#endif
