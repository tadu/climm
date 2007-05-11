/* $Id$ */
#ifndef UTIL_OTR
#define UTIL_OTR

#include "micq.h"

#ifdef ENABLE_OTR
#define OTR_CMD_LIST    (1)
#define OTR_CMD_START   (2)
#define OTR_CMD_STOP    (3)
#define OTR_CMD_TRUST   (4)
#define OTR_CMD_GENKEY  (5)
#define OTR_CMD_KEYS    (6)

void OTRInit ();
void OTREnd ();
void OTRFree (char *msg);

int OTRMsgIn (const char *msg, Contact *cont, char **new_msg);
int OTRMsgOut (const char *msg, Connection *conn, Contact *cont, char **new_msg);

int OTRCmd (int cmd, Contact *cont, const char *arg);
#endif /* ENABLE_OTR */
#endif
