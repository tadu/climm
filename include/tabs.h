/* $Id$ */

#ifndef MICQ_UTIL_TABS_H
#define MICQ_UTIL_TABS_H

void   TabInit (void);
void           TabAddIn  (const Contact *cont);
void           TabAddOut (const Contact *cont);
const Contact *TabGetIn  (int nr);
const Contact *TabGetOut (int nr);
int            TabHas    (const Contact *cont);

#endif /* MICQ_UTIL_TABS_H */
