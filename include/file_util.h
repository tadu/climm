/* $Id$ */

int   Save_RC(void);
int   Add_User (Connection *conn, UDWORD uin, const char *name);
void  Get_Unix_Config_Info (Connection *conn);
void  Set_rcfile (const char * name);

void Initalize_RC_File (void);
void Read_RC_File (FILE *rcf);

void fHexDump (FILE *f, void *buffer, size_t len);
