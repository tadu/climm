/*
 * Poor man's gettext; handles internationalization of texts.
 *
 * This file is Copyright © Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 */

#include "micq.h"
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "i18n.h"
#include "util.h"
#include "util_ui.h"
#include "util_io.h"
#include "preferences.h"
#include "util_str.h"
#include "conv.h"

/* use numbers 1000 ... 2999 */
#define i18nSLOTS  2000
#define i18nOffset 1000

char *i18nStrings[i18nSLOTS] = { 0 };

static int i18nAdd (FILE *i18nf, int debug, int *res);

/*
 * Finds the default locale/encoding.
 */
void i18nInit (char **loc, UBYTE *enc, const char *arg)
{
    char *p, *q;

    if (!arg)
        arg = getenv ("LC_ALL");
    if (!arg)
        arg = getenv ("LC_MESSAGES");
    if (!arg)
        arg = getenv ("LANG");
    if (!arg)
        arg = "en";
    *loc = q = strdup (arg);

    if (*q == '/')
        return;
    if ((p = strrchr (q, '@')))
    {
        if (!strcasecmp (p, "@euro"))
            *enc = ENC_AUTO | ENC_LATIN9;
        *p = '\0';
    }
    if ((p = strrchr (q, '.')))
    {
        if (!strncasecmp (p, ".KOI", 3))
            *enc = ENC_AUTO | ENC_KOI8;
        if (!strcasecmp (p, ".UTF-8"))
            *enc = ENC_AUTO | ENC_UTF8;
        *p = '\0';
    }
    if (*q == 'C' && !q[1] && enc && *enc == ENC_AUTO)
    {
        s_repl (loc, "en_US");
        *enc = ENC_LATIN1;
    }
}

/*
 * Opens and reads the localization file defined by parameter or the
 * environment variables LANG, LC_ALL, LC_MESSAGES.
 */
int i18nOpen (const char *loc, UBYTE enc)
{
    int j = 0, debug = 0, res = 1;
    char *floc = NULL;
    UBYTE utf8 = 0;
    FILE *i18nf;

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

    i18nInit (&floc, &enc, loc);
    loc = floc;

    if (!strcmp (loc, "C"))
    {
        s_free (floc);
        i18nClose ();
        return 0;
    }
    
    if ((enc & ~ENC_AUTO) == ENC_UTF8)
        utf8 = 1;

#define i18nTry(f) do { \
    if ((i18nf = fopen (f, "r"))) \
        j += i18nAdd (i18nf, debug, &res); \
} while (0)

    if (*loc == '/')
        i18nTry (loc);
    else
    {
        if (prG->flags & FLAG_FUNNY)
        {
            if (utf8)
                i18nTry (s_sprintf ("%s/i18n/%s.UTF-8@fun.i18n", PrefUserDir (prG), loc));
            i18nTry (s_sprintf ("%s/i18n/%s@fun.i18n", PrefUserDir (prG), loc));
            if (strchr (loc, '_') && utf8)
                i18nTry (s_sprintf ("%s/i18n/%.*s.UTF-8@fun.i18n", PrefUserDir (prG), strchr (loc, '_') - loc, loc));
            if (strchr (loc, '_'))
                i18nTry (s_sprintf ("%s/i18n/%.*s@fun.i18n", PrefUserDir (prG), strchr (loc, '_') - loc, loc));
            if (utf8)
                i18nTry (s_sprintf (PKGDATADIR "/%s.UTF-8@fun.i18n", loc));
            i18nTry (s_sprintf (PKGDATADIR "/%s@fun.i18n", loc));
            if (strchr (loc, '_') && utf8)
                i18nTry (s_sprintf (PKGDATADIR "/%.*s.UTF-8@fun.i18n", strchr (loc, '_') - loc, loc));
            if (strchr (loc, '_'))
                i18nTry (s_sprintf (PKGDATADIR "/%.*s@fun.i18n", strchr (loc, '_') - loc, loc));
        }

        if (utf8)
            i18nTry (s_sprintf ("%s/i18n/%s.UTF-8.i18n", PrefUserDir (prG), loc));
        i18nTry (s_sprintf ("%s/i18n/%s.i18n", PrefUserDir (prG), loc));
        if (strchr (loc, '_') && utf8)
            i18nTry (s_sprintf ("%s/i18n/%.*s.UTF-8.i18n", PrefUserDir (prG), strchr (loc, '_') - loc, loc));
        if (strchr (loc, '_'))
            i18nTry (s_sprintf ("%s/i18n/%.*s.i18n", PrefUserDir (prG), strchr (loc, '_') - loc, loc));
        if (utf8)
            i18nTry (s_sprintf (PKGDATADIR "/%s.UTF-8.i18n", loc));
        i18nTry (s_sprintf (PKGDATADIR "/%s.i18n", loc));
        if (strchr (loc, '_') && utf8)
            i18nTry (s_sprintf (PKGDATADIR "/%.*s.UTF-8.i18n", strchr (loc, '_') - loc, loc));
        if (strchr (loc, '_'))
            i18nTry (s_sprintf (PKGDATADIR "/%.*s.i18n", strchr (loc, '_') - loc, loc));
    }
    s_free (floc);

    return j ? j : -1;
}

/*
 * Adds i18n strings from given file descriptor.
 */
static int i18nAdd (FILE *i18nf, int debug, int *res)
{
    char buf[2048];
    int j = 0;
    UBYTE enc = 0;
    
    if (*res)
    {
        i18nClose ();
        *res = 0;
    }
    while (M_fdnreadln (i18nf, buf, sizeof (buf)) != -1)
    {
        int i;
        char *p;

        i = strtol (buf, &p, 10) - i18nOffset;

        if (i == 7)
        {
            if      (!strcasecmp (p + 1, "iso-8859-1")) enc = ENC_LATIN1;
            else if (!strcasecmp (p + 1, "koi8-r"))     enc = ENC_KOI8;
            else if (!strcasecmp (p + 1, "koi8-u"))     enc = ENC_KOI8;
            else if (!strcasecmp (p + 1, "utf-8"))      enc = ENC_UTF8;
            else                                        enc = ENC_LATIN1;
            if (prG->enc_loc == ENC_AUTO)
                prG->enc_loc = ENC_AUTO | enc;
        }

        if (p == buf || i < 0 || i >= i18nSLOTS || i18nStrings[i])
            continue;
        
        p = debug ? buf : p + 1;
#ifdef ENABLE_UTF8
        i18nStrings[i] = p = strdup (ConvToUTF8 (p, enc ? enc : ENC_LATIN1));
#else
        i18nStrings[i] = p = strdup (p);
#endif
        j++;
        for (; *p; p++)
            if (*p == '\\' && p[1] == 'n')
            {
                *p++ = '\r';
                *p = '\n';
            }
    }
    fclose (i18nf);
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
        s_repl (p, NULL);
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

    if (i >= i18nOffset && i < i18nSLOTS + i18nOffset)
        if ((p = i18nStrings[i - i18nOffset]))
            return p;

    return txt;
}
