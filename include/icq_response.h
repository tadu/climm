#define IREP_HASAUTHFLAG 1

void Meta_User (Session *sess, UDWORD uin, Packet *p);
void Display_Rand_User (Session *sess, UBYTE * data, UDWORD len);
void Recv_Message (Session *sess, UBYTE * pak);
void ack_srv (Session *sess, UDWORD seq);
void Display_Info_Reply (Session *sess, Packet *pak, const char *uinline, unsigned int flags);
void Display_Ext_Info_Reply (Session *sess, Packet *pak, const char *uinline);
void Do_Msg (Session *sess, UDWORD type, UWORD len, const char *data, UDWORD uin, BOOL tcp);
void UserOnlineSetVersion (Contact *con, UDWORD tstamp, UDWORD tstamp2);
