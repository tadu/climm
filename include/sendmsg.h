
void icq_sendmsg (Session *sess, UDWORD uin, char *text, UDWORD msg_type);
void icq_sendurl (Session *sess, UDWORD uin, char *description, char *url);
size_t SOCKREAD (Session *sess, void *ptr, size_t len);
