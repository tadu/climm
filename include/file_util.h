
int   Save_RC();
int   Add_User (SOK_T sok, UDWORD uin, char *name);
#ifdef UNIX
void  Get_Unix_Config_Info (void);
#endif
void  Set_rcfile (const char * name);
