/* $Id$ */

char *MsgEllipsis (const char *msg);

void Init_New_User (Session *sess);

int putlog (Session *sess, time_t stamp, UDWORD uin, 
            UDWORD status, enum logtype level, UWORD type, char *str, ...);
void clrscr (void);
void init_log (void);
void Hex_Dump (void *buffer, size_t len);

void ExecScript (char *script, UDWORD uin, long num, char *data);

UDWORD UtilCheckUIN (Session *sess, UDWORD uin);
