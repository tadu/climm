/* $Id$ */

#ifndef MICQ_IM_SRV_H
#define MICQ_IM_SRV_H

void HistShow (Contact *cont);
void HistMsg (Connection *conn, Contact *cont, time_t stamp, const char *msg, UWORD inout);

typedef enum {
  INT_FILE_ACKED, INT_FILE_REJED, INT_FILE_ACKING, INT_FILE_REJING, INT_CHAR_REJING,
  INT_MSGTRY_DC, INT_MSGTRY_TYPE2, INT_MSGTRY_V8, INT_MSGTRY_V5,
  INT_MSGACK_DC, INT_MSGACK_SSL, INT_MSGACK_TYPE2, INT_MSGACK_V8, INT_MSGACK_V5,
  INT_MSGDISPL, INT_MSGCOMP, INT_MSGNOCOMP, INT_MSGOFF
} int_msg_t;

void IMIntMsg    (Contact *cont, time_t stamp, status_t tstatus, int_msg_t type, const char *text);
void IMIntMsgFat (Contact *cont, time_t stamp, status_t tstatus, int_msg_t type, const char *text,
                  const char *opt_text, UDWORD port, UDWORD bytes);
void IMSrvMsg    (Contact *cont, time_t stamp, UDWORD opt_origin, UDWORD opt_type, const char *text);
void IMSrvMsgFat (Contact *cont, time_t stamp, Opt *opt);
void IMOnline  (Contact *cont, status_t status, statusflag_t flags, UDWORD nativestatus, const char *text);
void IMOffline (Contact *cont);

#define HIST_IN 1
#define HIST_OUT 2

#endif /* MICQ_IM_SRV_H */
