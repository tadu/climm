/* $Id$ */

#ifndef MICQ_IM_SRV_H
#define MICQ_IM_SRV_H

#define IREP_HASAUTHFLAG 1

void ExtraFree (MetaList *extra);
UDWORD ExtraGet (MetaList *extra, UWORD type);
MetaList *ExtraSet (MetaList *extra, UWORD type, UDWORD value, const char *text);

void Meta_User (Connection *conn, UDWORD uin, Packet *pak);
void Display_Rand_User (Connection *conn, Packet *pak);
void Recv_Message (Connection *conn, Packet *pak);
void Display_Info_Reply (Connection *conn, Contact *cont, Packet *pak, UBYTE flags);
void Display_Ext_Info_Reply (Connection *conn, Packet *pak);

void IMSrvMsg  (Contact *cont, Connection *conn, time_t stamp, UDWORD tstatus, UWORD type, const char *text, Event *event);
void IMOnline  (Contact *cont, Connection *conn, UDWORD status);
void IMOffline (Contact *cont, Connection *conn);

#endif /* MICQ_IM_SRV_H */
