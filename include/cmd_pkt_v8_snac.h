

void SrvCallBackSnac (struct Event *event);
const char *SnacName (UWORD fam, UWORD cmd);
void SnacPrint (Packet *pak);

void SnacCliFamilies     (Session *sess);
void SnacCliRatesrequest (Session *sess);
void SnacCliReqinfo      (Session *sess);
void SnacCliReqlocation  (Session *sess);
void SnacCliBuddy        (Session *sess);
void SnacCliReqicbm      (Session *sess);
void SnacCliReqbos       (Session *sess);
void SnacCliSetuserinfo  (Session *sess);
void SnacCliSetstatus    (Session *sess, UWORD status);
void SnacCliReady        (Session *sess);
void SnacCliSeticbm      (Session *sess);
void SnacCliSendcontactlist (Session *sess);
void SnacCliReqroster    (Session *sess);
void SnacCliSendmsg      (Session *sess, UDWORD uin, char *text, UDWORD type);
/*void  (Session *sess);
void  (Session *sess);
void  (Session *sess);
void  (Session *sess);*/
