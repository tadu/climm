
#include <assert.h>
#include <string.h>
#include "micq.h"
#include "contactopts.h"
#include "util_str.h"

static const char **strtable = NULL;
static int strmax = 0;

struct ContactOptionsTable_s
{
    UBYTE tags[32];
    UWORD vals[32];
};

BOOL ContactOptionsGet (const ContactOptions *opt, UWORD flag, const char **res)
{
    if (flag & CO_DIRECT)
    {
        if ((flag & ~CO_DIRECT) & opt->set)
        {
            *res = ((flag & ~CO_DIRECT) & opt->val) ? "" : NULL;
            return TRUE;
        }
        return FALSE;
    }
    
    if (opt->set & CO_DIRECT)
    {
        int j, k;
        for (j = k = 0; j < opt->co_un.co_indir.size; j++)
        {
            for (k = 0; k < 32; k++)
                if (opt->co_un.co_indir.table[j].tags[k] == flag)
                    break;
            if (k != 32)
                break;
        }
        if (j == opt->co_un.co_indir.size)
        {
            return FALSE;
        }
        *res = strtable[opt->co_un.co_indir.table[j].vals[k]];
        return TRUE;
    }

    if (opt->co_un.co_dir.taga == flag)
    {
        *res = strtable[opt->co_un.co_dir.vala];
        return TRUE;
    }
    if (opt->co_un.co_dir.tagb == flag)
    {
        *res = strtable[opt->co_un.co_dir.valb];
        return TRUE;
    }
    return FALSE;
}

void ContactOptionsSet (ContactOptions *opt, UWORD flag, const char *val)
{
    if (flag & CO_DIRECT)
    {
        flag &= ~CO_DIRECT;
        if (val)
        {
            opt->set |= flag;
            if (*val)
                opt->val |= flag;
            else
                opt->set &= ~flag;
        }
        else
        {
            opt->set &= ~flag;
            opt->val &= ~flag;
        }
    }
    else
    {
        if (val)
        {
            int i, j, k;

            for (i = 0; i < strmax; i++)
                if (!strtable[i] || !strcmp (strtable[i], val))
                    break;
            if (i == strmax)
            {
                int news = (strmax ? strmax * 2 : 128);
                const char **new = realloc (strtable, sizeof (char *) * news);
                if (!new)
                    return;
                for (j = i; j < news; j++)
                    new[j] = NULL;
                strtable = new;
                strmax = news;
            }
            if (!strtable[i])
                strtable[i] = strdup (val);

            if ((~opt->set & CO_DIRECT) && (!opt->co_un.co_dir.taga || (opt->co_un.co_dir.taga == flag)))
            {
                opt->co_un.co_dir.taga = flag & ~CO_DIRECT;
                opt->co_un.co_dir.vala = i;
                return;
            }
            if ((~opt->set & CO_DIRECT) && (!opt->co_un.co_dir.tagb || (opt->co_un.co_dir.tagb == flag)))
            {
                opt->co_un.co_dir.tagb = flag & ~CO_DIRECT;
                opt->co_un.co_dir.valb = i;
                return;
            }
            if (~opt->set & CO_DIRECT)
            {
                struct ContactOptionsTable_s *new = calloc (sizeof (struct ContactOptionsTable_s), 1);
                if (!new)
                    return;
                new->tags[0] = opt->co_un.co_dir.taga;
                new->tags[1] = opt->co_un.co_dir.tagb;
                new->vals[0] = opt->co_un.co_dir.vala;
                new->vals[1] = opt->co_un.co_dir.valb;
                opt->co_un.co_indir.size = 1;
                opt->co_un.co_indir.table = new;
                opt->set |= CO_DIRECT;
            }
            for (j = k = 0; j < opt->co_un.co_indir.size; j++)
            {
                for (k = 0; k < 32; k++)
                    if ((opt->co_un.co_indir.table[j].tags[k] == flag) || !opt->co_un.co_indir.table[j].tags[k])
                        break;
                if (k != 32)
                    break;
            }
            if (j == opt->co_un.co_indir.size)
            {
                struct ContactOptionsTable_s *new = realloc (opt->co_un.co_indir.table, sizeof (struct ContactOptionsTable_s) * (j + 1));
                if (!new)
                    return;
                opt->co_un.co_indir.size++;
                for (k = 0; k < 32; k++)
                    new[j].tags[k] = new[j].vals[k] = 0;
                k = 0;
                opt->co_un.co_indir.table = new;
            }
            opt->co_un.co_indir.table[j].tags[k] = flag;
            opt->co_un.co_indir.table[j].vals[k] = i;
        }
        else
        {
            if (opt->set & CO_DIRECT)
            {
                int j, k, l;
                for (j = k = 0; j < opt->co_un.co_indir.size; j++)
                    for (k = 0; k < 32; k++)
                        if (opt->co_un.co_indir.table[j].tags[k] == flag)
                            break;
                if (j == opt->co_un.co_indir.size)
                    return;
                for (l = 0; opt->co_un.co_indir.table[opt->co_un.co_indir.size - 1].tags[l]; l++)
                    ;
                assert (l);
                l--;
                opt->co_un.co_indir.table[j].tags[k] = opt->co_un.co_indir.table[opt->co_un.co_indir.size - 1].tags[l];
                opt->co_un.co_indir.table[j].vals[k] = opt->co_un.co_indir.table[opt->co_un.co_indir.size - 1].vals[l];
                if (l)
                {
                    opt->co_un.co_indir.table[opt->co_un.co_indir.size - 1].tags[l] = 0;
                    opt->co_un.co_indir.table[opt->co_un.co_indir.size - 1].vals[l] = 0;
                }
                else
                    opt->co_un.co_indir.size--;
            }
            else
            {
                if (opt->co_un.co_dir.taga == flag)
                {
                    opt->co_un.co_dir.taga = 0;
                    opt->co_un.co_dir.vala = 0;
                }
                if (opt->co_un.co_dir.tagb == flag)
                {
                    opt->co_un.co_dir.tagb = 0;
                    opt->co_un.co_dir.valb = 0;
                }
            }
        }
    }
}

struct ContactOption_s ContactOptionsList[] = {
  { "intimate",  CO_INTIMATE,  TRUE },
  { "hidefrom",  CO_HIDEFROM,  TRUE },
  { "ignore",    CO_IGNORE,    TRUE },
  { "autoaway",  CO_AUTOAWAY,  FALSE },
  { "autona",    CO_AUTONA,    FALSE },
  { "autoocc",   CO_AUTOOCC,   FALSE },
  { "autodnd",   CO_AUTODND,   FALSE },
  { "autoffc",   CO_AUTOFFC,   FALSE },
  { NULL }
};

const char *ContactOptionsString (const ContactOptions *opts)
{
    static str_s str;
    int i;
    const char *res;
    
    s_init (&str, "", 100);
    
    for (i = 0; ContactOptionsList[i].name; i++)
        if (ContactOptionsGet (opts, ContactOptionsList[i].flag, &res))
        {
            if (ContactOptionsList[i].isbool)
                s_catf (&str, " %s %s", ContactOptionsList[i].name, res ? "on" : "off");
            else
                s_catf (&str, " %s %s", ContactOptionsList[i].name, s_quote (res));
        }

    return *str.txt ? str.txt : NULL;
}

int ContactOptionsImport (ContactOptions *opts, const char *args)
{
    char *cmd;
    UWORD flag = 0;
    int booli = 0, i;
    
    while (s_parse (&args, &cmd))
    {
        for (i = 0; ContactOptionsList[i].name; i++)
            if (!strcmp (cmd, ContactOptionsList[i].name))
            {
                flag = ContactOptionsList[i].flag;
                booli = ContactOptionsList[i].isbool;
                break;
            }
        
        if (!ContactOptionsList[i].name)
            return 1;
        
        if (!s_parse (&args, &cmd))
            return 1;

        if (!booli)
            ContactOptionsSet (opts, flag, cmd);
        else if (!strcasecmp (cmd, "on")  || !strcasecmp (cmd, i18n (1085, "on")))
            ContactOptionsSet (opts, flag, "+");
        else if (!strcasecmp (cmd, "off") || !strcasecmp (cmd, i18n (1086, "off")))
            ContactOptionsSet (opts, flag, "");
        else if (!strcasecmp (cmd, "undef"))
            ContactOptionsSet (opts, flag, NULL);
        else
            return 1;
    }
    return 0;
}
