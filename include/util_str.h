/* $Id$ */

#ifndef MICQ_UTIL_STR_H
#define MICQ_UTIL_STR_H

const char *s_sprintf (const char *fmt, ...);
const char *s_ip      (UDWORD ip);
const char *s_status  (UDWORD status);
const char *s_time    (time_t *stamp);
const char *s_msgtok  (char *);
char       *s_cat     (char *str, UDWORD *size, const char *add);
char       *s_catf    (char *str, UDWORD *size, const char *fmt, ...);
const char *s_dump    (const UBYTE *data, UWORD len);
const char *s_dumpnd  (const UBYTE *data, UWORD len);
const char *s_ind     (const char *str);
UDWORD      s_strlen  (const char *str);
UDWORD      s_offset  (const char *str, UDWORD offset);

BOOL s_parse     (char **input, char **parsed);
BOOL s_parsenick (char **input, Contact **parsed, Contact **parsedr, Connection *serv);
BOOL s_parserem  (char **input, char **parsed);
BOOL s_parseint  (char **input, UDWORD *parsed);

#define s_repl(old,new) do { char **_p_p_ = old; const char *_q_q_ = new; \
                             if (*_p_p_) free (*_p_p_); *_p_p_ = NULL; \
                             if (_q_q_) *_p_p_ = strdup (_q_q_); } while (0)

#define s_free(old)     do { char *_p_p_ = old; if (_p_p_) free (_p_p_); } while (0)
#define s_now           s_time (NULL)

#endif /* MICQ_UTIL_STR_H */
