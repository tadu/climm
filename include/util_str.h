/* $Id: */

const char *s_sprintf (const char *fmt, ...);
const char *s_ip (UDWORD ip);
const char *s_status (UDWORD status);
const char *s_time (time_t *stamp);

#define s_repl(old,new) do { char **_p_p_ = old; const char *_q_q_ = new; \
                             if (*_p_p_) free (*_p_p_); *_p_p_ = NULL; \
                             if (_q_q_) *_p_p_ = strdup (_q_q_); } while (0)

#define s_free(old)     do { char *_p_p_ = old; if (_p_p_) free (_p_p_); } while (0)
#define s_now s_time (NULL)
