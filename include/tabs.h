/* $Id$ */

#ifndef MICQ_UTIL_TABS_H
#define MICQ_UTIL_TABS_H

#define TAB_SLOTS 10

void   TabAddUIN (UDWORD uin);
int    TabHasNext (void);
UDWORD TabGetNext (void);
void   TabReset (void);

#endif /* MICQ_UTIL_TABS_H */
