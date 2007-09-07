/* $Id$ */

#ifndef CLIMM_UTIL_TABS_H
#define CLIMM_UTIL_TABS_H

void           TabInit (void);
void           TabAddIn  (const Contact *cont);
void           TabAddOut (const Contact *cont);
const Contact *TabGet    (int nr);
time_t         TabTime   (int nr);
int            TabHas    (const Contact *cont);

#endif /* CLIMM_UTIL_TABS_H */
