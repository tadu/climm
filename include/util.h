/* $Id$ */

#ifndef MICQ_UTIL_H
#define MICQ_UTIL_H

char *MsgEllipsis (const char *msg);

void Init_New_User (Connection *conn);

int putlog (Connection *conn, time_t stamp, UDWORD uin, 
            UDWORD status, enum logtype level, UWORD type, char *str, ...);

void EventExec (Contact *cont, const char *script, UBYTE type, UDWORD msgtype, const char *text);

#endif /* MICQ_UTIL_H */
