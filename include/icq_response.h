/* $Id$ */

#define IREP_HASAUTHFLAG 1

void Meta_User (Session *sess, UDWORD uin, Packet *p);
void Display_Rand_User (Session *sess, Packet *pak);
void Recv_Message (Session *sess, Packet *pak);
void ack_srv (Session *sess, UDWORD seq);
void Display_Info_Reply (Session *sess, Packet *pak, const char *uinline, unsigned int flags);
void Display_Ext_Info_Reply (Session *sess, Packet *pak, const char *uinline);

void IMSrvMsg (Contact *cont, Session *sess, time_t stamp, UWORD type, const char *text, UDWORD tstatus);
