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
UDWORD      s_strnlen (const char *str, UDWORD len);
UDWORD      s_offset  (const char *str, UDWORD offset);
const char *s_realpath(const char *path);

BOOL s_parse_s     (char **input, char    **parsed, char *sep);
BOOL s_parsenick_s (char **input, Contact **parsed, char *sep, Contact **parsedr, Connection *serv);
BOOL s_parserem_s  (char **input, char    **parsed, char *sep);
BOOL s_parseint_s  (char **input, UDWORD   *parsed, char *sep);

#define s_repl(old,new) do { char **_p_p_ = old; const char *_q_q_ = new; \
                             if (*_p_p_) free (*_p_p_); *_p_p_ = NULL; \
                             if (_q_q_) *_p_p_ = strdup (_q_q_); } while (0)

#define s_free(old)     do { char *_p_p_ = old; if (_p_p_) free (_p_p_); } while (0)
#define s_now           s_time (NULL)

#define DEFAULT_SEP " \t\r\n"
#define MULTI_SEP   " \t\r\n,"

#define s_parse(i,p)          s_parse_s     (i, p, DEFAULT_SEP)
#define s_parsenick(i,p,pr,s) s_parsenick_s (i, p, DEFAULT_SEP, pr, s)
#define s_parserem(i,p)       s_parserem_s  (i, p, DEFAULT_SEP)
#define s_parseint(i,p)       s_parseint_s  (i, p, DEFAULT_SEP)

#endif /* MICQ_UTIL_STR_H */
