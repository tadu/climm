/* $Id$ */

#define TAB_SLOTS 10

void   TabAddUIN (UDWORD uin);
int    TabHasNext (void);
UDWORD TabGetNext (void);
void   TabReset (void);
