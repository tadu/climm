
void Meta_User (SOK_T sok, UBYTE * data, UDWORD len, UDWORD uin);
void Display_Rand_User (SOK_T sok, UBYTE * data, UDWORD len);
void Recv_Message (int sok, UBYTE * pak);
void User_Offline (int sok, UBYTE * pak);
void User_Online (int sok, UBYTE * pak);
void Status_Update (int sok, UBYTE * pak);
void Login (int sok, int UIN, char *pass, int ip, int port, UDWORD status);
void ack_srv (SOK_T sok, UDWORD seq);
void Display_Info_Reply (int sok, UBYTE * pak);
void Display_Ext_Info_Reply (int sok, UBYTE * pak);
void Display_Search_Reply (int sok, UBYTE * pak);
void Do_Msg (SOK_T sok, UDWORD type, UWORD len, char *data, UDWORD uin, BOOL tcp);
