/* $Id$ */

#ifndef CLIMM_UTIL_H
#define CLIMM_UTIL_H

typedef enum {
   ev_msg    = 1, /* a message has arrived */
   ev_on     = 2, /* a contact went online */
   ev_off    = 3, /* a contact went offline */
   ev_beep   = 4, /* a beep should be generated */
   ev_status = 5, /* a contact changed status */
   ev_other       /* anything else */
} evtype_t;

int putlog (Server *conn, time_t stamp, Contact *cont, 
            status_t status, UDWORD nativestatus, enum logtype level, UWORD type, const char *str);

void EventExec (Contact *cont, const char *script, evtype_t type, UDWORD msgtype, status_t status, const char *text);

#endif /* CLIMM_UTIL_H */
