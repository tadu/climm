/* $Id$ */

#ifndef MICQ_UTIL_H
#define MICQ_UTIL_H

char *MsgEllipsis (const char *msg);

void Init_New_User (Connection *conn);

int putlog (Connection *conn, time_t stamp, UDWORD uin, 
            UDWORD status, enum logtype level, UWORD type, char *str, ...);
void clrscr (void);
void Hex_Dump (void *buffer, size_t len);

void ExecScript (char *script, UDWORD uin, long num, char *data);
void EventExec (Contact *cont, const char *script, UBYTE type, UDWORD msgtype, const char *text);

UDWORD UtilCheckUIN (Connection *conn, UDWORD uin);

#endif /* MICQ_UTIL_H */
