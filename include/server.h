
void Auto_Reply (Session *sess, UDWORD uin);
char *Get_Auto_Reply (Session *sess);
void icq_sendmsg (Session *sess, UDWORD uin, char *text, UDWORD msg_type);
void icq_sendurl (Session *sess, UDWORD uin, char *description, char *url);
