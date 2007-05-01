/* $Id$ */

#ifndef MICQ_IM_RESPONSE_H
#define MICQ_IM_RESPONSE_H

void HistShow (Contact *cont);
void HistMsg (Connection *conn, Contact *cont, time_t stamp, const char *msg, UWORD inout);

typedef enum { pm_leaf, pm_parent } parentmode_t;
typedef enum { st_on, st_ch, st_off } change_t;

typedef enum {
  INT_FILE_ACKED, INT_FILE_REJED, INT_FILE_ACKING, INT_FILE_REJING, INT_CHAR_REJING,
  INT_MSGTRY_DC, INT_MSGTRY_TYPE2, INT_MSGTRY_V8, INT_MSGTRY_V5,
  INT_MSGACK_DC, INT_MSGACK_SSL, INT_MSGACK_TYPE2, INT_MSGACK_V8, INT_MSGACK_V5,
  INT_MSGDISPL, INT_MSGCOMP, INT_MSGNOCOMP, INT_MSGOFF
} int_msg_t;

typedef struct {
    const char *orig_data;
    const char *msgtext;
    const char *opt_text;
    UDWORD port;
    UDWORD bytes;
    int_msg_t type;
    status_t tstatus;
} fat_int_msg_t;

typedef struct {
    const char *orig_data;
    char *msgtext;
    const char *subj;
    const char *url;
    const char *tmp[6];
    UDWORD type;
    UDWORD origin;
    UDWORD nativestatus;
    UDWORD bytes;
    UDWORD ref;
} fat_srv_msg_t;

typedef int (cb_status)  (Contact *cont, parentmode_t pm, change_t ch, const char *text);
typedef int (cb_int_msg) (Contact *cont, parentmode_t pm, time_t stamp, fat_int_msg_t *msg);
typedef int (cb_srv_msg) (Contact *cont, parentmode_t pm, time_t stamp, fat_srv_msg_t *msg);

void IMIntMsg    (Contact *cont, time_t stamp, status_t tstatus, int_msg_t type, const char *text);
void IMIntMsgFat (Contact *cont, time_t stamp, status_t tstatus, int_msg_t type, const char *text,
                  const char *opt_text, UDWORD port, UDWORD bytes);
void IMSrvMsg    (Contact *cont, time_t stamp, UDWORD opt_origin, UDWORD opt_type, const char *text);
void IMSrvMsgFat (Contact *cont, time_t stamp, Opt *opt);
void IMOnline  (Contact *cont, status_t status, statusflag_t flags, UDWORD nativestatus, const char *text);
void IMOffline (Contact *cont);

#define HIST_IN 1
#define HIST_OUT 2

#endif /* MICQ_IM_RESPONSE_H */
