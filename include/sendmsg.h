
void icq_sendmsg (SOK_T sok, UDWORD uin, char *text, UDWORD msg_type);
void icq_sendauthmsg (SOK_T sok, UDWORD uin);
void icq_rand_user_req (SOK_T sok, UDWORD group);
void icq_rand_set (SOK_T sok, UDWORD group);
void icq_change_status (SOK_T sok, UDWORD status);
void icq_sendurl (SOK_T sok, UDWORD uin, char *description, char *url);

void snd_contact_list (int sok);
void snd_invis_list (int sok);
void snd_vis_list (int sok);
void snd_login_1 (int sok);
void snd_got_messages (SOK_T sok);

size_t SOCKWRITE (SOK_T sok, void *ptr, size_t len);
size_t SOCKREAD (SOK_T sok, void *ptr, size_t len);

void send_info_req (SOK_T sok, UDWORD uin);
void send_ext_info_req (SOK_T sok, UDWORD uin);
void start_search (SOK_T sok, char *email, char *nick, char *first, char *last);
void reg_new_user (SOK_T sok, char *pass);
void Change_Password (SOK_T sok, char *pass);
void Update_User_Info (SOK_T sok, USER_INFO_PTR user);
void Update_More_User_Info (SOK_T sok, MORE_INFO_PTR user);
void Update_Other (SOK_T sok, OTHER_INFO_PTR info);
void Update_About (SOK_T sok, const char *about);
void update_list (int sok, UDWORD uin, int which, BOOL add);
void Keep_Alive (int sok);
void Quit_ICQ (int sok);
void Initialize_Msg_Queue (void);
void Do_Resend (SOK_T sok);
