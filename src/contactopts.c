
#include <assert.h>
#include <string.h>
#include "micq.h"
#include "conv.h"
#include "util_ui.h"
#include "preferences.h"
#include "contactopts.h"

static const char **strtable = NULL;
static int strmax = 0;

struct ContactOptionsTable_s
{
    UBYTE tags[16];
    UWORD vals[16];
};

struct ContactOption_s ContactOptionsList[] = {
  { "intimate",      CO_INTIMATE      },
  { "hidefrom",      CO_HIDEFROM      },
  { "ignore",        CO_IGNORE        },
  { "logonoff",      CO_LOGONOFF      },
  { "logchange",     CO_LOGCHANGE     },
  { "logmess",       CO_LOGMESS       },
  { "showonoff",     CO_SHOWONOFF     },
  { "showchange",    CO_SHOWCHANGE    },
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
  { NULL }
};

/*
 * Get a contact option.
 */
BOOL ContactOptionsGetVal (const ContactOptions *opt, UWORD flag, UWORD *res)
{
    UBYTE tag;

    if (flag & COF_DIRECT)
    {
        if (flag & CO_FLAGS & opt->set)
        {
            *res = (flag & CO_FLAGS & opt->val) ? 1 : 0;
            Debug (DEB_OPTS, "(%p,%x) = %d", opt, flag, *res);
            return TRUE;
        }
        Debug (DEB_OPTS, "(%p,%x) undef", opt, flag);
        return FALSE;
    }
    
    tag = flag & 0xff;
    
    if (opt->set & COF_DIRECT)
    {
        int j, k;
        for (j = k = 0; j < opt->co_un.co_indir.size; j++)
        {
            for (k = 0; k < 16; k++)
                if (opt->co_un.co_indir.table[j].tags[k] == tag)
                    break;
            if (k != 16)
                break;
        }
        if (j == opt->co_un.co_indir.size)
        {
            Debug (DEB_OPTS, "(%p,%x) undef", opt, tag);
            return FALSE;
        }

        *res = opt->co_un.co_indir.table[j].vals[k];
        Debug (DEB_OPTS, "(%p,%x) = %x = %d", opt, tag, *res, *res);
        return TRUE;
    }

    if (opt->co_un.co_dir.taga == tag)
    {
        *res = opt->co_un.co_dir.vala;
        Debug (DEB_OPTS, "(%p,%x) = %x = %d", opt, tag, *res, *res);
        return TRUE;
    }
    if (opt->co_un.co_dir.tagb == tag)
    {
        *res = opt->co_un.co_dir.valb;
        Debug (DEB_OPTS, "(%p,%x) = %x = %d", opt, tag, *res, *res);
        return TRUE;
    }
    Debug (DEB_OPTS, "(%p,%x) undef", opt, tag);
    return FALSE;
}

/*
 * Set a contact option.
 */
BOOL ContactOptionsSetVal (ContactOptions *opt, UWORD flag, UWORD val)
{
    int j, k;
    UBYTE tag;

    Debug (DEB_OPTS, "(%p,%x) := %x = %d", opt, flag, val, val);

    if (flag & COF_DIRECT)
    {
        flag &= CO_FLAGS;
        opt->set |= flag;
        if (val)
            opt->val |= flag;
        else
            opt->val &= ~flag;
        return TRUE;
    }
    
    tag = flag & 0xff;

    if ((~opt->set & COF_DIRECT) && (!opt->co_un.co_dir.taga || (opt->co_un.co_dir.taga == tag)))
    {
        opt->co_un.co_dir.taga = tag;
        opt->co_un.co_dir.vala = val;
        return TRUE;
    }
    if ((~opt->set & COF_DIRECT) && (!opt->co_un.co_dir.tagb || (opt->co_un.co_dir.tagb == tag)))
    {
        opt->co_un.co_dir.tagb = tag;
        opt->co_un.co_dir.valb = val;
        return TRUE;
    }
    if (~opt->set & COF_DIRECT)
    {
        struct ContactOptionsTable_s *new = calloc (sizeof (struct ContactOptionsTable_s), 1);
        if (!new)
            return FALSE;
        new->tags[0] = opt->co_un.co_dir.taga;
        new->tags[1] = opt->co_un.co_dir.tagb;
        new->vals[0] = opt->co_un.co_dir.vala;
        new->vals[1] = opt->co_un.co_dir.valb;
        opt->co_un.co_indir.size = 1;
        opt->co_un.co_indir.table = new;
        opt->set |= COF_DIRECT;
    }
    for (j = k = 0; j < opt->co_un.co_indir.size; j++)
    {
        for (k = 0; k < 16; k++)
            if ((opt->co_un.co_indir.table[j].tags[k] == tag) || !opt->co_un.co_indir.table[j].tags[k])
                break;
        if (k != 16)
            break;
    }
    if (j == opt->co_un.co_indir.size)
    {
        struct ContactOptionsTable_s *new = realloc (opt->co_un.co_indir.table, sizeof (struct ContactOptionsTable_s) * (j + 1));
        if (!new)
            return FALSE;
        opt->co_un.co_indir.size++;
        for (k = 0; k < 16; k++)
            new[j].tags[k] = new[j].vals[k] = 0;
        k = 0;
        opt->co_un.co_indir.table = new;
    }
    opt->co_un.co_indir.table[j].tags[k] = tag;
    opt->co_un.co_indir.table[j].vals[k] = val;
    return TRUE;
}

/*
 * Undefine a contact option.
 */
void ContactOptionsUndef (ContactOptions *opt, UWORD flag)
{
    UBYTE tag;

    Debug (DEB_OPTS, "(%p,%x) := undef", opt, flag);

    if (flag & COF_DIRECT)
    {
        flag &= ~COF_DIRECT;
        opt->set &= ~flag;
        opt->val &= ~flag;
        return;
    }

    tag = flag & 0xff;

    if (opt->set & COF_DIRECT)
    {
        int j, k, l, m;
        for (j = k = 0; j < opt->co_un.co_indir.size; j++)
        {
            for (k = 0; k < 16; k++)
                if (opt->co_un.co_indir.table[j].tags[k] == tag)
                    break;
            if (k != 16)
                break;
        }
        if (j == opt->co_un.co_indir.size)
            return;
        for (l = opt->co_un.co_indir.size - 1; l >= 0; l--)
            if (opt->co_un.co_indir.table[l].tags[0])
                break;
        for (m = 16 - 1; m >= 0; m--)
            if (opt->co_un.co_indir.table[l].tags[m])
                break;
        opt->co_un.co_indir.table[j].tags[k] = opt->co_un.co_indir.table[l].tags[m];
        opt->co_un.co_indir.table[j].vals[k] = opt->co_un.co_indir.table[l].vals[m];
        opt->co_un.co_indir.table[l].tags[m] = 0;
        opt->co_un.co_indir.table[l].vals[m] = 0;
    }
    else
    {
        if (opt->co_un.co_dir.taga == tag)
        {
            opt->co_un.co_dir.taga = 0;
            opt->co_un.co_dir.vala = 0;
        }
        if (opt->co_un.co_dir.tagb == tag)
        {
            opt->co_un.co_dir.tagb = 0;
            opt->co_un.co_dir.valb = 0;
        }
    }
}

/*
 * Get a (string) contact option.
 */
BOOL ContactOptionsGetStr (const ContactOptions *opt, UWORD flag, const char **res)
{
    UWORD val;

    assert (flag & (COF_STRING | COF_COLOR));

    if (!ContactOptionsGetVal (opt, flag, &val))
        return FALSE;

    if (val >= strmax)
        return FALSE;

    *res = strtable[val];
    Debug (DEB_OPTS, "(%p,%x) = %s", opt, flag, s_quote (*res));
    return TRUE;
}

/*
 * Set a (string) contact option.
 */
BOOL ContactOptionsSetStr (ContactOptions *opt, UWORD flag, const char *text)
{
    UWORD val;

    assert (flag & (COF_STRING | COF_COLOR));
    
    if (!text)
    {
        ContactOptionsUndef (opt, flag);
        return TRUE;
    }

    for (val = 0; val < strmax; val++)
        if (!strtable[val] || !strcmp (strtable[val], text))
            break;
    if (val == strmax)
    {
        int j, news = (strmax ? strmax * 2 : 128);
        const char **new = realloc (strtable, sizeof (char *) * news);
        if (!new)
            return FALSE;
        for (j = val; j < news; j++)
            new[j] = NULL;
        strtable = new;
        strmax = news;
    }
    if (!strtable[val])
        if (!(strtable[val] = strdup (text)))
            return FALSE;

    Debug (DEB_OPTS, "(%p,%x) := %d / %s", opt, flag, val, s_quote (strtable [val]));

    return ContactOptionsSetVal (opt, flag, val);
}

/*
 * Convert a string describing a color into an escape sequence.
 */
const char *ContactOptionsC2S (const char *color)
{
    static str_s str;
    strc_t par;
    const char *cmd, *c;

    s_init (&str, "", 10);

    while (s_parse (&color, &par))
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
const char *ContactOptionsS2C (const char *text)
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

/*
 * Export options into a string.
 */
const char *ContactOptionsString (const ContactOptions *opts)
{
    static str_s str;
    int i, flag;
    UWORD val;
    
    s_init (&str, "", 100);
    
    for (i = 0; ContactOptionsList[i].name; i++)
        if (ContactOptionsGetVal (opts, flag = ContactOptionsList[i].flag, &val))
        {
            if (flag & COF_BOOL)
            {
                if (!*str.txt)
                    s_cat (&str, "options");
                s_catf (&str, " %s %s", ContactOptionsList[i].name, val ? "on" : "off");
            }
            else
            {
                if (*str.txt)
                    s_catc (&str, '\n');
                if (flag & COF_NUMERIC)
                    s_catf (&str, "options %s %d", ContactOptionsList[i].name, val);
                else if (flag & COF_COLOR)
                    s_catf (&str, "options %s %s", ContactOptionsList[i].name, s_quote (ContactOptionsS2C (strtable[val])));
                else
                    s_catf (&str, "options %s %s", ContactOptionsList[i].name, s_quote (strtable[val]));
            }
        }
    if (*str.txt)
        s_catc (&str, '\n');

    return str.txt;
}

/*
 * Import options from a string.
 */
int ContactOptionsImport (ContactOptions *opts, const char *args)
{
    strc_t par;
    char *argst;
    const char *argstt;
    UWORD flag = 0;
    int i, ret = 0;
    
    argst = strdup (args);
    argstt = argst;
    
    while (s_parse (&argstt, &par))
    {
        for (i = 0; ContactOptionsList[i].name; i++)
            if (!strcmp (par->txt, ContactOptionsList[i].name))
            {
                flag = ContactOptionsList[i].flag;
                break;
            }
        
        if (!ContactOptionsList[i].name)
        {
            ret = 1;
            break;
        }
        
        if (!s_parse (&argstt, &par))
        {
            ret = 1;
            break;
        }
        
        if (flag & COF_COLOR)
        {
            char *color = strdup (par->txt);
            ContactOptionsSetStr (opts, flag, ContactOptionsC2S (color));
            ContactOptionsUndef  (opts, CO_CSCHEME);
            free (color);
        }
        else if (flag == CO_ENCODINGSTR)
        {
            UWORD enc = ConvEnc (par->txt) & ~ENC_FAUTO;
            ContactOptionsSetVal (opts, CO_ENCODING, enc);
            ContactOptionsSetStr (opts, CO_ENCODINGSTR, ConvEncName (enc));
        }
        else if (flag & COF_NUMERIC)
        {
            UWORD val = atoi (par->txt);
            
            if (flag == CO_CSCHEME)
                ContactOptionsImport (opts, PrefSetColorScheme (val));
            
            ContactOptionsSetVal (opts, flag, val);
        }
        else if (~flag & COF_BOOL)
            ContactOptionsSetStr (opts, flag, par->txt);
        else if (!strcasecmp (par->txt, "on")  || !strcasecmp (par->txt, i18n (1085, "on")))
            ContactOptionsSetVal (opts, flag, 1);
        else if (!strcasecmp (par->txt, "off") || !strcasecmp (par->txt, i18n (1086, "off")))
            ContactOptionsSetVal (opts, flag, 0);
        else if (!strcasecmp (par->txt, "undef"))
            ContactOptionsUndef (opts, flag);
        else
        {
            ret = 1;
            break;
        }
    }
    free (argst);
    return ret;
}
