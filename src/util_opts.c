/*
 * Option handling within mICQ.
 *
 * mICQ Copyright (C) © 2003 Rüdiger Kuhlmann
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
#include <assert.h>
#include <stdarg.h>
#include "util_opts.h"
#include "util_parse.h"
#include "conv.h"
#include "util_ui.h"
#include "preferences.h"

static char **strtable = NULL;
static int strmax = 0;
static int struse = 0;
static int strmin = 0;

typedef struct OptTable_s COT;

struct OptEntry_s OptList[] = {
  { "intimate",      CO_INTIMATE      },
  { "hidefrom",      CO_HIDEFROM      },
  { "ignore",        CO_IGNORE        },
  { "logonoff",      CO_LOGONOFF      },
  { "logchange",     CO_LOGCHANGE     },
  { "logmess",       CO_LOGMESS       },
  { "showonoff",     CO_SHOWONOFF     },
  { "showchange",    CO_SHOWCHANGE    },
  { "hideack",       CO_HIDEACK       },
  { "wantsbl",       CO_WANTSBL       },
  { "obeysbl",       CO_OBEYSBL       },
  { "shadow",        CO_SHADOW        },
  { "local",         CO_LOCAL         },
  { "webaware",      CO_WEBAWARE      },
  { "hideip",        CO_HIDEIP        },
  { "dcauth",        CO_DCAUTH        },
  { "dccont",        CO_DCCONT        },
  { "autoaway",      CO_AUTOAWAY      },
  { "autona",        CO_AUTONA        },
  { "autoocc",       CO_AUTOOCC       },
  { "autodnd",       CO_AUTODND       },
  { "autoffc",       CO_AUTOFFC       },
  { "colornone",     CO_COLORNONE     },
  { "colorserver",   CO_COLORSERVER   },
  { "colorclient",   CO_COLORCLIENT   },
  { "colorerror",    CO_COLORERROR    },
  { "colordebug",    CO_COLORDEBUG    },
  { "colorquote",    CO_COLORQUOTE    },
  { "colorinvchar",  CO_COLORINVCHAR  },
  { "colormessage",  CO_COLORMESSAGE  },
  { "colorincoming", CO_COLORINCOMING },
  { "colorsent",     CO_COLORSENT     },
  { "colorack",      CO_COLORACK      },
  { "colorcontact",  CO_COLORCONTACT  },
  { "encoding",      CO_ENCODINGSTR   }, /* not CO_ENCODING */
  { "colorscheme",   CO_CSCHEME       },
  { "tabspool",      CO_TABSPOOL      },
  { "timeseen",      CO_TIMESEEN      },
  { "timeonline",    CO_TIMEONLINE    },
  { "timemicq",      CO_TIMEMICQ      },
  { NULL, 0 }
};

/*
 * Create new contact options
 */
Opt *OptC (void)
{
    return calloc (sizeof (Opt), 1);
}

/*
 * Delete contact options
 */
void OptD (Opt *opt)
{
    Opt *cot;
    UDWORD flag;
    val_t val;
    int i;

    if (!opt)
        return;
    for (i = 0; OptList[i].name; i++)
    {
        if (~(flag = OptList[i].flag) & (COF_STRING | COF_COLOR))
            continue;
        if ((val = OptUndef (opt, flag)))
        {
            free (strtable[val]);
            strtable[val] = NULL;
            if (val + 1 == struse)
                while (!strtable[val])
                    val--, struse--;
            if (val < strmin)
                strmin = val;
        }
    }
    while (opt)
    {
        cot = opt->next;
        free (opt);
        opt = cot;
    }
}

/*
 * Import options from a string.
 */
int OptImport (Opt *opts, const char *args)
{
    strc_t par;
    char *argst;
    const char *argstt;
    UDWORD flag = 0;
    int i, ret = 0;
    
    argst = strdup (args);
    argstt = argst;
    
    while ((par = s_parse (&argstt)))
    {
        for (i = 0; OptList[i].name; i++)
            if (!strcmp (par->txt, OptList[i].name))
            {
                flag = OptList[i].flag;
                break;
            }
        
        if (!OptList[i].name)
        {
            ret = 1;
            break;
        }
        
        if (!(par = s_parse (&argstt)))
        {
            ret = 1;
            break;
        }
        
        if (flag & COF_COLOR)
        {
            char *color = strdup (par->txt);
            OptSetStr (opts, flag, OptC2S (color));
            OptUndef  (opts, CO_CSCHEME);
            free (color);
        }
        else if (flag == CO_ENCODINGSTR)
        {
            UWORD enc = ConvEnc (par->txt) & ~ENC_FAUTO;
            OptSetVal (opts, CO_ENCODING, enc);
            OptSetStr (opts, CO_ENCODINGSTR, ConvEncName (enc));
        }
        else if (flag & COF_NUMERIC)
        {
            val_t val = atoll (par->txt);
            
            if (flag == CO_CSCHEME)
                OptImport (opts, PrefSetColorScheme (val));
            
            OptSetVal (opts, flag, val);
        }
        else if (~flag & COF_BOOL)
            OptSetStr (opts, flag, par->txt);
        else if (!strcasecmp (par->txt, "on")  || !strcasecmp (par->txt, i18n (1085, "on")))
            OptSetVal (opts, flag, 1);
        else if (!strcasecmp (par->txt, "off") || !strcasecmp (par->txt, i18n (1086, "off")))
            OptSetVal (opts, flag, 0);
        else if (!strcasecmp (par->txt, "undef"))
            OptUndef (opts, flag);
        else
        {
            ret = 1;
            break;
        }
    }
    free (argst);
    return ret;
}

/*
 * Export options into a string.
 */
const char *OptString (const Opt *opts)
{
    static str_s str;
    int i, flag;
    val_t val;
    
    s_init (&str, "", 100);
    
    for (i = 0; OptList[i].name; i++)
        if (OptGetVal (opts, flag = OptList[i].flag, &val))
        {
            if (flag & COF_BOOL)
            {
                if (!*str.txt)
                    s_cat (&str, "options");
                s_catf (&str, " %s %s", OptList[i].name, val ? "on" : "off");
            }
            else
            {
                if (*str.txt)
                    s_catc (&str, '\n');
                if (flag & COF_NUMERIC)
                    s_catf (&str, "options %s %lu", OptList[i].name, val);
                else if (flag & COF_COLOR)
                    s_catf (&str, "options %s %s", OptList[i].name, s_quote (OptS2C (strtable[val])));
                else
                    s_catf (&str, "options %s %s", OptList[i].name, s_quote (strtable[val]));
            }
        }
    if (*str.txt)
        s_catc (&str, '\n');

    return str.txt;
}

/*
 * Get a (string) contact option.
 */
#undef OptGetStr
BOOL OptGetStr (const Opt *opt, UDWORD flag, const char **res DEBUGPARAM)
{
    val_t val;

    assert (flag & (COF_STRING | COF_COLOR));

    if (!OptGetVal (opt, flag, &val))
        return FALSE;

    if (val >= strmax)
        return FALSE;

    *res = strtable[val];
    if (~flag & 0x80)
        Debug (DEB_OPTS, "(%p,%lx) = %s", opt, flag, s_quote (*res));
    return TRUE;
}

/*
 * Set several contact options at once
 */
Opt *OptSetVals (Opt *opt, UDWORD flag, ...)
{
    const char *text;
    va_list args;
    val_t val;
    
    va_start (args, flag);
    if (!opt)
        opt = OptC ();
    while (flag)
    {
        if (flag & COF_STRING)
        {
            text = va_arg (args, const char *);
            OptSetStr (opt, flag, text);
        }
        else
        {
            val = va_arg (args, val_t);
            OptSetVal (opt, flag, val);
        }
        flag = va_arg (args, UDWORD);
    }
    va_end (args);
    return opt;
}

/*
 * Set a (string) contact option.
 */
#undef OptSetStr
BOOL OptSetStr (Opt *opt, UDWORD flag, const char *text DEBUGPARAM)
{
    val_t val;

    assert (flag & (COF_STRING | COF_COLOR));
    
    if ((val = OptUndef (opt, flag)))
    {
        free (strtable[val]);
        strtable[val] = NULL;
        if (val < strmin)
            strmin = val;
    }
    if (!text)
        return TRUE;

    val = strmin;
    while (val < strmax && strtable[val])
       val++;
    strmin = val + 1;
    
    if (val == strmax)
    {
        int j, news = (strmax ? strmax * 2 : 128);
        char **new = realloc (strtable, sizeof (char *) * news);
        if (!new)
            return FALSE;
        for (j = strmax; j < news; j++)
            new[j] = NULL;
        strtable = new;
        strmax = news;
    }
    if (!(strtable[val] = strdup (text)))
        return FALSE;
    
    Debug (DEB_OPTS, "(%p,%lx) := %ld / %s", opt, flag, val, s_quote (strtable [val]));

    return OptSetVal (opt, flag, val);
}

/*
 * Get a contact option.
 */
#undef OptGetVal
BOOL OptGetVal (const Opt *opt, UDWORD flag, val_t *res DEBUGPARAM)
{
    const Opt *cot;
    UBYTE tag = flag & 0xff;
    int k;

    for (cot = opt; cot; cot = cot->next)
    {
        for (k = 0; k < OPT_TABLESIZE; k++)
            if (cot->tags[k] == tag)
                break;
        if (k != OPT_TABLESIZE)
            break;
    }
    if (!cot || ((flag & COF_BOOL) && (~cot->vals[k] & (flag & CO_BOOLMASK))))
    {
        Debug (DEB_OPTS, "(%p,%lx) undef", opt, flag);
        return FALSE;
    }
    *res = (flag & COF_BOOL) ? (cot->vals[k] & (flag * 2) & CO_BOOLMASK) != 0 : cot->vals[k];
    if (~tag & 0x80)
        Debug (DEB_OPTS, "(%p,%lx) = %lx = %lu", opt, flag, *res, *res);
    return TRUE;
}

/*
 * Set a contact option.
 */
#undef OptSetVal
BOOL OptSetVal (Opt *opt, UDWORD flag, val_t val DEBUGPARAM)
{
    Opt *cot, *cotold;
    int k;
    UBYTE tag = flag & 0xff;

    cotold = NULL;
    for (cot = opt; cot; cot = cot->next)
    {
        cotold = cot;
        for (k = 0; k < OPT_TABLESIZE; k++)
            if ((cot->tags[k] == tag) || !cot->tags[k])
                break;
        if (k != OPT_TABLESIZE)
            break;
    }
    if (!cot)
    {
        if (!(cot = calloc (sizeof (Opt), 1)))
        {
            Debug (DEB_OPTS, "(%p,%lx) != %lx = %lu <mem %p>", opt, flag, val, val, cot);
            return FALSE;
        }

        Debug (DEB_OPTS, "(%p,%lx) := %lx = %lu <new %p>", opt, flag, val, val, cot);
        cotold->next = cot;
        k = 0;
    }
    else
        Debug (DEB_OPTS, "(%p,%lx) := %lx = %lu <%p>", opt, flag, val, val, cot);

    cot->tags[k] = tag;
    if (flag & COF_BOOL)
    {
        cot->vals[k] |= flag & CO_BOOLMASK;
        if (val)
            cot->vals[k] |= (flag & CO_BOOLMASK) * 2;
        else
            cot->vals[k] &= ~((flag & CO_BOOLMASK) * 2);
    }
    else
        cot->vals[k] = val;
    return TRUE;
}

/*
 * Undefine a contact option.
 */
#undef OptUndef
val_t OptUndef (Opt *opt, UDWORD flag DEBUGPARAM)
{
    Opt *cot, *cotold;
    UBYTE tag = flag & 0xff;
    val_t old = 0;
    int k, m;


    for (cot = opt; cot; cot = cot->next)
    {
        for (k = 0; k < OPT_TABLESIZE; k++)
            if (cot->tags[k] == tag)
                break;
        if (k != OPT_TABLESIZE)
            break;
    }
    if (!cot)
    {
        Debug (DEB_OPTS, "(%p,%lx) := undef <unset>", opt, flag);
        return 0;
    }
    if (flag & COF_BOOL)
    {
        cot->vals[k] &= ~(flag & CO_BOOLMASK);
        cot->vals[k] &= ~((flag & CO_BOOLMASK) * 2);
        Debug (DEB_OPTS, "(%p,%lx) := undef <bit>", opt, flag);
        if (cot->vals[k])
            return 0;
    }
    for (cotold = cot; cotold->next; )
        cotold = cotold->next;
    for (m = OPT_TABLESIZE - 1; m >= 0; m--)
        if (cotold->tags[m])
            break;
    old = cot->vals[k];
    cot->tags[k] = cotold->tags[m];
    cot->vals[k] = cotold->vals[m];
    cotold->tags[m] = 0;
    cotold->vals[m] = 0;
    Debug (DEB_OPTS, "(%p,%lx) := undef <%ld>", opt, flag, old);

    return old;
}

/*
 * Convert a string describing a color into an escape sequence.
 */
const char *OptC2S (const char *color)
{
    static str_s str;
    strc_t par;
    const char *cmd, *c;

    s_init (&str, "", 10);

    while ((par = s_parse (&color)))
    {
        cmd = par->txt;
        if      (!strcasecmp (cmd, "black"))   c = BLACK;
        else if (!strcasecmp (cmd, "red"))     c = RED;
        else if (!strcasecmp (cmd, "green"))   c = GREEN;
        else if (!strcasecmp (cmd, "yellow"))  c = YELLOW;
        else if (!strcasecmp (cmd, "blue"))    c = BLUE;
        else if (!strcasecmp (cmd, "magenta")) c = MAGENTA;
        else if (!strcasecmp (cmd, "cyan"))    c = CYAN;
        else if (!strcasecmp (cmd, "white"))   c = WHITE;
        else if (!strcasecmp (cmd, "none"))    c = SGR0;
        else if (!strcasecmp (cmd, "bold"))    c = BOLD;
        else c = cmd;
        
        s_cat (&str, c);
    }
    return str.txt;
}

/*
 * Convert an escape sequence into a description of the color it selects
 */
const char *OptS2C (const char *text)
{
    static str_s str;
    const char *c;
    int l;
    
    s_init (&str, "", 20);

    for ( ; *text; text += l)
    {
        if      (!strncmp (BLACK,   text, l = strlen (BLACK)))   c = "black";
        else if (!strncmp (RED,     text, l = strlen (RED)))     c = "red";
        else if (!strncmp (BLUE,    text, l = strlen (BLUE)))    c = "blue";
        else if (!strncmp (GREEN,   text, l = strlen (GREEN)))   c = "green";
        else if (!strncmp (YELLOW,  text, l = strlen (YELLOW)))  c = "yellow";
        else if (!strncmp (MAGENTA, text, l = strlen (MAGENTA))) c = "magenta";
        else if (!strncmp (CYAN,    text, l = strlen (CYAN)))    c = "cyan";
        else if (!strncmp (WHITE,   text, l = strlen (WHITE)))   c = "white";
        else if (!strncmp (SGR0,    text, l = strlen (SGR0)))    c = "none";
        else if (!strncmp (BOLD,    text, l = strlen (BOLD)))    c = "bold";
        else (c = text), (l = strlen (text));

        if (*str.txt)
            s_catc (&str, ' ');
        s_cat (&str, s_quote (c));
    }
    return str.txt;
}

