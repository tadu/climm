/*
 * This file contains static string helper functions.
 *
 * This file is Copyright © Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 */

#include "micq.h"
#include <stdarg.h>
#include <netinet/in.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <string.h>
#include <ctype.h>
#include "util_str.h"
#include "preferences.h"
#include "contact.h"
#include "conv.h"
#include "session.h" /* conn->contacts */
#include "util.h"

/*
 * Return a static formatted string.
 */
const char *s_sprintf (const char *fmt, ...)
{
    static char *buf = NULL;
    static UDWORD size = 0;
    va_list args;
    char *nbuf;
    UDWORD rc;

    if (!buf)
        buf = calloc (1, size = 1024);

    while (1)
    {
        buf[size - 2] = '\0';
        va_start (args, fmt);
        rc = vsnprintf (buf, size, fmt, args);
        va_end (args);
        
        if (rc != -1 && rc < size && !buf[size - 2])
            break;

        nbuf = malloc (size + 1024);
        if (!nbuf)
            break;
        buf = nbuf;
        size += 1024;
    }
    return buf;
}

/*
 * Appends to a dynamically allocated string.
 */
char *s_cat (char *str, UDWORD *size, const char *add)
{
    UDWORD nsize;
    char *nstr;
    
    if (!str)
        str = strdup ("");
    if (!size)
        return str;
    
    nsize = strlen (nstr = str) + strlen (add) + 1;
    if (nsize > *size)
    {
        nstr = realloc (str, nsize += 64);
        if (nstr)
            *size = nsize;
        else
        {
            nstr = str;
            nsize = *size;
        }
    }
    strcat (nstr, add);
    return nstr;
}


/*
 * Appends to a dynamically allocated string.
 */
char *s_catf (char *str, UDWORD *size, const char *fmt, ...)
{
    va_list args;
    UDWORD nsize;
    char *nstr;
    
    if (!str)
        str = strdup ("");
    if (!size)
        return str;
    
    nsize = strlen (nstr = str) + 1024;
    if (nsize > *size)
    {
        nstr = realloc (str, nsize += 64);
        if (nstr)
            *size = nsize;
        else
        {
            nstr = str;
            nsize = *size;
        }
    }
    
    va_start (args, fmt);
    vsnprintf (nstr + strlen (nstr), nsize - strlen (nstr), fmt, args);
    va_end (args);
    
    return nstr;
}

/*
 * Return a static string consisting of the given IP.
 */
const char *s_ip (UDWORD ip)
{
    struct sockaddr_in sin;
    sin.sin_addr.s_addr = htonl (ip);
    return s_sprintf ("%s", inet_ntoa (sin.sin_addr));
}

/*
 * Return a static string describing the status.
 */
const char *s_status (UDWORD status)
{
    static char buf[200];
    
    if (status == STATUS_OFFLINE)
        return i18n (1969, "offline");
 
    if (status & STATUSF_INV)
        snprintf (buf, sizeof (buf), "%s-", i18n (1975, "invisible"));
    else
        buf[0] = '\0';
    
    if (status & STATUSF_DND)
        snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), "%s", i18n (1971, "do not disturb"));
    else if (status & STATUSF_OCC)
        snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), "%s", i18n (1973, "occupied"));
    else if (status & STATUSF_NA)
        snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), "%s", i18n (1974, "not available"));
    else if (status & STATUSF_AWAY)
        snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), "%s", i18n (1972, "away"));
    else if (status & STATUSF_FFC)
        snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), "%s", i18n (1976, "free for chat"));
    else if (buf[0])
        buf[strlen (buf) - 1] = '\0';
    else
        snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), "%s", i18n (1970, "online"));
    
    if (prG->verbose)
        snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), " %08lx", status);
    
    return buf;
}

/*
 * Returns static string describing the given time.
 */
const char *s_time (time_t *stamp)
{
    struct timeval p = {0L, 0L};
    struct tm now;
    struct tm *thetime;
    static char tbuf[40];

#ifdef HAVE_GETTIMEOFDAY
    if (gettimeofday (&p, NULL) == -1)
#endif
    {
        p.tv_usec = 0L;
        p.tv_sec = time (NULL);
    }

    now = *localtime (&p.tv_sec);

    thetime = (!stamp || *stamp == NOW) ? &now : localtime (stamp);

    strftime(tbuf, sizeof (tbuf), thetime->tm_year == now.tm_year 
        && thetime->tm_mon == now.tm_mon && thetime->tm_mday == now.tm_mday
        ? "%X" : "%a %b %d %X %Y", thetime);

    if (prG->verbose > 7)
        snprintf (tbuf + strlen (tbuf), sizeof (tbuf) - strlen (tbuf),
                  ".%.06ld", !stamp || *stamp == NOW ? p.tv_usec : 0);
    else if (prG->verbose > 1)
        snprintf (tbuf + strlen (tbuf), sizeof (tbuf) - strlen (tbuf),
                  ".%.03ld", !stamp || *stamp == NOW ? p.tv_usec / 1000 : 0);
    
    return tbuf;
}

/*
 * strtok()/strsep() replacement, use to seperate message fields
 */
const char *s_msgtok (char *txt)
{
    static char *str = NULL;
    static char sep = '\0';
    char *p, *t;
    
    if (txt)
        str = txt;
    if (!sep)
        sep = ConvSep ();
    if (!str)
        return NULL;
    if (txt && !*str)
        return NULL;
    p = strchr (t = str, sep);
    if (p)
    {
        *p = '\0';
        str = p + 1;
    }
    else
    {
        str = NULL;
    }
    return t;
}

/*
 * Indent a string two characters.
 */
const char *s_ind (const char *str)
{
    static char *t = NULL;
    static UDWORD size = 0;
    UDWORD cnt = 5;
    const char *p;
    char *q;
    
    for (p = str; *p; p++, cnt++)
        if (*p == '\n')
            cnt += 2;
    if (!t)
        t = calloc (1, size = 100);
    if (size < cnt)
        t = realloc (t, cnt);
    if (!t)
        return str;
    q = t;
    *q++ = ' ';
    *q++ = ' ';
    for (p = str; *p; )
        if ((*q++ = *p++) == '\n')
            if (*p)
            {
                *q++ = ' ';
                *q++ = ' ';
            }
    *q = '\0';
    return t;
}

/*
 * Count the string length in unicode characters.
 */
UDWORD s_strlen (const char *str)
{
    UDWORD c;
    
    for (c = 0; *str; str++)
        if (!(*str & 0x80) || (*str & 0x40))
            c++;
    return c;
}

/*
 * Count the string length in unicode characters.
 */
UDWORD s_strnlen (const char *str, UDWORD len)
{
    UDWORD c;
    
    for (c = 0; *str && len; str++, len--)
        if (!(*str & 0x80) || (*str & 0x40))
            c++;
    return c;
}

/*
 * Gives the byte offset of a character offset in UTF-8.
 */
UDWORD s_offset  (const char *str, UDWORD offset)
{
    int off;
    for (off = 0; offset && *str; offset--, off++)
    {
        if (*str & 0x80)
        {
            str++;
            while (*str && ((*str & 0xc0) == 0x80))
                str++, off++;
        }
        else
            str++;
    }
    return off;
}

#ifdef ENABLE_UTF8
#define noctl(x) ((((x & 0x60) && x != 0x7f)) ? ConvUTF8 (x) : ".")
#else
#define noctl(x) ((((x & 0x60) && x != 0x7f)) ? x : '.')
#endif

/*
 * Hex dump to a string with ASCII.
 */
const char *s_dump (const UBYTE *data, UWORD len)
{
    static char *t = NULL;
    static UDWORD size = 0;
    UDWORD i, off;
    const unsigned char *d = (const unsigned char *)data;
    
    if (!t)
        t = calloc (1, size = 100);
    *t = '\0';
    while (len >= 16)
    {
        t = s_catf (t, &size, "%02x %02x %02x %02x %02x %02x %02x %02x  "
                              "%02x %02x %02x %02x %02x %02x %02x %02x  ",
                    d[0], d[1],  d[2],  d[3],  d[4],  d[5],  d[6],  d[7],
                    d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15]);
#ifdef ENABLE_UTF8
        t = s_cat (t, &size, "\"");
        t = s_cat (t, &size, noctl  (d[0]));  t = s_cat (t, &size, noctl  (d[1]));
        t = s_cat (t, &size, noctl  (d[2]));  t = s_cat (t, &size, noctl  (d[3]));
        t = s_cat (t, &size, noctl  (d[4]));  t = s_cat (t, &size, noctl  (d[5]));
        t = s_cat (t, &size, noctl  (d[6]));  t = s_cat (t, &size, noctl  (d[7]));
        t = s_cat (t, &size, " ");
        t = s_cat (t, &size, noctl  (d[8]));  t = s_cat (t, &size, noctl  (d[9]));
        t = s_cat (t, &size, noctl (d[10]));  t = s_cat (t, &size, noctl (d[11]));
        t = s_cat (t, &size, noctl (d[12]));  t = s_cat (t, &size, noctl (d[13]));
        t = s_cat (t, &size, noctl (d[14]));  t = s_cat (t, &size, noctl (d[15]));
        t = s_cat (t, &size, "'\n");
#else
        t = s_catf (t, &size, "'%c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c'\n",
                    noctl  (d[0]), noctl  (d[1]), noctl  (d[2]), noctl  (d[3]),
                    noctl  (d[4]), noctl  (d[5]), noctl  (d[6]), noctl  (d[7]),
                    noctl  (d[8]), noctl  (d[9]), noctl (d[10]), noctl (d[11]),
                    noctl (d[12]), noctl (d[13]), noctl (d[14]), noctl (d[15]));
#endif
        len -= 16;
        d += 16;
    }
    if (!len)
        return (t);
    for (off = i = 0; i <= 16; i++)
    {
        if (len)
        {
            t = s_catf (t, &size, "%02x ", *d++);
            len--;
            off++;
        }
        else if (i != 16)
            t = s_catf (t, &size, "   ");
        if (i == 7)
            t = s_catf (t, &size, " ");
        
    }
    t = s_catf (t, &size, " '");
    while (off)
    {
#ifdef ENABLE_UTF8
        t = s_catf (t, &size, "%s", noctl (*(d - off)));
#else
        t = s_catf (t, &size, "%c", noctl (*(d - off)));
#endif
        off--;
    }
    t = s_catf (t, &size, "'\n");
    return t;
}

/*
 * Hex dump to a string without ASCII.
 */
const char *s_dumpnd (const UBYTE *data, UWORD len)
{
    static char *t = NULL;
    static UDWORD size = 0;
    UDWORD i;
    const unsigned char *d = (const unsigned char *)data;
    
    if (t)
        *t = 0;
    while (len > 16)
    {
        t = s_catf (t, &size, "%02x %02x %02x %02x %02x %02x %02x %02x  "
                              "%02x %02x %02x %02x %02x %02x %02x %02x\\\n",
                    d[0], d[1],  d[2],  d[3],  d[4],  d[5],  d[6],  d[7],
                    d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15]);
        len -= 16;
        d += 16;
    }
    if (len > 10)
    {
        for (i = 0; i < len && i < 8; i++)
        {
            t = s_catf (t, &size, "%02x ", *d++);
        }
        t = s_catf (t, &size, "\n");
        len -= i;
    }
    for (i = 0; i < 10; i++)
    {
        if (len)
        {
            t = s_catf (t, &size, "%02x ", *d++);
            len--;
        }
        else
            t = s_catf (t, &size, "   ");
        if (i == 7)
            t = s_catf (t, &size, " ");
    }
    return t;
}

/*
 * Expand ~ and make absolute path.
 */
const char *s_realpath (const char *path)
{
    char *f = NULL;

    if (*path == '~' && path[1] == '/' && getenv ("HOME"))
        return s_sprintf ("%s%s", getenv ("HOME"), path + 1);
    if (*path == '/')
        return path;
#ifdef AMIGA
    if (strchr (path, ':') && (!strchr (path, '/') || strchr (path, '/') > strchr (path, ':')))
        return path;
#endif
    path = s_sprintf ("%s%s", PrefUserDir (prG), f = strdup (path));
    free (f);
    if (*path != '~' || path[1] != '/' || !getenv ("HOME"))
        return path;
    path = s_sprintf ("%s%s", getenv ("HOME"), (f = strdup (path)) + 1);
    free (f);
    return path;
}

/*
 * s_parse* - find a parameter of given type in string.
 * input is avdanced to point after the parsed argument,
 *   or to the next non-whitespace or end of string if not found.
 * parsed is cleared (0, NULL) if no suitable argument found.
 */

/*
 * Try to find a parameter in the string.
 * Result must NOT be free()d.
 */
BOOL s_parse_s (char **input, char **parsed, char *sep)
{
    static char *t = NULL;
    char *p = *input, *q;
    int s = 0;
    
    while (*p && strchr (sep, *p))
        p++;
    if (*p == '#')
    {
        while (*p)
            p++;
    }
    *input = p;
    if (!*p)
    {
        *parsed = NULL;
        return FALSE;
    }

    s_repl (&t, p);

    *parsed = q = t;
    if (!t)
    {
        return FALSE;
    }
    
    if (*p == '"')
    {
        s = 1;
        p++;
    }
    while (*p)
    {
        if (*p == '\\')
        {
            p++;
            if (*p == 'x' && p[1] && p[2])
            {
                p++;
                *q = (*p >= '0' && *p <= '9' ? *p - '0' : *p >= 'a' && *p <= 'f' ? *p - 'a' + 10 : *p - 'A' + 10) << 4;
                p++;
                *q = *p >= '0' && *p <= '9' ? *p - '0' : *p >= 'a' && *p <= 'f' ? *p - 'a' + 10 : *p - 'A' + 10;
                p++, q++;
                continue;
            }
            else if (*p)
            {
                *(q++) = *(p++);
                continue;
            }
        }
        if (*p == '"' && s)
        {
            *q = '\0';
            *input = p + 1;
            return TRUE;
        }
        if (!s && strchr (sep, *p))
            break;
        *(q++) = *(p++);
    }
    *q = '\0';
    *input = p;
    return TRUE;
}

/*
 * Try to find a nick name or uin in the string.
 * parsed is the Contact entry for this alias,
 * parsedr the "real" entry for this UIN.
 * parsed ->nick will be the nick given,
 * unless the user entered an UIN.
 */
BOOL s_parsenick_s (char **input, Contact **parsed, char *sep, Contact **parsedr, Connection *serv)
{
    ContactGroup *cg;
    Contact *r;
    char *p = *input, *t;
    UDWORD max, l, ll, i;
    
    while (*p && strchr (sep, *p))
        p++;
    *input = p;
    if (!*p)
    {
        *parsed = NULL;
        if (parsedr)
            *parsedr = NULL;
        return FALSE;
    }
    
    t = NULL;
    if (s_parse_s (&p, &t, sep))
    {
        *parsed = ContactFind (serv->contacts, 0, 0, t, 0);
        if (*parsed)
        {
            if (parsedr)
            {
                *parsedr = ContactFind (serv->contacts, 0, (*parsed)->uin, NULL, 0);
                if (!*parsedr)
                {
                    *parsed = NULL;
                    return FALSE;
                }
            }
            *input = p;
            return TRUE;
        }
    }
    p = *input;

    if (strchr ("0123456789", *p))
    {
        l = 0;
        if (s_parseint_s (&p, &max, sep))
        {
            if ((r = ContactUIN (serv, max)))
            {
                *parsed = r;
                if (parsedr)
                    *parsedr = r;
                *input = p;
                return TRUE;
            }
        }
    }
    max = 0;
    *parsed = NULL;
    ll = strlen (p);
    cg = serv->contacts;
    for (i = 0; (r = ContactIndex (cg, i)); i++)
    {
        l = strlen (r->nick);
        if (l > max && l <= ll && (!p[l] || strchr (sep, p[l])) && !strncasecmp (p, r->nick, l))
        {
            *parsed = r;
            max = strlen (r->nick);
        }
    }
    if (max)
    {
        if (parsedr)
        {
            *parsedr = ContactFind (serv->contacts, 0, (*parsed)->uin, NULL, 0);
            if (!*parsedr)
            {
                *parsed = NULL;
                return FALSE;
            }
        }
        *input = p + max;
        return TRUE;
    }
    return FALSE;
}

/*
 * Finds the remaining non-whitespace line.
 */
BOOL s_parserem_s (char **input, char **parsed, char *sep)
{
    static char *t = NULL;
    char *p = *input, *q, s = 0;
    
    while (*p && strchr (sep, *p))
        p++;
    *input = p;
    if (!*p)
    {
        *parsed = NULL;
        return FALSE;
    }

    if (*p == '"')
        s = 1;
    
    s_repl (&t, p);
    *parsed = q = t + s;
    q = q + strlen (q) - 1;
    while (strchr (sep, *q))
        *(q--) = '\0';
    if (s)
    {
        if (*q == '"')
            *(q--) = '\0';
        else
            (*parsed)--;
    }
    return TRUE;
}

/*
 * Try to find a number.
 */
BOOL s_parseint_s (char **input, UDWORD *parsed, char *sep)
{
    char *p = *input;
    UDWORD nr, sig;
    
    while (*p && strchr (sep, *p))
        p++;
    if (*p == '#')
    {
        while (*p)
            p++;
    }
    *input = p;
    if (!*p || !strchr ("0123456789+-", *p))
    {
        *parsed = 0;
        return FALSE;
    }

    nr = 0;
    sig = 1;

    if (*p == '+')
        p++;
    if (*p == '-')
    {
        sig = -1;
        p++;
    }

    while (*p && *p >= '0' && *p <= '9')
    {
        nr = nr * 10 + (*p - '0');
        p++;
    }
    if (*p && !strchr (sep, *p))
    {
        *parsed = 0;
        return FALSE;
    }
    *input = p;
    *parsed = nr * sig;
    return TRUE;
}

#define SUPERSAFE "%*+./0123456789:=@ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz"

/*
 * Quote a string for use in a config file
 */
const char *s_quote (const char *input)
{
    static char *t = NULL;
    static UDWORD size = 0;
    const char *tmp;
    
    if (!t)
        t = malloc (size = 32);
    if (!t || !input || !*input)
        return "";
    for (tmp = input; *tmp; tmp++)
        if (!strchr (SUPERSAFE, *tmp))
            break;
    if (*tmp)
        return input;
    *t = 0;
    t = s_cat (t, &size, "\"");
    for (tmp = input; *tmp; tmp++)
    {
        if (strchr ("\\\"", *tmp))
            t = s_catf (t, &size, "\\%c", *tmp);
        else if (*tmp & 0xe0)
            t = s_catf (t, &size, "%c", *tmp);
        else
            t = s_catf (t, &size, "\\x%c%c",
                    (*tmp / 16) <= 9 ? ((UBYTE)*tmp / 16) + '0'
                                     : ((UBYTE)*tmp / 16) - 10 + 'a',
                    (*tmp & 15) <= 9 ? (*tmp & 15) + '0'
                                     : (*tmp & 15) - 10 + 'a');
    }
    t = s_cat (t, &size, "\"");
    return t;
}
