
#include <assert.h>
#include <string.h>
#include "micq.h"
#include "conv.h"
#include "util_ui.h"
#include "preferences.h"
#include "contactopts.h"

static char **strtable = NULL;
static int strmax = 0;
static int struse = 0;

#define TABLESIZE CONTACTOPTS_TABLESIZE

typedef struct ContactOptionsTable_s COT;

struct ContactOption_s ContactOptionsList[] = {
  { "intimate",      CO_INTIMATE      },
  { "hidefrom",      CO_HIDEFROM      },
  { "ignore",        CO_IGNORE        },
  { "logonoff",      CO_LOGONOFF      },
  { "logchange",     CO_LOGCHANGE     },
  { "logmess",       CO_LOGMESS       },
  { "showonoff",     CO_SHOWONOFF     },
  { "showchange",    CO_SHOWCHANGE    },
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
 * Get a contact option.
 */
BOOL ContactOptionsGetVal (const ContactOptions *opt, UDWORD flag, val_t *res)
{
    const ContactOptions *cot;
    UBYTE tag = flag & 0xff;
    int k;

    for (cot = opt; cot; cot = cot->next)
    {
        for (k = 0; k < TABLESIZE; k++)
            if (cot->tags[k] == tag)
                break;
        if (k != TABLESIZE)
            break;
    }
    if (!cot || ((flag & COF_BOOL) && (~cot->vals[k] & (flag & CO_BOOLMASK))))
    {
        Debug (DEB_OPTS, "(%p,%lx) undef", opt, flag);
        return FALSE;
    }
    *res = (flag & COF_BOOL) ? (cot->vals[k] & (flag * 2) & CO_BOOLMASK) != 0 : cot->vals[k];
    Debug (DEB_OPTS, "(%p,%lx) = %lx = %lu", opt, flag, *res, *res);
    return TRUE;
}

/*
 * Set a contact option.
 */
BOOL ContactOptionsSetVal (ContactOptions *opt, UDWORD flag, val_t val)
{
    ContactOptions *cot, *cotold;
    int k;
    UBYTE tag = flag & 0xff;

    cotold = NULL;
    for (cot = opt; cot; cot = cot->next)
    {
        cotold = cot;
        for (k = 0; k < TABLESIZE; k++)
            if ((cot->tags[k] == tag) || !cot->tags[k])
                break;
        if (k != TABLESIZE)
            break;
    }
    if (!cot)
    {
        if (!(cot = calloc (sizeof (ContactOptions), 1)))
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
val_t ContactOptionsUndef (ContactOptions *opt, UDWORD flag)
{
    ContactOptions *cot, *cotold;
    UBYTE tag = flag & 0xff;
    val_t old = 0;
    int k, m;


    for (cot = opt; cot; cot = cot->next)
    {
        for (k = 0; k < TABLESIZE; k++)
            if (cot->tags[k] == tag)
                break;
        if (k != TABLESIZE)
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
    for (m = TABLESIZE - 1; m >= 0; m--)
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
 * Get a (string) contact option.
 */
BOOL ContactOptionsGetStr (const ContactOptions *opt, UDWORD flag, const char **res)
{
    val_t val;

    assert (flag & (COF_STRING | COF_COLOR));

    if (!ContactOptionsGetVal (opt, flag, &val))
        return FALSE;

    if (val >= struse)
        return FALSE;

    *res = strtable[val];
    Debug (DEB_OPTS, "(%p,%lx) = %s", opt, flag, s_quote (*res));
    return TRUE;
}

/*
 * Set a (string) contact option.
 */
BOOL ContactOptionsSetStr (ContactOptions *opt, UDWORD flag, const char *text)
{
    val_t val;

    assert (flag & (COF_STRING | COF_COLOR));
    
    if ((val = ContactOptionsUndef (opt, flag)))
    {
        free (strtable[val]);
        strtable[val] = NULL;
        if (val + 1 == struse)
            while (!strtable[val])
                val--, struse--;
    }
    if (!text)
        return TRUE;

    if (struse == strmax)
    {
        int j, news = (strmax ? strmax * 2 : 128);
        char **new = realloc (strtable, sizeof (char *) * news);
        if (!new)
            return FALSE;
        for (j = struse; j < news; j++)
            new[j] = NULL;
        strtable = new;
        strmax = news;
    }
    if (!(strtable[val = struse] = strdup (text)))
        return FALSE;
    
    struse++;
    Debug (DEB_OPTS, "(%p,%lx) := %ld / %s", opt, flag, val, s_quote (strtable [val]));

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
    val_t val;
    
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
                    s_catf (&str, "options %s %lu", ContactOptionsList[i].name, val);
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
    UDWORD flag = 0;
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
            val_t val = atoll (par->txt);
            
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
