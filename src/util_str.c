/*
 * This file contains static string helper functions.
 *
 * climm Copyright (C) © 2001-2007 Rüdiger Kuhlmann
 *
 * climm is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 dated June, 1991.
 *
 * climm is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * In addition, as a special exception permission is granted to link the
 * code of this release of climm with the OpenSSL project's "OpenSSL"
 * library, and distribute the linked executables.  You must obey the GNU
 * General Public License in all respects for all of the code used other
 * than "OpenSSL".  If you modify this file, you may extend this exception
 * to your version of the file, but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your version
 * of this file.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * $Id$
 */

#include "climm.h"
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
#include <ctype.h>
#include <assert.h>
#include "util_str.h"
#include "conv.h"
#include "contact.h"
#include "preferences.h"

/*
 * Initialize a dynamically allocated string.
 *
 * Returns NULL if not enough memory.
 */
str_t s_init (str_t str, const char *init, size_t add)
{
    UDWORD initlen, size;
    
    initlen = strlen (init);
    size = ((initlen + add) | 0x7f) + 1;
    
    if (!str->txt || size > str->max)
    {
        if (str->txt && str->max)
            free (str->txt);
        str->txt = malloc (size);
        if (!str->txt)
        {
            str->max = str->len = 0;
            str->txt = "\0";
            return NULL;
        }
        str->max = size;
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
    int newlen;
    char *tmp;
    
    newlen = 1 + ((str->max + len - 1) | 0x7f);
    
    if (!str->txt || !str->max)
        tmp = malloc (newlen);
    else
        tmp = realloc (str->txt, newlen);
    
    if (!tmp)
    {
        if (!str->txt || !str->max)
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
    if (str->len + 2 > str->max)
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
    if (str->len + len + 2 > str->max)
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
    
    if (str->len + addlen + 2 > str->max)
        if (!s_blow (str, str->len + addlen + 2 - str->max))
            return NULL;
    
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
    size_t add = 128;
    int rc;
    
    if (str->max < add || str->len + 2 >= str->max)
        s_blow (str, add);

    while (1)
    {
        if (str->len + 2 >= str->max)
            return NULL;
        str->txt[str->max - 2] = '\0';
        va_start (args, fmt);
        rc = vsnprintf (str->txt + str->len, str->max - str->len - 1, fmt, args);
        va_end (args);
        if (rc >= 0 && rc < str->max - str->len - 2 && !str->txt[str->max - 2])
        {
            str->len += strlen (str->txt + str->len);
            return str;
        }
        add = (rc > 0 ? rc - str->max + str->len + 5 : str->max);
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
    if (pos > str->len || pos < 0)
        return NULL;
    if (str->len + len + 2 > str->max)
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
    if (pos > str->len || pos < 0)
        return NULL;
    if (str->len + 3 > str->max)
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
    if (pos > str->len || pos < 0)
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
const char *s_status (status_t status, UDWORD nativestatus)
{
    static char buf[200];
    
    if (status == ims_offline)
        return i18n (1969, "offline");
 
    if (ContactIsInv (status))
        snprintf (buf, sizeof (buf), "%s-", i18n (1975, "invisible"));
    else
        buf[0] = '\0';
    
    switch (ContactClearInv (status))
    {
        case imr_dnd:     snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), "%s", i18n (1971, "do not disturb")); break;
        case imr_occ:     snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), "%s", i18n (1973, "occupied"));       break;
        case imr_na:      snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), "%s", i18n (1974, "not available"));  break;
        case imr_away:    snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), "%s", i18n (1972, "away"));           break;
        case imr_ffc:     snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), "%s", i18n (1976, "free for chat"));  break;
        case imr_offline: assert (0);
        case imr_online:
            if (buf[0])
                buf[strlen (buf) - 1] = '\0';
            else
                snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), "%s", i18n (1970, "online"));
    }

    if (prG->verbose)
        snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), " %08lx", nativestatus);
    
    return buf;
}

/*
 * Return a static string short describing the status.
 */
const char *s_status_short (status_t status)
{
    static char buf[20];
    
    if (status == ims_offline)
        return i18n (2621, "off");
    if (status == ims_inv)
        return i18n (2622, "inv");
 
    if (ContactIsInv (status))
        snprintf (buf, sizeof (buf), "%s-", i18n (2622, "inv"));
    else
        buf[0] = '\0';
    
    switch (ContactClearInv (status))
    {
        case imr_dnd:     snprintf (buf + strlen (buf), sizeof (buf) - strlen (buf), "%s", i18n (2623, "dnd"));  break;
        case imr_occ:     snprintf (buf + strlen (buf), sizeof (buf) - strlen (buf), "%s", i18n (2624, "occ"));  break;
        case imr_na:      snprintf (buf + strlen (buf), sizeof (buf) - strlen (buf), "%s", i18n (2625, "na"));   break;
        case imr_away:    snprintf (buf + strlen (buf), sizeof (buf) - strlen (buf), "%s", i18n (2626, "away")); break;
        case imr_ffc:     snprintf (buf + strlen (buf), sizeof (buf) - strlen (buf), "%s", i18n (2627, "ffc"));  break;
        case imr_offline: assert (0);
        case imr_online:  snprintf (buf + strlen (buf), sizeof (buf) - strlen (buf), "%s", i18n (2628, "online"));
    }
    return buf;
}

/*
 * Returns static string describing the given time.
 */
const char *s_time (time_t *stamp)
{
    static str_s str = { NULL, 0, 0 };
    struct timeval p = {0L, 0L};
    struct tm now;
    struct tm *thetime;
    time_t nowsec;
    size_t rc = 0;

    if (stamp && *stamp != NOW)
    {
        nowsec = time (NULL);
        now = *localtime (&nowsec);
        thetime = localtime (stamp);
    }
    else
    {
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
        thetime = &now;
        stamp = NULL;
        now = *localtime (&nowsec);
    }

    s_init (&str, "", 32);    
    while (!rc)
    {
        rc = strftime (str.txt, str.max,
                    thetime->tm_year == now.tm_year 
                 && thetime->tm_mon  == now.tm_mon
                 && thetime->tm_mday == now.tm_mday
                 ? "%H:%M:%S" : "%a %b %d %H:%M:%S %Y", thetime); /*"*/
        if (rc <= 0 || rc >= str.max)
        {
            rc = 0;
            s_blow (&str, 32);
        }
        else
        {
            str.txt[str.max - 1] = 0;
            str.len = strlen (str.txt);
        }
    }
    if (prG->verbose > 7)
        s_catf (&str, ".%.06ld", p.tv_usec);
    else if (prG->verbose > 1)
        s_catf (&str, ".%.03ld", p.tv_usec / 1000);
    
    return ConvFrom (&str, prG->enc_loc)->txt;
}

/*
 * Returns static string describing the given time in strftime format.
 */
const char *s_strftime (time_t *stamp, const char *fmt, char as_gmt)
{
    static str_s str = { NULL, 0, 0 };
    struct tm *thetime;
    size_t rc = 0;
    char *dotfmt;
    
    /* strftime()'s error reporting is incomplete, so make sure a correct result is never empty */
    dotfmt = malloc (strlen (fmt) + 2);
    strcpy (dotfmt, fmt);
    strcat (dotfmt, ".");

    if (stamp && *stamp != NOW)
        thetime = as_gmt ? gmtime (stamp) : localtime (stamp);
    else
    {
        time_t nowsec = time (NULL);
        thetime = as_gmt ? gmtime (&nowsec) : localtime (&nowsec);
    }
    
    s_init (&str, "", 32);
    
    while (!rc)
    {
        rc = strftime (str.txt, str.max, dotfmt, thetime);
        if (rc <= 0 || rc >= str.max)
        {
            rc = 0;
            s_blow (&str, 32);
        }
        else
        {
            str.txt[str.max - 1] = 0;
            str.len = strlen (str.txt);
            if (str.len && str.txt [str.len - 1] == '.')
            {
                str.txt [str.len - 1] = 0;
                str.len--;
            }
        }
    }
    s_free (dotfmt);
    
    return ConvFrom (&str, prG->enc_loc)->txt;
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
    strc_t tr;
    int off, offt, offold, offnl, offin, offmax;
    UDWORD ucs;
    
    in.txt = (char *) *input;
    in.len = strlen (*input);
    off = offt = offnl = offin = offmax = 0;
    
    while (in.txt[off])
    {
        offold = off;
        ucs = ConvGetUTF8 (&in, &off);
        tr = ConvToLen (in.txt + offold, enc, off - offold);
        if (ucs == '\r')
            offnl = offold;
        if (!(ucs & ~0xff) && isspace (ucs & 0xff))
            offin = offold;
        if (tr->len > len)
        {
            off = offold;
            break;
        }
        if (ucs == '\n')
            offnl = off;
        if (!iswalnum (ucs))
            offin = off;
        len -= tr->len;
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
    return &str;
}

/*
 * Replace all occurrences of a substring by another
 */
void s_strrepl (str_t str, const char *old, const char *news)
{
    char *e;
    size_t d = 0, olen = strlen (old), nlen = strlen (news);
    
    if (str->len >= str->max)
        s_blow (str, 1);
    str->txt[str->len] = 0;
    while ((e = strstr (str->txt + d, old)))
    {
        if (str->len + nlen - olen + 1 >= str->max)
            s_blow (str, nlen - olen);
        d = e - str->txt;
        memmove (e + nlen, e + olen, str->len - d - olen + 1);
        memmove (e, news, nlen);
        str->len += nlen - olen;
        if (nlen >= olen)
            d++; /* prevent endless loops */
    }
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
        s_catc (&t, '\"');
        s_cat (&t, noctl  (d[0]));  s_cat (&t, noctl  (d[1]));
        s_cat (&t, noctl  (d[2]));  s_cat (&t, noctl  (d[3]));
        s_cat (&t, noctl  (d[4]));  s_cat (&t, noctl  (d[5]));
        s_cat (&t, noctl  (d[6]));  s_cat (&t, noctl  (d[7]));
        s_cat (&t, " ");
        s_cat (&t, noctl  (d[8]));  s_cat (&t, noctl  (d[9]));
        s_cat (&t, noctl (d[10]));  s_cat (&t, noctl (d[11]));
        s_cat (&t, noctl (d[12]));  s_cat (&t, noctl (d[13]));
        s_cat (&t, noctl (d[14]));  s_cat (&t, noctl (d[15]));
        s_catc (&t, '"');
        s_catc (&t, '\n');
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
 * Quote a string for display. May use quotes to make sure text boundary is clear.
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
            s_cat  (&t, COLINVCHAR);
            s_catc (&t, *tmp - 1 + 'A');
            s_cat  (&t, color);
        }
    }
    s_cat (&t, COLNONE);
    return t.txt;
}

