/*
 * Alias handling code.
 *
 * mICQ Copyright (C) © 2001-2005 Rüdiger Kuhlmann
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
#include "util_alias.h"
#include "contact.h"
#include "preferences.h"

#include <stdarg.h>
#include <ctype.h>
#include <signal.h>
#include <assert.h>

typedef struct al_autoexpandv_s {
  struct al_autoexpandv_s *next;
  char *command;
  char *replace;
  char autoexpand;
} aliasv_t;
      
static aliasv_t *al_ae = NULL;

const alias_t *AliasList (void)
{
    return (const alias_t *) al_ae;
}

void AliasAdd (const char *command, const char *replace, char autoexpand)
{
    aliasv_t *e;
    
    for (e = al_ae; e; e = e->next)
    {
        if (!strcasecmp (command, e->command))
        {
            s_repl (&e->command, command);
            s_repl (&e->replace, replace);
            e->autoexpand = autoexpand;
            return;
        }
    }
    e = malloc (sizeof (aliasv_t));
    e->next = al_ae;
    al_ae = e;
    e->command = strdup (command);
    e->replace = strdup (replace);
    e->autoexpand = autoexpand;
}

char AliasRemove (const char *command)
{
    aliasv_t **e, *t;
    
    if (!al_ae)
        return FALSE;
    
    for (e = &al_ae; *e; e = &((*e)->next))
    {
        if (!strcasecmp (command, (*e)->command))
        {
            s_free ((*e)->command);
            s_free ((*e)->replace);
            t = *e;
            *e = (*e)->next;
            free (t);
            return TRUE;
        }
    }
    return FALSE;
}

const char *al_recurse (const char *string, char autoexpand)
{
    static int level = 0;
    const char *ret;
    char *tmp;

    if (level > 10)
        return string;
    level++;
    tmp = strdup (string);
    ret = AliasExpand (tmp, 0, autoexpand);
    ret = ret ? ret : string;
    free (tmp);
    level--;
    return ret;
}

const char *AliasExpand (const char *string, UDWORD bytepos, char autoexpand)
{
    aliasv_t *e;
    static str_s al_exp = { NULL, 0, 0 };
    char *p, *q;
    char found_s = 0;
    
    if (!bytepos)
    {
        if ((p = strchr (string, ' ')))
            bytepos = p - string;
        else
            bytepos = strlen (string);
    }
    
    for (e = al_ae; e; e = e->next)
        if ((!autoexpand || e->autoexpand) && strlen (e->command) == bytepos
            && !strncmp (string, e->command, bytepos))
            break;

    if (!e)
        return NULL;
    
    s_init (&al_exp, "", 0);
    for (p = e->replace; *p && (q = strchr (p, '%')); p = q)
    {
        s_catn (&al_exp, p, q - p);
        if (q[1] == 'r')
        {
            if (uiG.last_rcvd)
                s_cat (&al_exp, uiG.last_rcvd->nick);
            q += 2;
        }
        else if (q[1] == 'a')
        {
            if (uiG.last_sent)
                s_cat (&al_exp, uiG.last_sent->nick);
            q += 2;
        }
        else if (q[1] == 's')
        {
            s_cat (&al_exp, string + bytepos);
            found_s = 1;
            q += 2;
        }
        else
            q++;
    }
    s_cat (&al_exp, p);
    if (!found_s && string[bytepos])
    {
        if (string[bytepos] != ' ')
            s_catc (&al_exp, ' ');
        s_cat (&al_exp, string + bytepos);
    }
    return al_recurse (al_exp.txt, autoexpand);
}
