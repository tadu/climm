/* $Id$ */

/*
 * Poor man's gettext; handles internationalization of texts.
 *
 * This file is © Rüdiger Kuhlmann; it may be distributed under the BSD
 * licence (without the advertising clause) or version 2 of the GPL.
 */

#include "micq.h"
#include "i18n.h"
#include "util.h"
#include "util_ui.h"
#include <string.h>

#define i18nSLOTS  900

char *i18nStrings[i18nSLOTS] = { 0 };

static int i18nAdd (int i18nf, int debug, int *res);

/*
 * Opens and reads the localization file defined by parameter or the
 * environment variables LANG, LC_ALL, LC_MESSAGES.
 */
int i18nOpen (const char *loc)
{
    int j = 0, debug = 0, res = 1;
    FD_T i18nf;

    if (*loc == '+')
    {
        res = 0;
        loc++;
    }
    if (!strncmp (loc, "debug", 5))
    {
        debug = 1;
        loc += 5;
    }

    if (!strcmp (loc, "!") || !strcmp (loc, "auto") || !strcmp (loc, "default"))
        loc = NULL;

    if (!loc)   loc = getenv ("LANG");
    if (!loc)   loc = getenv ("LC_ALL");
    if (!loc)   loc = getenv ("LC_MESSAGES");
    if (!loc)   return 0;

    if (!strcmp (loc, "C"))
    {
        i18nClose ();
        return 0;
    }

#define i18nTry(x,y,z,a) if ((i18nf = M_fdopen (x,y,z,a)) != -1) j += i18nAdd (i18nf, debug, &res);

    if (*loc == '/')
    {
        i18nTry (loc, "", "", "");
    }
    else
    {
        if (prG->flags & FLAG_FUNNY)
        {
            i18nTry ("%s/i18n/%s_fun.i18n", GetUserBaseDir (), loc, "");
            if (strchr (loc, '_'))
                i18nTry ("%s/i18n/%.*s_fun.i18n", GetUserBaseDir (), strrchr (loc, '_') - loc, loc);
            if (strchr (loc, '_') && strchr (loc, '_') != strrchr (loc, '_'))
                i18nTry ("%s/i18n/%.*s_fun.i18n", GetUserBaseDir (), strchr (loc, '_') - loc, loc);
            i18nTry (PREFIX "/share/micq/%s_fun.i18n", loc, "", "");
            if (strchr (loc, '_'))
                i18nTry (PREFIX "/share/micq/%.*s_fun.i18n", strrchr (loc, '_') - loc, loc, "");
            if (strchr (loc, '_') && strchr (loc, '_') != strrchr (loc, '_'))
                i18nTry (PREFIX "/share/micq/%.*s_fun.i18n", strchr (loc, '_') - loc, loc, "");
        }

        i18nTry ("%s/i18n/%s.i18n", GetUserBaseDir (), loc, "");
        if (strchr (loc, '_'))
            i18nTry ("%s/i18n/%.*s.i18n", GetUserBaseDir (), strrchr (loc, '_') - loc, loc);
        if (strchr (loc, '_') && strchr (loc, '_') != strrchr (loc, '_'))
            i18nTry ("%s/i18n/%.*s.i18n", GetUserBaseDir (), strchr (loc, '_') - loc, loc);
        i18nTry (PREFIX "/share/micq/%s.i18n", loc, "", "");
        if (strchr (loc, '_'))
            i18nTry (PREFIX "/share/micq/%.*s.i18n", strrchr (loc, '_') - loc, loc, "");
        if (strchr (loc, '_') && strchr (loc, '_') != strrchr (loc, '_'))
            i18nTry (PREFIX "/share/micq/%.*s.i18n", strchr (loc, '_') - loc, loc, "");
    }

    return j ? j : -1;
}

/*
 * Adds i18n strings from given file descriptor.
 */
int i18nAdd (int i18nf, int debug, int *res)
{
    char buf[2048];
    int j = 0;
    
    if (*res)
    {
        i18nClose ();
        *res = 0;
    }
    while (M_fdnreadln (i18nf, buf, sizeof (buf)) != -1)
    {
        int i;
        char *p;

        i = strtol (buf, &p, 10);

        if (p == buf || i < 0 || i >= i18nSLOTS || i18nStrings[i])
            continue;
        
        if (debug)
            i18nStrings[i] = p = strdup (buf);
        else
            i18nStrings[i] = p = strdup (++p);
        j++;
        for (; *p; p++)
            if (*p == '¶')
                *p = '\n';
    }
    close (i18nf);
    return j;
}

/*
 * Frees all internationalization strings.
 */
void i18nClose (void)
{
    char **p, **q;
    
    q = i18nStrings + i18nSLOTS;
    for (p = i18nStrings; p < q; p++)
    {
        if (*p)
            free (*p);
        *p = NULL;
    }
}

/*
 * Looks up the localized text by number, using the given text as default if
 * it is not found. If the text number is -1, the number is in the string as
 * constructed by the _i18n macro for static localized data.
 */
const char *i18n (int i, const char *txt)
{
    const char *p;

    if (i == -1)
    {
        char *q;
        i = strtol (txt, &q, 10);
        txt = ++q;
    }

    if (i >= 0 && i < i18nSLOTS)
        if ((p = i18nStrings[i]))
            return p;

    return txt;
}
