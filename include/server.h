
void Auto_Reply (Session *sess, SIMPLE_MESSAGE_PTR s_mesg);
char *Get_Auto_Reply (Session *sess);
void icq_sendmsg (Session *sess, UDWORD uin, char *text, UDWORD msg_type);
void icq_sendurl (Session *sess, UDWORD uin, char *description, char *url);
