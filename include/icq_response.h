/* $Id$ */

#ifndef MICQ_IM_SRV_H
#define MICQ_IM_SRV_H

#define IREP_HASAUTHFLAG 1
void Meta_User (Connection *conn, UDWORD uin, Packet *pak);
void Display_Rand_User (Connection *conn, Packet *pak);
void Recv_Message (Connection *conn, Packet *pak);
void Display_Info_Reply (Connection *conn, Contact *cont, Packet *pak, UBYTE flags);
void Display_Ext_Info_Reply (Connection *conn, Packet *pak);

void IMIntMsg  (Contact *cont, Connection *conn, time_t stamp, UDWORD tstatus, UWORD type, const char *text, Extra *extra);
void IMSrvMsg  (Contact *cont, Connection *conn, time_t stamp, UDWORD tstatus, UWORD type, const char *text, Event *event);
void IMOnline  (Contact *cont, Connection *conn, UDWORD status);
void IMOffline (Contact *cont, Connection *conn);

#define INT_FILE_ACKED    1
#define INT_FILE_REJED    2
#define INT_FILE_ACKING   3
#define INT_FILE_REJING   4
#define INT_CHAR_REJING   5
#define INT_MSGTRY_DC     6
#define INT_MSGTRY_TYPE2  7
#define INT_MSGTRY_V8     8
#define INT_MSGTRY_V5     9
#define INT_MSGACK_DC    10
#define INT_MSGACK_TYPE2 11
#define INT_MSGACK_V8    12
#define INT_MSGACK_V5    13

#endif /* MICQ_IM_SRV_H */
