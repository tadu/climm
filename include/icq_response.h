/* $Id$ */

#ifndef MICQ_IM_SRV_H
#define MICQ_IM_SRV_H

void HistShow (Contact *cont);

void IMIntMsg  (Contact *cont, Connection *conn, time_t stamp, UDWORD tstatus, UWORD type, const char *text, Opt *opt);
void IMSrvMsg  (Contact *cont, Connection *conn, time_t stamp, Opt *opt);
void IMOnline  (Contact *cont, Connection *conn, UDWORD status);
void IMOffline (Contact *cont, Connection *conn);

#define INT_FILE_ACKED    1
#define INT_FILE_REJED    2
#define INT_FILE_ACKING   3
#define INT_FILE_REJING   4
#define INT_CHAR_REJING   5
#define INT_MSGTRY_DC     6
#define INT_MSGTRY_TYPE2  7
#define INT_MSGTRY_V8     8
#define INT_MSGTRY_V5     9
#define INT_MSGACK_DC    10
#define INT_MSGACK_SSL   14
#define INT_MSGACK_TYPE2 11
#define INT_MSGACK_V8    12
#define INT_MSGACK_V5    13

#endif /* MICQ_IM_SRV_H */
