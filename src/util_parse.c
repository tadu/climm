/*
 * This file contains string parsing functions.
 *
 * All functions advance the input after the successfully parsed token, or
 * to the next non-delimiter if no token could be successfully parsed.
 * All functions return pointer to static data which may not be freed.
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

int is_valid_icq_name (char *t)
{
    if (!*t)
        return 0;
    for ( ; *t; t++)
        if (*t < '0' || *t > '9')
            return 0;
    return 1;
}

int is_valid_aim_name (char *t)
{
    if (!*t)
        return 0;
    if (*t >= '0' && *t <= '9')
        return 0;
    for ( ; *t; t++)
    {
        if ((*t < '0' || *t > '9') && (*t < 'a' || *t > 'z') && (*t < 'A' || *t > 'Z'))
            return 0;
        if (*t >= 'A' && *t <= 'Z')
            t += 'a' - 'A';
    }
    return 1;
}

int is_valid_xmpp_name (char *txt)
{
    char *at, *slash;
    
    if (!*txt)
        return 0;
    if (!(at = strchr (txt, '@')))
        return 0;
    if (strchr (at + 1, '@'))
        return 0;
    slash = strchr (txt, '/');
    if (slash && slash != strchr (at, '/'))
        return 0;
    return 1;
}

int is_valid_msn_name (char *txt)
{
    char *at;
    
    if (!*txt)
        return 0;
    if (!(at = strchr (txt, '@')))
        return 0;
    if (strchr (at + 1, '@'))
        return 0;
    return 1;
}

/*
 * Parses a nick, UIN or screen name.
 */
Contact *s_parsenick_s (const char **input, const char *sep, BOOL any, Server *serv)
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
        if ((parsed = ContactFind (serv, t->txt)))
        {
            *input = p;
            return parsed;
        }
    }
    p = *input;

    if (serv->type == TYPE_SERVER && !strncasecmp (p, "AIM:", 4))
    {
        p += 4;
        t = s_parse (&p);
        if (t && is_valid_aim_name (t->txt))
        {
            *input = p;
            return ContactScreen (serv, t->txt);
        }
        p = *input;
    }

    if (serv->type == TYPE_MSN_SERVER && !strncasecmp (p, "MSN:", 4))
    {
        p += 4;
        t = s_parse (&p);
        if (t && is_valid_msn_name (t->txt))
        {
            *input = p;
            return ContactScreen (serv, t->txt);
        }
        p = *input;
    }

    if (serv->type == TYPE_XMPP_SERVER &&
        (!strncasecmp (p, "JABBER:", 7) || !strncasecmp (p, "XMPP:", 5)))
    {
        p += (*p == 'J') ? 7 : 5;
        t = s_parse (&p);
        if (t && is_valid_xmpp_name (t->txt))
        {
            *input = p;
            return ContactScreen (serv, t->txt);
        }
        p = *input;
    }

    if (serv->type == TYPE_SERVER && !strncasecmp (p, "ICQ:", 4))
    {
        t = s_parse (&p);
        if (t && is_valid_icq_name (t->txt))
        {
            *input = p;
            return ContactScreen (serv, t->txt);
        }
        p = *input;
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

    t = s_parse (&p);
    if (!t)
        return NULL;

    if (serv->type == TYPE_MSN_SERVER && is_valid_msn_name (t->txt))
    {
        *input = p;
        return ContactScreen (serv, t->txt);
    }

    if (serv->type == TYPE_XMPP_SERVER && is_valid_xmpp_name (t->txt))
    {
        *input = p;
        return ContactScreen (serv, t->txt);
    }

    if (serv->type == TYPE_SERVER && is_valid_icq_name (t->txt))
    {
        *input = p;
        return ContactScreen (serv, t->txt);
    }
    
    if (!any)
        return NULL;

    for (i = 0; (serv = Connection2Server (ConnectionNr (i))); i++)
        if ((r = s_parsenick_s (input, sep, 0, serv)))
            return r;

    return NULL;
}

/*
 * Parses a contact group by name.
 */
ContactGroup *s_parsecg_s (const char **input, const char *sep, BOOL any, Server *serv)
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
    if (!any)
        return NULL;
    
    for (i = 0; (serv = Connection2Server (ConnectionNr (i))); i++)
        if ((cg = s_parsecg_s (input, sep, 0, serv)))
            return cg;
    
    return NULL;
}

/*
 * Parses nicks and contact groups.
 */
ContactGroup *s_parselist_s (const char **input, BOOL rem, BOOL any, Server *serv)
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
        if ((cg = s_parsecg_s (&p, MULTI_SEP, 0, serv)))
        {
            for (i = 0; (cont = ContactIndex (cg, i)); i++)
                if (!ContactHas (scg, cont))
                    ContactAdd (scg, cont);
        }
        else if ((cont = s_parsenick_s (&p, MULTI_SEP, 0, serv)))
        {
            if (!ContactHas (scg, cont))
                ContactAdd (scg, cont);
        }
        else if (any && (cg = s_parsecg_s (&p, MULTI_SEP, 1, serv)))
        {
            for (i = 0; (cont = ContactIndex (cg, i)); i++)
                if (!ContactHas (scg, cont))
                    ContactAdd (scg, cont);
        }
        else if (any && (cont = s_parsenick_s (&p, MULTI_SEP, 1, serv)))
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

