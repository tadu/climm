/* $Id$ */

#ifndef MICQ_MREADLINE_H
#define MICQ_MREADLINE_H

#define Echo_Off()
#define Echo_On()  

void   M_print  (const char *str);
void   M_printf (const char *str, ...) __attribute__ ((format (__printf__, 1, 2)));
int    M_pos (void);
void   M_logo (const char *logo);
void   M_logo_clear (void);

#endif /* MICQ_MREADLINE_H */
