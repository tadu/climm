/* $Id$ */

#ifndef CLIMM_UTIL_PARSE_H
#define CLIMM_UTIL_PARSE_H

strc_t        s_parse_s     (const char **input, const char *sep);
Contact      *s_parsenick_s (const char **input, const char *sep, Connection *serv);
ContactGroup *s_parsecg_s   (const char **input, const char *sep, Connection *serv);
ContactGroup *s_parselist_s (const char **input, BOOL rem,        Connection *serv);
char         *s_parserem_s  (const char **input, const char *sep);

BOOL          s_parseint_s  (const char **input, UDWORD        *parsed, const char *sep);
BOOL          s_parsekey_s  (const char **input, const char *kw,        const char *sep);

int is_valid_icq_name  (char *user);
int is_valid_aim_name  (char *user);
int is_valid_xmpp_name (char *user);
int is_valid_msn_name  (char *user);

#define DEFAULT_SEP " \t\r\n"
#define MULTI_SEP   " \t\r\n,"

#define s_parse(i)          s_parse_s     (i, DEFAULT_SEP)
#define s_parsenick(i,s)    s_parsenick_s (i, DEFAULT_SEP, s)
#define s_parsecg(i,s)      s_parsecg_s   (i, DEFAULT_SEP, s)
#define s_parselist(i,s)    s_parselist_s (i, 0, s)
#define s_parselistrem(i,s) s_parselist_s (i, 1, s)
#define s_parserem(i)       s_parserem_s  (i, DEFAULT_SEP)
#define s_parseint(i,p)     s_parseint_s  (i, p, DEFAULT_SEP)
#define s_parsekey(i,k)     s_parsekey_s  (i, k, DEFAULT_SEP)

#endif /* CLIMM_UTIL_PARSE_H */
