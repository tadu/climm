
#include "cmd_pkt_cmd_v5.h"

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
void SnacCliSetstatus    (Session *sess, UWORD status, UWORD action);
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
void SnacCliReqofflinemsgs  (Session *sess);
void SnacCliAckofflinemsgs  (Session *sess);
void SnacCliRegisteruser    (Session *sess);
void SnacCliMetareqinfo     (Session *sess, UDWORD uin);
void SnacCliMetasetabout    (Session *sess, const char *text);
void SnacCliMetasetmore     (Session *sess, const MetaMore *user);
void SnacCliMetasetgeneral  (Session *sess, const MetaGeneral *user);
void SnacCliSearchbypersinf (Session *sess, const char *email, const char *nick, const char *name, char *surname);
void SnacCliSearchbymail    (Session *sess, const char *email);
void SnacCliSearchwp        (Session *sess, const MetaWP *wp);
void SnacCliRandsearch      (Session *sess, UDWORD group);
