/* $Id$ */

#ifndef MICQ_IM_SRV_H
#define MICQ_IM_SRV_H

#define IREP_HASAUTHFLAG 1

void Meta_User (Connection *conn, UDWORD uin, Packet *p);
void Display_Rand_User (Connection *conn, Packet *pak);
void Recv_Message (Connection *conn, Packet *pak);
void ack_srv (Connection *conn, UDWORD seq);
void Display_Info_Reply (Connection *conn, Packet *pak, const char *uinline, unsigned int flags);
void Display_Ext_Info_Reply (Connection *conn, Packet *pak, const char *uinline);

void IMSrvMsg  (Contact *cont, Connection *conn, time_t stamp, UDWORD tstatus, UWORD type, const char *text, Event *event);
void IMOnline  (Contact *cont, Connection *conn, UDWORD status);
void IMOffline (Contact *cont, Connection *conn);

#endif /* MICQ_IM_SRV_H */
