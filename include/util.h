const char *Convert_Status_2_Str (UDWORD status);
BOOL        Log_Dir_Normal (void);
const char *Get_Log_Dir (void);
const char *Set_Log_Dir (const char *newpath);

UDWORD Chars_2_DW (UBYTE * buf);
UWORD Chars_2_Word (UBYTE * buf);
void DW_2_Chars (UBYTE * buf, UDWORD num);
void Word_2_Chars (UBYTE * buf, UWORD num);

void Print_CMD (UWORD cmd);
void Print_Status (UDWORD new_status);
int Print_UIN_Name (UDWORD uin);
int Print_UIN_Name_10 (UDWORD uin);
void Print_IP (UDWORD uin);

char *MsgEllipsis (char *msg);
char *UIN2nick (UDWORD uin);
char *UIN2Name (UDWORD uin);
UDWORD nick2uin (char *nick);
CONTACT_PTR UIN2Contact (UDWORD uin);

void Init_New_User (void);
UDWORD Get_Port (UDWORD uin);
int log_event (UDWORD uin, int type, char *str, ...);
void clrscr (void);
void Hex_Dump (void *buffer, size_t len);

void ExecScript (char *script, UDWORD uin, long num, char *data);
