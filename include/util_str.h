/* $Id$ */

#ifndef MICQ_UTIL_STR_H
#define MICQ_UTIL_STR_H

typedef struct str_s str_s;
typedef struct str_s *str_t;
typedef const struct str_s *strc_t;

struct str_s
{
    char  *txt;
    size_t len;
    size_t max;
};

str_t       s_init    (str_t str, const char *init, size_t len);
str_t       s_blow    (str_t str, size_t len);
str_t       s_cat     (str_t str, const char *add);
str_t       s_catc    (str_t str, char add);
str_t       s_catf    (str_t str, const char *fmt, ...) __attribute__ ((format (__printf__, 2, 3)));

const char *s_sprintf (const char *fmt, ...) __attribute__ ((format (__printf__, 1, 2)));
const char *s_ip      (UDWORD ip);
const char *s_status  (UDWORD status);
const char *s_time    (time_t *stamp);
const char *s_msgtok  (char *);

const char *s_dump    (const UBYTE *data, UWORD len);
const char *s_dumpnd  (const UBYTE *data, UWORD len);
const char *s_ind     (const char *str);
UDWORD      s_strlen  (const char *str);
UDWORD      s_strnlen (const char *str, UDWORD len);
UDWORD      s_offset  (const char *str, UDWORD offset);
const char *s_realpath(const char *path);

BOOL        s_parse_s     (char **input, char    **parsed, char *sep);
BOOL        s_parsenick_s (char **input, Contact **parsed, char *sep, Contact **parsedr, Connection *serv);
BOOL        s_parserem_s  (char **input, char    **parsed, char *sep);
BOOL        s_parseint_s  (char **input, UDWORD   *parsed, char *sep);
const char *s_quote       (const char *input);

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
