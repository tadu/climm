/* $Id$ */

int  Save_RC(void);
void Initialize_RC_File (void);
int  Read_RC_File (FILE *rcf);
void PrefReadStat (FILE *stf);
int PrefWriteStatusFile (void);
int PrefWriteConfFile (void);

Connection *PrefNewConnection (UDWORD uin, const char *passwd);
