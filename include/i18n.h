/* $Id$ */

#ifndef MICQ_I18N_H
#define MICQ_I18N_H

#define _i18n(i,s) #i ":" s

int          i18nOpen (const char *);
void         i18nClose (void);
const char  *i18n (int, const char *);

#endif /* MICQ_I18N_H */
