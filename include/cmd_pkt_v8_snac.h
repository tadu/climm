

void SrvCallBackSnac (struct Event *event);
const char *SnacName (UWORD fam, UWORD cmd);

void SnacCliFamilies     (Session *sess);
void SnacCliRatesrequest (Session *sess);
void SnacCliReqinfo      (Session *sess);
void SnacCliReqlocation  (Session *sess);
void SnacCliBuddy        (Session *sess);
void SnacCliReqicbm      (Session *sess);
void SnacCliReqbos       (Session *sess);
void SnacCliSetuserinfo  (Session *sess);
void SnacCliSetstatus    (Session *sess);
void SnacCliReady        (Session *sess);
/*void  (Session *sess);
void  (Session *sess);
void  (Session *sess);
void  (Session *sess);
void  (Session *sess);
void  (Session *sess);
void  (Session *sess);*/
