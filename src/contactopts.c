
#include <assert.h>
#include <string.h>
#include "micq.h"
#include "conv.h"
#include "util_ui.h"
#include "preferences.h"
#include "contactopts.h"
#include "util_str.h"

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
  { "autoaway",      CO_AUTOAWAY      },
  { "autona",        CO_AUTONA        },
  { "autoocc",       CO_AUTOOCC       },
  { "autodnd",       CO_AUTODND       },
  { "autoffc",       CO_AUTOFFC       },
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
        if ((flag & ~COF_DIRECT) & opt->set)
        {
            *res = ((flag & ~COF_DIRECT) & opt->val) ? 1 : 0;
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
        flag &= ~COF_DIRECT;
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
    char *cmd, *argst;
    const char *argstt;
    UWORD flag = 0;
    int i, ret = 0;
    
    argst = strdup (args);
    argstt = argst;
    
    while (s_parse (&argstt, &cmd))
    {
        for (i = 0; ContactOptionsList[i].name; i++)
            if (!strcmp (cmd, ContactOptionsList[i].name))
            {
                flag = ContactOptionsList[i].flag;
                break;
            }
        
        if (!ContactOptionsList[i].name)
        {
            ret = 1;
            break;
        }
        
        if (!s_parse (&argstt, &cmd))
        {
            ret = 1;
            break;
        }
        
        if (flag & COF_NUMERIC)
        {
            UWORD val = atoi (cmd);
            ContactOptionsSetVal (opts, flag, val);
        }
        else if (~flag & COF_BOOL)
            ContactOptionsSetStr (opts, flag, cmd);
        else if (!strcasecmp (cmd, "on")  || !strcasecmp (cmd, i18n (1085, "on")))
            ContactOptionsSetVal (opts, flag, 1);
        else if (!strcasecmp (cmd, "off") || !strcasecmp (cmd, i18n (1086, "off")))
            ContactOptionsSetVal (opts, flag, 0);
        else if (!strcasecmp (cmd, "undef"))
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
