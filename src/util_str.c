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
#include "util.h"

/*
 * Return a static formatted string.
 */
const char *s_sprintf (const char *fmt, ...)
{
    static char buf[1024];
    va_list args;

    va_start (args, fmt);
    vsnprintf (buf, sizeof (buf), fmt, args);
    va_end (args);

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
        nstr = realloc (str, nsize);
    if (nstr)
        *size = nsize;
    else
    {
        nstr = str;
        nsize = *size;
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
        nstr = realloc (str, nsize);
    if (nstr)
        *size = nsize;
    else
    {
        nstr = str;
        nsize = *size;
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
    t = s_catf (t, &size, "");
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
 * Gives the byte offset of a character offset in UTF-8.
 */
UDWORD s_offset  (const char *str, UDWORD offset)
{
    int off;
    for (off = 0; offset && *str; offset--, str++, off++)
    {
        if (*str & 0x80)
        {
            str++;
            while (*str && (~(*str) & 0x40))
                str++, off++;
            str--;
        }
    }
    return off;
}

/*
 * Hex dump to a string with ASCII.
 */
const char *s_dump (const UBYTE *data, UWORD len)
{
    static char *t = NULL;
    static UDWORD size = 0;
    UDWORD i, off;
    const unsigned char *d = (const unsigned char *)data;
    
    t = s_catf (t, &size, "");
    if (t)
        *t = 0;
    while (len >= 16)
    {
        t = s_catf (t, &size, "%02x %02x %02x %02x %02x %02x %02x %02x  "
                              "%02x %02x %02x %02x %02x %02x %02x %02x  ",
                    d[0], d[1],  d[2],  d[3],  d[4],  d[5],  d[6],  d[7],
                    d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15]);
        t = s_catf (t, &size, "'%c%c%c%c%c%c%c%c %c%c%c%c%c%c%c%c'\n",
                    isalnum  (d[0]) ?  d[0] : '.', isalnum  (d[1]) ?  d[1] : '.',
                    isalnum  (d[2]) ?  d[2] : '.', isalnum  (d[3]) ?  d[3] : '.',
                    isalnum  (d[4]) ?  d[4] : '.', isalnum  (d[5]) ?  d[5] : '.',
                    isalnum  (d[6]) ?  d[6] : '.', isalnum  (d[7]) ?  d[7] : '.',
                    isalnum  (d[8]) ?  d[8] : '.', isalnum  (d[9]) ?  d[9] : '.',
                    isalnum (d[10]) ? d[10] : '.', isalnum (d[11]) ? d[11] : '.',
                    isalnum (d[12]) ? d[12] : '.', isalnum (d[13]) ? d[13] : '.',
                    isalnum (d[14]) ? d[14] : '.', isalnum (d[15]) ? d[15] : '.');
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
        t = s_catf (t, &size, "%c", isalnum (*(d - off)) ? *(d - off) : '.');
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
 * s_parse* - find a parameter of given type in string.
 * input is avdanced to point after the parsed argument,
 *   or to the next non-whitespace or end of string if not found.
 * parsed is cleared (0, NULL) if no suitable argument found.
 */

/*
 * Try to find a parameter in the string.
 * Result must NOT be free()d.
 */
BOOL s_parse (char **input, char **parsed)
{
    static char *t = NULL;
    char *p = *input, *q;
    int s = 0;
    
    while (*p && strchr (" \t\r\n", *p))
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
        if (*p == '\\' && *(p + 1))
        {
            p++;
            *(q++) = *(p++);
            continue;
        }
        if (*p == '"' && s)
        {
            *q = '\0';
            *input = p + 1;
            return TRUE;
        }
        if (!s && strchr (" \r\t\n", *p))
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
BOOL s_parsenick (char **input, Contact **parsed, Contact **parsedr, Connection *serv)
{
    Contact *r;
    char *p = *input, *t;
    UDWORD max, l, ll;
    
    while (*p && strchr (" \t\r\n", *p))
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
    if (s_parse (&p, &t))
    {
        *parsed = ContactFindContact (t);
        if (*parsed)
        {
            if (parsedr)
            {
                *parsedr = ContactFind ((*parsed)->uin);
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
        if (s_parseint (&p, &max))
        {
            UtilCheckUIN (serv, max);
            if ((r = ContactFind (max)))
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
    for (r = ContactStart (); ContactHasNext (r); r = ContactNext (r))
    {
        l = strlen (r->nick);
        if (l > max && l <= ll && (!p[l] || strchr (" \t\r\n", p[l])) && !strncmp (p, r->nick, l))
        {
            *parsed = r;
            max = strlen (r->nick);
        }
    }
    if (max)
    {
        if (parsedr)
        {
            *parsedr = ContactFind ((*parsed)->uin);
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
BOOL s_parserem (char **input, char **parsed)
{
    static char *t = NULL;
    char *p = *input, *q, s = 0;
    
    while (*p && strchr (" \t\r\n", *p))
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

    if (*p == '"')
    {
        s = 1;
        p++;
    }
    
    s_repl (&t, p);
    *parsed = q = t;
    q = q + strlen (q) - 1;
    while (strchr (" \t\r\n", *q))
        *(q--) = '\0';
    if (s && *q == '"')
        *(q--) = '\0';
    return TRUE;
}

/*
 * Try to find a number.
 */
BOOL s_parseint (char **input, UDWORD *parsed)
{
    char *p = *input;
    UDWORD nr, sig;
    
    while (*p && strchr (" \t\r\n", *p))
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
    if (*p && !strchr (" \t\r\n", *p))
    {
        *parsed = 0;
        return FALSE;
    }
    *input = p;
    *parsed = nr * sig;
    return TRUE;
}

