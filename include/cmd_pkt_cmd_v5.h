
typedef struct
{
   char  *nick,   *first,  *last; 
   char  *email,  *email2, *email3;
   char  *city,   *state;
   char  *phone,  *fax;
   char  *street, *cellular;
   UDWORD zip;
   UWORD  country; 
   UBYTE  tz;
   BOOL   auth, webaware, hideip;
} MetaGeneral;

typedef struct
{
    UWORD age;
    UBYTE sex;
    char *hp;
    UBYTE year, month, day;
    UBYTE lang1, lang2, lang3;
} MetaMore;

typedef struct
{
   char *nick, *first, *last, *email;
   UWORD minage, maxage;
   UBYTE sex, language;
   char *city, *state;
   UWORD country;
   char *company, *department, *position;
   BOOL online;
} MetaWP;

void CmdPktCmdAck (Session *sess, UDWORD seq);
void CmdPktCmdSendMessage (Session *sess, UDWORD uin, const char *text, UDWORD type);
void CmdPktCmdTCPRequest (Session *sess, UDWORD tuin, UDWORD port);
void CmdPktCmdLogin (Session *sess);
void CmdPktCmdRegNewUser (Session *sess, const char *pass);
void CmdPktCmdContactList (Session *sess);
void CmdPktCmdSearchUser (Session *sess, const char *email, const char *nick, const char *first, const char *last);
void CmdPktCmdKeepAlive (Session *sess);
void CmdPktCmdSendTextCode (Session *sess, const char *text);
void CmdPktCmdAckMessages (Session *sess);
void CmdPktCmdLogin1 (Session *sess);
void CmdPktCmdExtInfoReq (Session *sess, UDWORD uin);
void CmdPktCmdStatusChange (Session *sess, UDWORD status);
void CmdPktCmdUpdateInfo (Session *sess, const char *email, const char *nick, const char *first, const char *last, BOOL auth);
void CmdPktCmdRandSet (Session *sess, UDWORD group);
void CmdPktCmdRandSearch (Session *sess, UDWORD group);
void CmdPktCmdMetaGeneral (Session *sess, MetaGeneral *user);
void CmdPktCmdMetaMore (Session *sess, MetaMore *info);
void CmdPktCmdMetaAbout (Session *sess, const char *about);
void CmdPktCmdMetaPass (Session *sess, char *pass);
void CmdPktCmdMetaReqInfo (Session *sess, UDWORD uin);
void CmdPktCmdMetaSearchWP (Session *sess, MetaWP *user);
void CmdPktCmdInvisList (Session *sess);
void CmdPktCmdVisList (Session *sess);
void CmdPktCmdUpdateList (Session *sess, UDWORD uin, int which, BOOL add);


