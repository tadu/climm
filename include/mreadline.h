/* $Id$ */

#ifndef MICQ_MREADLINE_H
#define MICQ_MREADLINE_H

#ifdef USE_MREADLINE

void R_init (void);                          /* init mreadline lib */
void R_undraw (void);                        /* hide input (defer) */
void R_redraw (void);                        /* unhide (redraw) input line */
void R_setprompt (const char *prompt);       /* set prompt */
void R_setpromptf (const char *prompt, ...); /* set prompt formatted */
void R_resetprompt (void);                   /* reset prompt to standard */
void R_remprompt (void);                     /* remove hidden prompt */
void R_prompt (void);                        /* type prompt */
int  R_process_input (void);                 /* parse input, returns 1 if CR typed */
void R_getline (char *buf, int len);         /* returns line */
void R_pause (void);                         /* pause mreadline befor system () */
void R_resume (void);                        /* resume ... */
void R_goto (int pos);                       /* go to position in input line */

#else /* USE_MREADLINE */

#define R_init()            {}
#define R_undraw()          M_print ("\n")
#define R_redraw            R_resetprompt
#define R_setprompt         M_print
#define R_setpromptf        M_printf
#define R_resetprompt       R_setprompt (i18n (1040, "mICQ> "))
#define R_remprompt         {}
#define R_prompt            R_resetprompt
#define R_process_input()   1
#define R_getline(buf,len)  M_fdnreadln (STDIN_FILENO, buf, len)
#define R_pause()           {}
#define R_resume()          {}
#define R_goto ()           {}

#endif /* USE_MREADLINE */
#endif /* MICQ_MREADLINE_H */
