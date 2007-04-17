/* $Id$ */

#ifndef MICQ_UTIL_STR_H
#define MICQ_UTIL_STR_H

typedef struct str_s str_s;
typedef struct strc_s strc_s;
typedef struct str_s *str_t;
typedef const struct str_s *strc_t;
typedef const struct strc_s *strx_t;

struct str_s
{
    char  *txt;
    UDWORD len;
    UDWORD max;
};

struct strc_s
{
    const char *txt;
    UDWORD len;
    UDWORD max;
};

str_t       s_init    (str_t str, const char *init, size_t add);
str_t       s_blow    (str_t str, size_t len);
str_t       s_cat     (str_t str, const char *add);
str_t       s_catc    (str_t str, char add);
str_t       s_catn    (str_t str, const char *add, size_t len);
str_t       s_catf    (str_t str, const char *fmt, ...) __attribute__ ((format (__printf__, 2, 3)));
str_t       s_insn    (str_t str, size_t pos, const char *ins, size_t len);
str_t       s_insc    (str_t str, size_t pos, char ins);
str_t       s_delc    (str_t str, size_t pos);
str_t       s_deln    (str_t str, size_t pos, size_t len);
void        s_done    (str_t str);

const char *s_sprintf (const char *fmt, ...) __attribute__ ((format (__printf__, 1, 2)));
const char *s_ip      (UDWORD ip);
const char *s_status  (status_t status, UDWORD nativestatus);
const char *s_status_short  (status_t status);
const char *s_time    (time_t *stamp);
const char *s_strftime (time_t *stamp, const char *fmt, char as_gmt);
const char *s_msgtok  (char *);

const char *s_dump    (const UBYTE *data, UWORD len);
const char *s_dumpnd  (const UBYTE *data, UWORD len);
const char *s_ind     (const char *str);
UDWORD      s_strlen  (const char *str);
UDWORD      s_strnlen (const char *str, UDWORD len);
UDWORD      s_offset  (const char *str, UDWORD offset);
strc_t      s_split   (const char **str, UBYTE enc, int len);
const char *s_realpath(const char *path);

const char *s_quote       (const char *input);
const char *s_cquote      (const char *input, const char *color);
const char *s_mquote      (const char *input, const char *color, BOOL allownl);

#define s_qquote(i)      s_cquote (i, COLQUOTE)
#define s_wordquote(i)   s_mquote (i, COLQUOTE, 0)
#define s_msgquote(i)    s_mquote (i, COLQUOTE, 1)

#define s_repl(old,new) do { char **_p_p_ = old; const char *_q_q_ = new; \
                             if (*_p_p_) free (*_p_p_); *_p_p_ = NULL; \
                             if (_q_q_) *_p_p_ = strdup (_q_q_); } while (0)

#define s_free(old)     do { char *_p_p_ = old; if (_p_p_) free (_p_p_); } while (0)
#define s_now           s_time (NULL)

#endif /* MICQ_UTIL_STR_H */
