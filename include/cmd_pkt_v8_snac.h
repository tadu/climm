

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
void SnacCliAddcontact   (Session *sess, UDWORD uin);
void SnacCliRemcontact   (Session *sess, UDWORD uin);
void SnacCliReqroster    (Session *sess);
void SnacCliAddvisible   (Session *sess, UDWORD uin);
void SnacCliRemvisible   (Session *sess, UDWORD uin);
void SnacCliAddinvis     (Session *sess, UDWORD uin);
void SnacCliReminvis     (Session *sess, UDWORD uin);
void SnacCliSendmsg      (Session *sess, UDWORD uin, char *text, UDWORD type);
void SnacCliReqOfflineMsgs (Session *sess);
void SnacCliAckOfflineMsgs (Session *sess);
/*void  (Session *sess);
void  (Session *sess);
void  (Session *sess);
void  (Session *sess);*/
