/*
 * This file contains static string helper functions.
 *
 * mICQ Copyright (C) © 2001,2002,2003 Rüdiger Kuhlmann
 *
 * mICQ is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 dated June, 1991.
 *
 * mICQ is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * $Id$
 */

#include "micq.h"
#include <stdarg.h>
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#if HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#if HAVE_WCTYPE_H
#include <wctype.h>
#endif
#include <string.h>
#include <ctype.h>
#include "preferences.h"
#include "contact.h"
#include "conv.h"
#include "session.h" /* conn->contacts */
#include "util.h"

/*
 * Initialize a dynamically allocated string.
 *
 * Returns NULL if not enough memory.
 */
str_t s_init (str_t str, const char *init, size_t add)
{
    UDWORD initlen, size;
    
    initlen = strlen (init);
    size = initlen + add + 2;
    
    if (!str->txt || size >= str->max)
    {
        if (str->txt && str->max)
            free (str->txt);
        str->max = 1 + (size | 0x3f);
        str->txt = malloc (str->max);
        if (!str->txt)
        {
            str->max = str->len = 0;
            str->txt = "\0";
            return NULL;
        }
    }
    str->len = initlen;
    memcpy (str->txt, init, initlen + 1);
    return str;
}

/*
 * Increase storage size of dynamically allocated string.
 *
 * Returns NULL if not enough memory.
 */
str_t s_blow (str_t str, size_t len)
{
    UDWORD newlen;
    char *tmp;
    
    newlen = 1 + ((str->max + len) | 0x3f);
    
    if (!str->txt || !str->max)
        tmp = malloc (newlen);
    else
        tmp = realloc (str->txt, newlen);
    
    if (!tmp)
    {
        if (!str->txt)
        {
            str->max = str->len = 0;
            str->txt = "\0";
        }
        return NULL;
    }
    str->txt = tmp;
    str->max = newlen;
    return str;
}


/*
 * Appends a byte to a dynamically allocated string.
 */
str_t s_catc (str_t str, char add)
{
    if (str->len + 2 >= str->max)
        if (!s_blow (str, str->len + 2 - str->max))
            return NULL;

    str->txt[str->len++] = add;
    str->txt[str->len] = '\0';
    return str;
}

/*
 * Appends bytes to a dynamically allocated string.
 */
str_t s_catn (str_t str, const char *add, size_t len)
{
    if (str->len + len + 2 >= str->max)
        if (!s_blow (str, str->len + len + 2 - str->max))
            return NULL;

    memcpy (str->txt + str->len, add, len);
    str->len += len;
    str->txt[str->len] = '\0';
    return str;
}

/*
 * Appends a string to a dynamically allocated string.
 */
str_t s_cat (str_t str, const char *add)
{
    int addlen = strlen (add);
    
    if (str->len + addlen + 1 >= str->max)
        if (!s_blow (str, str->len + addlen + 1 - str->max))
        {
            addlen = str->max - str->len - 2;
            if (addlen < 0)
                return NULL;
        }
    
    memcpy (str->txt + str->len, add, addlen + 1);
    str->len += addlen;
    return str;
}


/*
 * Appends a formatted string to a dynamically allocated string.
 */
str_t s_catf (str_t str, const char *fmt, ...)
{
    va_list args;
    size_t add = 64;
    int rc;
    
    s_blow (str, add);

    while (1)
    {
        if (str->len + 2 >= str->max)
            return NULL;
        str->txt[str->max - 2] = '\0';
        va_start (args, fmt);
        rc = vsnprintf (str->txt + str->len, str->max - str->len - 1, fmt, args);
        va_end (args);
        if (rc >= 0 && rc < str->max - str->len - 1 && !str->txt[str->max - 2])
        {
            str->len += strlen (str->txt + str->len);
            return str;
        }
        add = (rc > 0 ? rc - str->max - str->len + 5 : str->max);
        if (!s_blow (str, add))
        {
            str->txt[str->max - 2] = '\0';
            str->len = str->max - 2;
            return str;
        }
    }
    return str;
}

/*
 * Insert a string into a dynamically allocated string.
 */
str_t s_insn (str_t str, size_t pos, const char *ins, size_t len)
{
    if (pos > str->len)
        return NULL;
    if (str->len + len + 2 >= str->max)
        if (!s_blow (str, str->len + len + 2 - str->max))
            return NULL;
    
    memmove (str->txt + pos + len, str->txt + pos, str->len - pos + 1);
    memcpy (str->txt + pos, ins, len);
    str->len += len;
    return str;
}

/*
 * Insert a byte into a dynamically allocated string.
 */
str_t s_insc (str_t str, size_t pos, char ins)
{
    if (pos > str->len)
        return NULL;
    if (str->len + 3 >= str->max)
        if (!s_blow (str, str->len + 3 - str->max))
            return NULL;
    
    memmove (str->txt + pos + 1, str->txt + pos, str->len - pos + 1);
    str->txt[pos] = ins;
    str->len++;
    return str;
}

/*
 * Delete a byte from a dynamically allocated string.
 */
str_t s_delc (str_t str, size_t pos)
{
    if (pos > str->len)
        return NULL;
    memmove (str->txt + pos, str->txt + pos + 1, str->len - pos);
    str->len--;
    return str;
}

/*
 * Delete bytes from a dynamically allocated string.
 */
str_t s_deln (str_t str, size_t pos, size_t len)
{
    if (pos + len > str->len)
        return NULL;
    memmove (str->txt + pos, str->txt + pos + len, str->len - pos);
    str->len -= len;
    return str;
}


/*
 * Frees a dynamically allocated string.
 */
void s_done (str_t str)
{
    if (str->txt && str->max)
        free (str->txt);
    str->txt = "\0";
    str->len = str->max = 0;
}

/*
 * Return a static formatted string.
 */
const char *s_sprintf (const char *fmt, ...)
{
    static char *buf = NULL;
    static int size = 0;
    va_list args;
    char *nbuf;
    int rc, nsize;

    if (!buf)
        buf = calloc (1, size = 1024);

    while (1)
    {
        buf[size - 2] = '\0';
        va_start (args, fmt);
        rc = vsnprintf (buf, size, fmt, args);
        va_end (args);
        
        if (rc >= 0 && rc < size && !buf[size - 2])
            break;

        nsize = (rc > 0 ? rc + 5 : size * 2);
        nbuf = malloc (nsize);
        if (!nbuf)
            break;
        free (buf);
        buf = nbuf;
        size = nsize;
    }
    return buf;
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
    time_t nowsec;

#ifdef HAVE_GETTIMEOFDAY
    if (gettimeofday (&p, NULL) == -1)
#endif
    {
        p.tv_usec = 0L;
        nowsec = time (NULL);
    }
#ifdef HAVE_GETTIMEOFDAY
    else
        nowsec = p.tv_sec;
#endif

    now = *localtime (&nowsec);

    thetime = (!stamp || *stamp == NOW) ? &now : localtime (stamp);

    strftime(tbuf, sizeof (tbuf), thetime->tm_year == now.tm_year 
        && thetime->tm_mon == now.tm_mon && thetime->tm_mday == now.tm_mday
        ? "%H:%M:%S" : "%a %b %d %H:%M:%S %Y", thetime);

    if (prG->verbose > 7)
        snprintf (tbuf + strlen (tbuf), sizeof (tbuf) - strlen (tbuf),
                  ".%.06ld", !stamp || *stamp == NOW ? p.tv_usec : 0);
    else if (prG->verbose > 1)
        snprintf (tbuf + strlen (tbuf), sizeof (tbuf) - strlen (tbuf),
                  ".%.03ld", !stamp || *stamp == NOW ? p.tv_usec / 1000 : 0);
    
    return tbuf;
}

/*
 * strtok()/strsep() replacement, use to separate message fields
 */
const char *s_msgtok (char *txt)
{
    static char *str = NULL;
    char *p, *t;
    
    if (txt)
        str = txt;
    if (!str)
        return NULL;
    if (txt && !*str)
        return NULL;
    p = strchr (t = str, Conv0xFE);
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
    static str_s t;
    UDWORD cnt = 5;
    const char *p;
    char *q;
    
    if (!str || !*str)
        return "";

    for (p = str; *p; p++, cnt++)
        if (*p == '\n')
            cnt += 2;
    
    s_init (&t, "", cnt);
    if (t.max < cnt)
        return "<out of memory>";

    q = t.txt;
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
    return t.txt;
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

/*
 * Split off a certain amount of recoded bytes
 */
strc_t s_split (const char **input, UBYTE enc, int len)
{
    static str_s str = { NULL, 0, 0 };
    str_s in = { NULL, 0, 0 };
    int off, offold, offnl, offin, offmax;
    UDWORD ucs;
    
    in.txt = (char *) *input;
    in.len = strlen (*input);
    off = offnl = offin = offmax = 0;
    
    while (in.txt[off])
    {
        offold = off;
        ucs = ConvGetUTF8 (&in, &off);
        if (ucs == '\r')
            offnl = offold;
        if (isspace (ucs & 0xff))
            offin = offold;
        if ((off - offold) > len)
        {
            off = offold;
            break;
        }
        if (ucs == '\n')
            offnl = off;
        if (!iswalnum (ucs))
            offin = off;
        len -= (off - offold);
        if (!in.txt[off])
            offmax = off;
    }
    if (offmax)
        off = offmax;
    else if (offnl)
        off = offnl;
    else if (offin)
        off = offin;
    s_init (&str, "", off + 2);
    memcpy (str.txt, in.txt, off);
    str.len = off;
    str.txt[off] = 0;
    *input += off;
    return ConvTo (str.txt, enc);
}


#define noctl(x) ((((x & 0x60) && x != 0x7f)) ? ConvUTF8 (x) : ".")

/*
 * Hex dump to a string with ASCII.
 */
const char *s_dump (const UBYTE *data, UWORD len)
{
    static str_s t;
    UDWORD i, off;
    const unsigned char *d = (const unsigned char *)data;
    
    s_init (&t, "", 100);

    while (len >= 16)
    {
        s_catf (&t, "%02x %02x %02x %02x %02x %02x %02x %02x  "
                              "%02x %02x %02x %02x %02x %02x %02x %02x  ",
                    d[0], d[1],  d[2],  d[3],  d[4],  d[5],  d[6],  d[7],
                    d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15]);
        s_cat (&t, "\"");
        s_cat (&t, noctl  (d[0]));  s_cat (&t, noctl  (d[1]));
        s_cat (&t, noctl  (d[2]));  s_cat (&t, noctl  (d[3]));
        s_cat (&t, noctl  (d[4]));  s_cat (&t, noctl  (d[5]));
        s_cat (&t, noctl  (d[6]));  s_cat (&t, noctl  (d[7]));
        s_cat (&t, " ");
        s_cat (&t, noctl  (d[8]));  s_cat (&t, noctl  (d[9]));
        s_cat (&t, noctl (d[10]));  s_cat (&t, noctl (d[11]));
        s_cat (&t, noctl (d[12]));  s_cat (&t, noctl (d[13]));
        s_cat (&t, noctl (d[14]));  s_cat (&t, noctl (d[15]));
        s_cat (&t, "'\n");
        len -= 16;
        d += 16;
    }
    if (!len)
        return (t.txt);
    for (off = i = 0; i <= 16; i++)
    {
        if (len)
        {
            s_catf (&t, "%02x ", *d++);
            len--;
            off++;
        }
        else if (i != 16)
            s_catf (&t, "   ");
        if (i == 7)
            s_catc (&t, ' ');
        
    }
    s_catc (&t, ' ');
    s_catc (&t, '\'');
    while (off)
    {
        s_cat (&t, noctl (*(d - off)));
        off--;
    }
    s_catf (&t, "'\n");
    return t.txt;
}

/*
 * Hex dump to a string without ASCII.
 */
const char *s_dumpnd (const UBYTE *data, UWORD len)
{
    static str_s t;
    UDWORD i;
    const unsigned char *d = (const unsigned char *)data;
    
    s_init (&t, "", 100);

    while (len > 16)
    {
        s_catf (&t, "%02x %02x %02x %02x %02x %02x %02x %02x  "
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
            s_catf (&t, "%02x ", *d++);
        }
        s_catc (&t, '\n');
        len -= i;
    }
    for (i = 0; i < 10; i++)
    {
        if (len)
        {
            s_catf (&t, "%02x ", *d++);
            len--;
        }
        else
            s_cat (&t, "   ");
        if (i == 7)
            s_catc (&t, ' ');
    }
    return t.txt;
}

/*
 * Expand ~ and make absolute path.
 */
const char *s_realpath (const char *path)
{
    char *f = NULL;

    if (!*path)
        path = "";
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
 * input is advanced to point after the parsed argument,
 *   or to the next non-whitespace or end of string if not found.
 * parsed is cleared (0, NULL) if no suitable argument found.
 */

/*
 * Try to find a parameter in the string.
 * Result must NOT be free()d.
 */
strc_t s_parse_s (const char **input, const char *sep)
{
    static str_s str;
    strc_t parsed;
    const char *p = *input;
    char *q;
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
        return NULL;

    s_init (&str, p, 0);

    q = str.txt;
    parsed = &str;
    if (!q)
        return NULL;
    
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
                *q |= *p >= '0' && *p <= '9' ? *p - '0' : *p >= 'a' && *p <= 'f' ? *p - 'a' + 10 : *p - 'A' + 10;
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
            str.len = q - str.txt;
            return parsed;
        }
        if (!s && strchr (sep, *p))
            break;
        *(q++) = *(p++);
    }
    *q = '\0';
    *input = p;
    str.len = q - str.txt;
    return parsed;
}

/*
 * Try to find a nick name or uin in the string.
 */
Contact *s_parsenick_s (const char **input, const char *sep, Connection *serv)
{
    ContactGroup *cg;
    Contact *r, *parsed;
    const char *p = *input;
    strc_t t;
    UDWORD max, l, ll, i;
    
    while (*p && strchr (sep, *p))
        p++;
    *input = p;
    if (!*p)
        return NULL;
    
    if ((t = s_parse_s (&p, sep)))
    {
        if ((parsed = ContactFind (serv->contacts, 0, 0, t->txt)))
        {
            *input = p;
            return parsed;
        }
    }
    p = *input;

    if (strchr ("0123456789", *p))
    {
        l = 0;
        if (s_parseint_s (&p, &max, sep) && max)
        {
            if ((r = ContactUIN (serv, max)))
            {
                *input = p;
                return r;
            }
        }
    }
    max = 0;
    parsed = NULL;
    ll = strlen (p);
    cg = serv->contacts;
    for (i = 0; (r = ContactIndex (cg, i)); i++)
    {
        ContactAlias *ca;

        l = strlen (r->nick);
        if (l > max && l <= ll && (!p[l] || strchr (sep, p[l])) && !strncasecmp (p, r->nick, l))
        {
            parsed = r;
            max = strlen (r->nick);
        }
        
        for (ca = r->alias; ca; ca = ca->more)
        {
            l = strlen (ca->alias);
            if (l > max && l <= ll && (!p[l] || strchr (sep, p[l])) && !strncasecmp (p, ca->alias, l))
            {
                parsed = r;
                max = strlen (ca->alias);
            }
        }
    }
    if (max)
    {
        *input = p + max;
        return parsed;
    }
    return NULL;
}

/*
 * Try to find a contact group name in the string.
 */
ContactGroup *s_parsecg_s (const char **input, const char *sep, Connection *serv)
{
    ContactGroup *cg;
    const char *p = *input;
    strc_t t;
    UDWORD l, i;
    
    while (*p && strchr (sep, *p))
        p++;
    *input = p;

    if (!*p)
        return NULL;
    
    if ((t = s_parse_s (&p, sep)))
    {
        for (i = 0; (cg = ContactGroupIndex (i)); i++)
        {
            if (cg->serv == serv && !strcmp (cg->name, t->txt))
            {
                *input = p;
                return cg;
            }
        }
    }
    p = *input;

    for (i = 0; (cg = ContactGroupIndex (i)); i++)
    {
        if (!cg->name || !*cg->name)
            continue;
        l = strlen (cg->name);
        if (cg->serv == serv && !strncmp (cg->name, p, l)
            && (strchr (sep, p[l]) || !p[l]))
        {
            *input = p + l + (p[l] ? 1 : 0);
            return cg;
        }
    }
    return NULL;
}

/*
 * Find contact (and contact groups) names in the string.
 */
ContactGroup *s_parselist_s (const char **input, BOOL rem, Connection *serv)
{
    ContactGroup *cg, *ncg;
    Contact *cont;
    const char *p = *input;
    UDWORD i;
    
    ncg = ContactGroupC (serv, 0, "");
    while (*p)
    {
        while (*p && strchr (DEFAULT_SEP, *p))
            p++;
        if ((cg = s_parsecg_s (&p, MULTI_SEP, serv)))
        {
            for (i = 0; (cont = ContactIndex (cg, i)); i++)
                if (!ContactHas (ncg, cont))
                    ContactAdd (ncg, cont);
        }
        else if ((cont = s_parsenick_s (&p, MULTI_SEP, serv)))
        {
            if (!ContactHas (ncg, cont))
                ContactAdd (ncg, cont);
        }
        else
            break;
        if (!rem && *p != ',')
            break;
        if (*p)
            p++;
    }
    
    if (ContactIndex (ncg, 0) && (rem || strchr (DEFAULT_SEP, *p)))
    {
        *input = p;
        return ncg;
    }
    ContactGroupD (ncg);
    return NULL;
}

/*
 * Finds the remaining non-whitespace line, but parses '\'.
 */
char *s_parserem_s (const char **input, const char *sep)
{
    static char *t = NULL;
    const char *p = *input;
    char *q, *parsed;
    
    while (*p && strchr (sep, *p))
        p++;
    *input = p;
    if (!*p)
        return NULL;

    s_repl (&t, p);
    parsed = t;
    for (q = t ; *p; )
    {
        if (*p == '\\')
        {
            p++;
            if (*p == 'x' && p[1] && p[2])
            {
                p++;
                *q = (*p >= '0' && *p <= '9' ? *p - '0' : *p >= 'a' && *p <= 'f' ? *p - 'a' + 10 : *p - 'A' + 10) << 4;
                p++;
                *q |= *p >= '0' && *p <= '9' ? *p - '0' : *p >= 'a' && *p <= 'f' ? *p - 'a' + 10 : *p - 'A' + 10;
                p++, q++;
            }
            else if (*p)
                *(q++) = *(p++);
            else
                *(q++) = '\\';
        }
        else
            *(q++) = *(p++);
    }
    *q = '\0';
    *input = p;
    return parsed;
}

/*
 * Try to find a number.
 */
BOOL s_parseint_s (const char **input, UDWORD *parsed, const char *sep)
{
    const char *p = *input;
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
    if (!nr && *p == 'x')
    {
        p++;
        while (*p && ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')))
        {
            nr = nr * 16 + (*p >= '0' && *p <= '9' ? *p - '0' : *p >= 'a' && *p <= 'f' ? *p - 'a' + 10 : *p - 'A' + 10);
            p++;
        }
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

/*
 * Try to find a keyword.
 */
BOOL s_parsekey_s (const char **input, const char *keyword, const char *sep)
{
    const char *p = *input;
    
    while (*p && strchr (sep, *p))
        p++;
    *input = p;
    
    while (*keyword == *p && *p)
        keyword++, p++;
    
    if (*keyword)
        return FALSE;
    
    *input = p;
    return TRUE;
}


#define SUPERSAFE "%*+-_.:=@/0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"

/*
 * Quote a string for use in a config file
 */
const char *s_quote (const char *input)
{
    static str_s t;
    const char *tmp;
    
    if (!input || !*input)
        return "\"\"";
    for (tmp = input; *tmp; tmp++)
        if (!strchr (SUPERSAFE, *tmp))
            break;
    if (!*tmp)
        return input;
    
    s_init (&t, "", 100);

    s_catc (&t, '\"');
    for (tmp = input; *tmp; tmp++)
    {
        if (strchr ("\\\"", *tmp))
        {
            s_catc (&t, '\\');
            s_catc (&t, *tmp);
        }
        else if (*tmp & 0xe0)
            s_catc (&t, *tmp);
        else
        {
            s_catc (&t, '\\');
            s_catc (&t, 'x');
            s_catc (&t, (*tmp / 16) <= 9 ? ((UBYTE)*tmp / 16) + '0'
                                     : ((UBYTE)*tmp / 16) - 10 + 'a');
            s_catc (&t, (*tmp & 15) <= 9 ? (*tmp & 15) + '0'
                                     : (*tmp & 15) - 10 + 'a');
        }
    }
    s_catc (&t, '\"');
    return t.txt;
}

/*
 * Quote a string for display. May use quotes to make sure test boundary is clear.
 */
const char *s_cquote (const char *input, const char *color)
{
    static str_s t;
    const char *tmp;
    
    if (!input || !*input)
        return "\"\"";
    s_init (&t, "", 100);

    for (tmp = input; *tmp; tmp++)
        if (!strchr (SUPERSAFE, *tmp))
            break;
    if (!*tmp)
    {
        s_cat (&t, color);
        s_cat (&t, input);
        s_cat (&t, COLNONE);
        return t.txt;
    }
    
    s_catc (&t, '\"');
    s_cat (&t, color);
    for (tmp = input; *tmp; tmp++)
    {
        if (strchr ("\\\"", *tmp))
        {
            s_catc (&t, '\\');
            s_catc (&t, *tmp);
        }
        else if (*tmp & 0xe0)
            s_catc (&t, *tmp);
        else
        {
            s_catc (&t, '\\');
            s_catc (&t, 'x');
            s_catc (&t, (*tmp / 16) <= 9 ? ((UBYTE)*tmp / 16) + '0'
                                     : ((UBYTE)*tmp / 16) - 10 + 'a');
            s_catc (&t, (*tmp & 15) <= 9 ? (*tmp & 15) + '0'
                                     : (*tmp & 15) - 10 + 'a');
        }
    }
    s_cat (&t, COLNONE);
    s_catc (&t, '\"');
    return t.txt;
}

/*
 * Quote a string for message display. Uses quotes only for the empty string.
 */
const char *s_mquote (const char *input, const char *color, BOOL allownl)
{
    static str_s t;
    const char *tmp;
    
    if (!input || !*input)
        return "\"\"";
    s_init (&t, "", 100);

    for (tmp = input; *tmp; tmp++)
        if (!strchr (SUPERSAFE, *tmp))
            break;
    if (!*tmp)
    {
        s_cat (&t, color);
        s_cat (&t, input);
        s_cat (&t, COLNONE);
        return t.txt;
    }
    
    s_cat (&t, color);
    for (tmp = input; *tmp; tmp++)
    {
        if (*tmp == '\n' && !tmp[1])
            ;
        else if (*tmp & 0xe0 || (*tmp == '\n' && allownl))
            s_catc (&t, *tmp);
        else if (*tmp != '\r' || !allownl || tmp[1] != '\n')
        {
            s_catf (&t, COLINVCHAR);
            s_catc (&t, *tmp - 1 + 'A');
            s_catf (&t, color);
        }
    }
    s_cat (&t, COLNONE);
    return t.txt;
}

