
/*
 * Poor man's gettext; handles internationalization of texts.
 *
 * This file is © Rüdiger Kuhlmann; it may be distributed under the lastest
 * version of your choice of the GPL or BSD licence.
 */

#include "micq.h"
#include "i18n.h"
#include "util_ui.h"
#include <string.h>

#define i18nSLOTS  600

const char *i18nStrings[i18nSLOTS] = { 0 };

/*
 * Opens and reads the localization file defined by parameter or the
 * environment variables LANG, LC_ALL, LC_MESSAGES.
 */
int i18nOpen (const char *loc)
{
    char buf[2048];
    int i, j = 0;
    FD_T i18nf;

    if (!loc)
        loc = getenv ("LANG");
    if (!loc)
        loc = getenv ("LC_ALL");
    if (!loc)
        loc = getenv ("LC_MESSAGES");
    if (!loc)
        return 0;

        i18nf = M_fdopen ("/usr/local/share/micq/%s.i18n", loc);
    if (i18nf == -1 && strchr (loc, '_'))
        i18nf = M_fdopen ("/usr/local/share/micq/%.*s.i18n", strchr (loc, '_') - loc, loc);
    if (i18nf == -1)
        i18nf = M_fdopen ("%s/.micq/%s.i18n", getenv ("HOME") ? getenv ("HOME") : "", loc);
    if (i18nf == -1 && strchr (loc, '_'))
        i18nf = M_fdopen ("%s/.micq/%.*s.i18n", getenv ("HOME") ? getenv ("HOME") : "", strchr (loc, '_') - loc, loc);
    if (i18nf == -1)
        return 0;

    for (i = 0; i < i18nSLOTS; i++)
        i18nStrings[i] = NULL;

    while (M_fdnreadln (i18nf, buf, sizeof (buf)) != -1)
    {
        int i;
        char *p;

        i = strtol (buf, &p, 10);

        if (p == buf || i < 0 || i >= i18nSLOTS)
            continue;
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
