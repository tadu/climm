/*
 * Poor man's gettext; handles internationalization of texts.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "i18n.h"
#include "util.h"
#include "util_ui.h"
#include "util_io.h"
#include "preferences.h"
#include "conv.h"
#include "os.h"

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#endif

/* use numbers 1000 ... 2999 */
#define i18nSLOTS  4000
#define i18nOffset 1000
#define i18nFunny  2000

char *i18nStrings[i18nSLOTS] = { 0 };

static int i18nAdd (FILE *i18nf, int debug, int *res);

/*
 * Finds the default locale/encoding.
 */
void i18nInit (const char *arg)
{
    const char *earg = arg;
    char *newl;

    if (arg && !*arg)
        arg = NULL;
    prG->locale_broken = FALSE;

#if HAVE_SETLOCALE && defined(LC_MESSAGES)
    if (!setlocale (LC_ALL, arg ? arg : ""))
    {
        setlocale (LC_ALL, "C");
        prG->locale_broken = TRUE;
    }
    if (!arg)
    {
        arg = setlocale (LC_MESSAGES, NULL);
        if (arg)
        {
#if HAVE_NL_LANGINFO && defined (CODESET)
            if (!*arg) /* if nl_langinfo() exists we assume setlocale() to be more than a dummy */
#else
            if (!*arg || !strcasecmp (arg, "C") || !strcasecmp (arg, "POSIX"))
#endif
            {
                prG->locale_broken = TRUE;
                arg = NULL;
            }
            else
            {
                if (!prG->locale_orig)
                    s_repl (&prG->locale_orig, arg);
            }
            if (!prG->locale_full)
                s_repl (&prG->locale_full, arg);
        }
        else
            prG->locale_broken = TRUE;
    }
#else
    prG->locale_broken = TRUE;
#endif

    if (!earg)
    {
        earg = getenv ("LC_ALL");
        if (!earg)
            earg = getenv ("LC_MESSAGES");
        if (!earg)
            earg = getenv ("LANG");
        if (!earg)
            earg = "C";
        if (earg && !prG->locale_orig)
            s_repl (&prG->locale_orig, earg);
        if (earg && !prG->locale_full)
            s_repl (&prG->locale_full, earg);
        if (strchr (earg, '.') && arg && !strchr (arg, '.'))
        {
            arg = earg;
            prG->locale_broken = TRUE;
            s_repl (&prG->locale_full, earg);
        }
    }

    newl = strdup (arg ? arg : earg);
    s_free (prG->locale);
    prG->locale = newl;

#if HAVE_NL_LANGINFO && defined (CODESET)
    if (prG->enc_loc & ENC_FGUESS && !prG->locale_broken)
    {
        const char *encnli = nl_langinfo (CODESET);
        if (encnli)
            prG->enc_loc = ENC_FLC | ConvEnc (encnli);
    }
#endif
}

#define i18nTry(f) do { \
    if ((i18nf = fopen (f, "r"))) \
        j += i18nAdd (i18nf, debug, &res); \
} while (0)

/*
 * Opens and reads the localization file defined by parameter.
 */
int i18nOpen (const char *ploc)
{
    int j = 0, debug = 0, res = 1;
    const char *loc;
    char *floc = NULL, *p;
    FILE *i18nf;
    
    loc = ploc ? ploc : prG->locale_full;

    if (!loc || !strcmp (loc, "en_US.US-ASCII") || !strcmp (loc, "C"))
    {
        i18nClose ();
        if (ploc)
        {
            i18nInit ("C");
            /* if encoding was fixed in the micqrc file, never change it */
            if (prG->enc_loc & ENC_FLAGS)
                prG->enc_loc = ENC_FGUESS | ENC_ASCII;
        }
        return 0;
    }
    
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

    if (prG->enc_loc & ENC_FGUESS)
        prG->enc_loc = ENC_AUTO;

    if (*loc == '/')
        i18nTry (loc);
    else
    {
        const char *encs;

        if (!strcasecmp (loc, "!") || !strcasecmp (loc, "auto") || !strcasecmp (loc, "default"))
        {
            s_repl (&prG->locale_full, NULL);
            i18nInit (NULL);
            loc = prG->locale_full;
            ploc = NULL;
        }

        i18nClose ();
        if (ploc)
            i18nInit (loc);
        
        floc = strdup (prG->locale);
        if ((p = strrchr (floc, '@')))
        {
            if (prG->enc_loc == ENC_AUTO && !strcasecmp (p, "@euro"))
                prG->enc_loc = ENC_FGUESS | ENC_LATIN9;
            *p = '\0';
        }
        if ((p = strrchr (floc, '.')))
        {
            if (prG->enc_loc == ENC_AUTO)
            {
                if (!strncasecmp (p, ".KOI", 3))
                    prG->enc_loc = ENC_FGUESS | ENC_KOI8;
                else
                    prG->enc_loc = ENC_FGUESS | ConvEnc (p + 1);
            }
            *p = '\0';
        }
        encs = ConvEncName (prG->enc_loc);

        i18nTry (s_sprintf ("%si18n" _OS_PATHSEPSTR "%s.%s.i18n", PrefUserDir (prG), floc, encs));
        i18nTry (s_sprintf ("%si18n" _OS_PATHSEPSTR "%s.i18n", PrefUserDir (prG), floc));

        if (strchr (floc, '_'))
        {
            i18nTry (s_sprintf ("%si18n" _OS_PATHSEPSTR "%.*s.%s.i18n", PrefUserDir (prG), (int)(strchr (floc, '_') - floc), floc, encs));
            i18nTry (s_sprintf ("%si18n" _OS_PATHSEPSTR "%.*s.i18n", PrefUserDir (prG), (int)(strchr (floc, '_') - floc), floc));
        }

        i18nTry (s_sprintf ("%si18n" _OS_PATHSEPSTR "%s.%s.i18n", PrefDefUserDir (prG), floc, encs));
        i18nTry (s_sprintf ("%si18n" _OS_PATHSEPSTR "%s.i18n", PrefDefUserDir (prG), floc));

        if (strchr (floc, '_'))
        {
            i18nTry (s_sprintf ("%si18n" _OS_PATHSEPSTR "%.*s.%s.i18n", PrefDefUserDir (prG), (int)(strchr (floc, '_') - floc), floc, encs));
            i18nTry (s_sprintf ("%si18n" _OS_PATHSEPSTR "%.*s.i18n", PrefDefUserDir (prG), (int)(strchr (floc, '_') - floc), floc));
        }

        i18nTry (s_sprintf ("%s" _OS_PATHSEPSTR "%s.%s.i18n", os_packagedatadir (), floc, encs));
        i18nTry (s_sprintf ("%s" _OS_PATHSEPSTR "%s.i18n", os_packagedatadir (), floc));

        if (strchr (floc, '_'))
        {
            i18nTry (s_sprintf ("%s" _OS_PATHSEPSTR "%.*s.%s.i18n", os_packagedatadir (), (int)(strchr (floc, '_') - floc), floc, encs));
            i18nTry (s_sprintf ("%s" _OS_PATHSEPSTR "%.*s.i18n", os_packagedatadir (), (int)(strchr (floc, '_') - floc), floc));
        }

        s_free (floc);
        if (prG->enc_loc == ENC_AUTO)
            prG->enc_loc = ENC_FGUESS | ENC_ASCII;
    }

    return j ? j : -1;
}

/*
 * Adds i18n strings from given file descriptor.
 */
static int i18nAdd (FILE *i18nf, int debug, int *res)
{
    strc_t line;
    str_s str;
    int i, j = 0;
    UBYTE enc = ENC_UTF8, thisenc = 0;
    
    if (*res)
    {
        i18nClose ();
        *res = 0;
    }
    while ((line = UtilIOReadline (i18nf)))
    {
        char *p;

        i = strtol (line->txt, &p, 10) - i18nOffset;

        if (i == 7 || !i)
        {
            enc = ConvEnc (p + 1);
            if (prG->enc_loc == ENC_AUTO)
            {
                prG->enc_loc = ENC_FGUESS | enc;
                thisenc = 1;
            }
        }
        else if (i == 8) /* i18n (1008, "US-ASCII") */
        {
            if (prG->enc_loc == ENC_AUTO || thisenc)
                prG->enc_loc = ENC_FGUESS | ConvEnc (p + 1);
        }

        if (p == line->txt || i < 0 || i >= i18nSLOTS || i18nStrings[i])
            continue;
        
        str.txt = debug ? line->txt : p + 1;
        str.len = debug ? line->len : line->len - (p + 1 - line->txt);
        i18nStrings[i] = p = strdup (ConvFrom (&str, enc)->txt);
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
    {
        if (prG->flags & FLAG_FUNNY && (p = i18nStrings[i - i18nOffset + i18nFunny]))
            return p;
        if ((p = i18nStrings[i - i18nOffset]))
            return p;
    }

    return txt;
}
