/* $Id$ */
#ifndef UTIL_OTR
#define UTIL_OTR

#include "climm.h"

#include "im_response.h"

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

int OTRMsgIn (Contact *cont, fat_srv_msg_t *msg);
Message *OTRMsgOut (Message *msg);

void OTRContext (Contact *cont);
void OTRStart (Contact *cont, UBYTE start);
void OTRSetTrust (Contact *cont, UBYTE trust);
void OTRGenKey (void);
void OTRListKeys (void);
#endif /* ENABLE_OTR */
#endif
