/* $Id$ */

#ifndef MICQ_I18N_H
#define MICQ_I18N_H

#define _i18n(i,s) #i ":" s

void         i18nInit (char **loc, UBYTE *enc, const char *arg);
int          i18nOpen (const char *, UBYTE enc);
void         i18nClose (void);
const char  *i18n (int, const char *);

#endif /* MICQ_I18N_H */
