/* $Id$ */

#ifndef CLIMM_I18N_H
#define CLIMM_I18N_H

#define _i18n(i,s) #i ":" s

void         i18nInit (const char *loc);
int          i18nOpen (const char *loc);
void         i18nClose (void);
const char  *i18n (int, const char *) __attribute__ ((format_arg (2)));

#endif /* CLIMM_I18N_H */
