/* $Id$ */

#ifndef MICQ_MREADLINE_H
#define MICQ_MREADLINE_H

void R_init (void);                          /* init mreadline lib */
void R_undraw (void);                        /* hide input (defer) */
void R_redraw (void);                        /* unhide (redraw) input line */
void R_setprompt (const char *prompt);       /* set prompt */
void R_setpromptf (const char *prompt, ...) __attribute__ ((format (__printf__, 1, 2))); /* set prompt formatted */
void R_settimepromptf (const char *prompt, ...) __attribute__ ((format (__printf__, 1, 2))); /* set prompt formatted */
void R_resetprompt (void);                   /* reset prompt to standard */
void R_remprompt (void);                     /* remove hidden prompt */
void R_prompt (void);                        /* type prompt */
int  R_process_input (void);                 /* parse input, returns 1 if CR typed */
void R_getline (char *buf, int len);         /* returns line */
void R_pause (void);                         /* pause mreadline befor system () */
void R_resume (void);                        /* resume ... */
void R_goto (int pos);                       /* go to position in input line */

void R_clrscr (void);
UBYTE R_isinterrupted (void);

UWORD  Get_Max_Screen_Width ();
SDWORD Echo_Off (void);
SDWORD Echo_On (void);

void   M_print  (const char *str);
void   M_printf (const char *str, ...) __attribute__ ((format (__printf__, 1, 2)));
int    M_pos (void);
void   M_logo (const char *logo);
void   M_logo_clear (void);

#endif /* MICQ_MREADLINE_H */
