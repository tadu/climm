/* $Id$ */

#ifndef MICQ_UTIL_TABS_H
#define MICQ_UTIL_TABS_H

void   TabInit (void);
void   TabAddIn (Contact *cont);
void   TabAddOut (Contact *cont);
Contact *TabGetIn (int nr);
Contact *TabGetOut (int nr);
Contact *TabGet (int nr);

#endif /* MICQ_UTIL_TABS_H */
