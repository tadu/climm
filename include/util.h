/* $Id$ */


UDWORD Chars_2_DW (UBYTE * buf);
UWORD Chars_2_Word (UBYTE * buf);

char *UtilStatus (UDWORD status);
const char *Convert_Status_2_Str (UDWORD status);
void Print_Status (UDWORD new_status);
int Print_UIN_Name (UDWORD uin);
int Print_UIN_Name_10 (UDWORD uin);
void Print_IP (UDWORD uin);

char *MsgEllipsis (const char *msg);

void Init_New_User (Session *sess);

int putlog (Session *sess, time_t stamp, UDWORD uin, 
            UDWORD status, enum logtype level, UWORD type, char *str, ...);
void clrscr (void);
void init_log (void);
void Hex_Dump (void *buffer, size_t len);

void ExecScript (char *script, UDWORD uin, long num, char *data);
const char *GetUserBaseDir (void);
const char *UtilFill (const char *fmt, ...);
const char *UtilIP (UDWORD ip);

UDWORD UtilCheckUIN (Session *sess, UDWORD uin);
