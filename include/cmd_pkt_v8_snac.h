/* $Id$ */

#include "cmd_pkt_cmd_v5.h"

void SrvCallBackSnac (Event *event);
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
void SnacCliGrantauth    (Session *sess, UDWORD uin);
void SnacCliReqauth      (Session *sess, UDWORD uin, const char *msg);
void SnacCliAuthorize    (Session *sess, UDWORD uin, BOOL accept, const char *msg);
void SnacCliAddvisible   (Session *sess, UDWORD uin);
void SnacCliRemvisible   (Session *sess, UDWORD uin);
void SnacCliAddinvis     (Session *sess, UDWORD uin);
void SnacCliReminvis     (Session *sess, UDWORD uin);
void SnacCliSendmsg      (Session *sess, UDWORD uin, const char *text, UDWORD type, UBYTE format);
void SnacCliReqofflinemsgs  (Session *sess);
void SnacCliAckofflinemsgs  (Session *sess);
void SnacCliRegisteruser    (Session *sess);
void SnacCliMetareqinfo     (Session *sess, UDWORD uin);
void SnacCliMetasetabout    (Session *sess, const char *text);
void SnacCliMetasetmore     (Session *sess, const MetaMore *user);
void SnacCliMetasetgeneral  (Session *sess, const MetaGeneral *user);
void SnacCliMetasetpass     (Session *sess, const char *newpass);
void SnacCliSendsms         (Session *sess, const char *target, const char *text);
void SnacCliSearchbypersinf (Session *sess, const char *email, const char *nick, const char *name, const char *surname);
void SnacCliSearchbymail    (Session *sess, const char *email);
void SnacCliSearchwp        (Session *sess, const MetaWP *wp);
void SnacCliSearchrandom    (Session *sess, UWORD group);
void SnacCliSetrandom       (Session *sess, UWORD group);

#define AIM_CAPS_ICQSERVERRELAY "\x09\x46\x13\x49\x4c\x7f\x11\xd1\x82\x22\x44\x45\x53\x54\x00\x00"
#define AIM_CAPS_ICQRTF         "\x97\xb1\x27\x51\x24\x3c\x43\x34\xad\x22\xd6\xab\xf7\x3f\x14\x92"
#define AIM_CAPS_ICQ            "\x09\x46\x13\x44\x4c\x7f\x11\xd1\x82\x22\x44\x45\x53\x54\x00\x00"
#define AIM_CAPS_UNK            "\x09\x46\x13\x4e\x4c\x7f\x11\xd1\x82\x22\x44\x45\x53\x54\x00\x00"

