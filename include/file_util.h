/* $Id$ */

int   Save_RC(void);
void  Get_Unix_Config_Info (Connection *conn);
void  Set_rcfile (const char * name);

void Initialize_RC_File (void);
void Read_RC_File (FILE *rcf);

void fHexDump (FILE *f, void *buffer, size_t len);
