
int   Save_RC(void);
int   Add_User (Session *sess, UDWORD uin, char *name);
void  Get_Unix_Config_Info (Session *sess);
void  Set_rcfile (const char * name);

void Initalize_RC_File (void);
void Read_RC_File (FILE *rcf);
