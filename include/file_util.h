
int   Save_RC(Session *sess);
int   Add_User (Session *sess, UDWORD uin, char *name);
void  Get_Unix_Config_Info (Session *sess);
void  Set_rcfile (const char * name);

void Initalize_RC_File (Session *sess);
void Read_RC_File (Session *sess, FD_T rcf);
