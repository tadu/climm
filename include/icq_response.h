
void Meta_User (Session *sess, UBYTE * data, UDWORD len, UDWORD uin);
void Display_Rand_User (Session *sess, UBYTE * data, UDWORD len);
void Recv_Message (Session *sess, UBYTE * pak);
void User_Offline (Session *sess, UBYTE * pak);
void User_Online (Session *sess, Packet *pak);
void Status_Update (Session *sess, UBYTE * pak);
void ack_srv (Session *sess, UDWORD seq);
void Display_Info_Reply (Session *sess, UBYTE * pak);
void Display_Ext_Info_Reply (Session *sess, UBYTE * pak);
void Display_Search_Reply (Session *sess, UBYTE * pak);
void Do_Msg (Session *sess, UDWORD type, UWORD len, const char *data, UDWORD uin, BOOL tcp);
void UserOnlineSetVersion (Contact *con, UDWORD tstamp);
