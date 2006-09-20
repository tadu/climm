/* $Id$ */

#ifndef MICQ_OLDICQ_CLIENT_H
#define MICQ_OLDICQ_CLIENT_H

void   CmdPktCmdAck          (Connection *conn, UDWORD seq);
void   CmdPktCmdSendMessage  (Connection *conn, Contact *cont, const char *text, UDWORD type);
void   CmdPktCmdTCPRequest   (Connection *conn, Contact *cont, UDWORD port);
void   CmdPktCmdLogin        (Connection *conn);
void   CmdPktCmdRegNewUser   (Connection *conn, const char *pass);
void   CmdPktCmdContactList  (Connection *conn);
void   CmdPktCmdSearchUser   (Connection *conn, const char *email, const char *nick, const char *first, const char *last);
void   CmdPktCmdKeepAlive    (Connection *conn);
void   CmdPktCmdSendTextCode (Connection *conn, const char *text);
void   CmdPktCmdAckMessages  (Connection *conn);
void   CmdPktCmdLogin1       (Connection *conn);
void   CmdPktCmdExtInfoReq   (Connection *conn, Contact *cont);
void   CmdPktCmdStatusChange (Connection *conn, status_t status);
void   CmdPktCmdUpdateInfo   (Connection *conn, const char *email, const char *nick, const char *first, const char *last, BOOL auth);
void   CmdPktCmdRandSet      (Connection *conn, UDWORD group);
UDWORD CmdPktCmdRandSearch   (Connection *conn, UDWORD group);
void   CmdPktCmdMetaGeneral  (Connection *conn, Contact *cont);
void   CmdPktCmdMetaMore     (Connection *conn, Contact *cont);
void   CmdPktCmdMetaAbout    (Connection *conn, const char *about);
void   CmdPktCmdMetaPass     (Connection *conn, char *pass);
UDWORD CmdPktCmdMetaReqInfo  (Connection *conn, Contact *cont);
void   CmdPktCmdMetaSearchWP (Connection *conn, MetaWP *user);
void   CmdPktCmdInvisList    (Connection *conn);
void   CmdPktCmdVisList      (Connection *conn);
void   CmdPktCmdUpdateList   (Connection *conn, Contact *cont, int which, BOOL add);

#endif /* MICQ_ICQV5_CLI_H */
