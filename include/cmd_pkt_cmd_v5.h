/* $Id$ */

#ifndef MICQ_ICQV5_CLI_H
#define MICQ_ICQV5_CLI_H

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
    UWORD year;
    UBYTE month, day;
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

void CmdPktCmdAck          (Connection *conn, UDWORD seq);
void CmdPktCmdSendMessage  (Connection *conn, UDWORD uin, const char *text, UDWORD type);
void CmdPktCmdTCPRequest   (Connection *conn, UDWORD tuin, UDWORD port);
void CmdPktCmdLogin        (Connection *conn);
void CmdPktCmdRegNewUser   (Connection *conn, const char *pass);
void CmdPktCmdContactList  (Connection *conn);
void CmdPktCmdSearchUser   (Connection *conn, const char *email, const char *nick, const char *first, const char *last);
void CmdPktCmdKeepAlive    (Connection *conn);
void CmdPktCmdSendTextCode (Connection *conn, const char *text);
void CmdPktCmdAckMessages  (Connection *conn);
void CmdPktCmdLogin1       (Connection *conn);
void CmdPktCmdExtInfoReq   (Connection *conn, UDWORD uin);
void CmdPktCmdStatusChange (Connection *conn, UDWORD status);
void CmdPktCmdUpdateInfo   (Connection *conn, const char *email, const char *nick, const char *first, const char *last, BOOL auth);
void CmdPktCmdRandSet      (Connection *conn, UDWORD group);
void CmdPktCmdRandSearch   (Connection *conn, UDWORD group);
void CmdPktCmdMetaGeneral  (Connection *conn, MetaGeneral *user);
void CmdPktCmdMetaMore     (Connection *conn, MetaMore *info);
void CmdPktCmdMetaAbout    (Connection *conn, const char *about);
void CmdPktCmdMetaPass     (Connection *conn, char *pass);
void CmdPktCmdMetaReqInfo  (Connection *conn, UDWORD uin);
void CmdPktCmdMetaSearchWP (Connection *conn, MetaWP *user);
void CmdPktCmdInvisList    (Connection *conn);
void CmdPktCmdVisList      (Connection *conn);
void CmdPktCmdUpdateList   (Connection *conn, UDWORD uin, int which, BOOL add);

#endif /* MICQ_ICQV5_CLI_H */
