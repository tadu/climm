/*
 * This file contains string parsing functions.
 *
 * All functions advance the input after the successfully parsed token, or
 * to the next non-delimiter if no token could be successfully parsed.
 * All functions return pointer to static data which may not be freed.
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
 * In addition, as a special exception permission is granted to link the
 * code of this release of mICQ with the OpenSSL project's "OpenSSL"
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

#include "micq.h"
#include "util_str.h"
#include "util_parse.h"
#include "contact.h"
#include "connection.h"

/*
 * Parses the next word, respecting double quotes, and interpreting
 * \x##, \# and comments (#).
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
 * Parses a nick, accepting numeric UINs as well.
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
    
    if ((t = s_parse_s (&p, sep)) && strncmp (t->txt, *input, t->len))
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
 * Parses a contact group by name.
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
            if (cg->serv == serv && cg->name && !strcmp (cg->name, t->txt))
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
 * Parses nicks and contact groups.
 */
ContactGroup *s_parselist_s (const char **input, BOOL rem, Connection *serv)
{
    static ContactGroup *scg = NULL;
    ContactGroup *cg;
    Contact *cont;
    const char *p = *input;
    strc_t par;
    UDWORD i, one = 0;
    
    if (scg)
        ContactGroupD (scg);
    scg = ContactGroupC (NULL, 0, "");
    scg->temp = 1; /* ContactD will not compact contacts on temporary lists! */
    while (*p)
    {
        while (*p && strchr (DEFAULT_SEP, *p))
            p++;
        if ((cg = s_parsecg_s (&p, MULTI_SEP, serv)))
        {
            for (i = 0; (cont = ContactIndex (cg, i)); i++)
                if (!ContactHas (scg, cont))
                    ContactAdd (scg, cont);
        }
        else if ((cont = s_parsenick_s (&p, MULTI_SEP, serv)))
        {
            if (!ContactHas (scg, cont))
                ContactAdd (scg, cont);
        }
        else if ((par = s_parse (&p)))
        {
            rl_printf (i18n (2523, "%s not recognized as a nick name.\n"), s_wordquote (par->txt));
            if (!rem)
                break;
            one = 1;
            continue;
        }
        else
            break;
        if (!rem && *p != ',')
            break;
        if (*p)
            p++;
    }
    
    if ((one || ContactIndex (scg, 0)) && (rem || strchr (DEFAULT_SEP, *p)))
    {
        *input = p;
        return scg;
    }
    return NULL;
}

/*
 * Parses the remainder of the input, translates \x## and \#.
 */
char *s_parserem_s (const char **input, const char *sep)
{
    static char *t = NULL;
    const char *p = *input;
    
    while (*p && strchr (sep, *p))
        p++;
    *input = p;
    if (!*p)
        return NULL;

    s_repl (&t, p);
    while (*p)
        p++;
    return t;
}

/*
 * Parses a number.
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
 * Parses a keyword.
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
    
    if (*p && !strchr (sep, *p))
        return FALSE;
    
    *input = p;
    return TRUE;
}

