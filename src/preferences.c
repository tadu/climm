/*
 * Reads and writes preferences.
 *
 * Note: Most stuff is still in file_util.c
 *
 * This file is Copyright © Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 */

#include "micq.h"
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "file_util.h"
#include "preferences.h"
#include "util_ui.h"
#include "session.h"
#include "util_str.h"

static FILE *PrefOpenRC (Preferences *pref);

/*
 * Create a (global) preference structure
 */
Preferences *PreferencesC ()
{
    Preferences *pref = calloc (1, sizeof (Preferences));
    assert (pref);
    
    pref->flags = FLAG_DELBS;
    PrefSetColorScheme (pref, 0);
    
    return pref;
}

/*
 * Create a connection specific preference structure
 */
PreferencesSession *PreferencesSessionC ()
{
    PreferencesSession *pref = calloc (1, sizeof (PreferencesSession));
    assert (pref);
    
    return pref;
}

#ifdef _WIN32
#define _OS_PREFPATH ".\\"
#define _OS_PATHSEP  '\\'
#define _OS_PATHSEPSTR  "\\"
#else
#ifdef __amigaos__
#define _OS_PREFPATH "/PROGDIR/"
#define _OS_PATHSEP  '/'
#define _OS_PATHSEPSTR  "/"
#else
#define _OS_PREFPATH NULL
#define _OS_PATHSEP  '/'
#define _OS_PATHSEPSTR  "/"
#endif
#endif

static const char *userbasedir = NULL;

/*
 * Open preference file.
 */
static FILE *PrefOpenRC (Preferences *pref)
{
    char def[200];
    FILE *rcf;
    
    if (pref->rcfile)
    {
        rcf = fopen (pref->rcfile, "r");
        if (rcf)
            return rcf;
        M_print (i18n (1864, "Can't open rcfile %s."), pref->rcfile);
        exit (20);
    }
    
    strcpy (def, PrefUserDir ());
    strcat (def, "micqrc");
    pref->rcfile = strdup (def);

    rcf = fopen (pref->rcfile, "r");
    if (rcf)
    {
        pref->rcisdef = 1;
        return rcf;
    }
    return NULL;
}

/*
 * Return the preference base directory
 */
const char *PrefUserDir ()
{
    if (!userbasedir)
    {
        char *home, *path, def[200];
        
        path = _OS_PREFPATH;
        home = getenv ("HOME");
        if (home || !path)
        {
            if (!home)
                home = "";
            assert (strlen (home) < 180);
            strcpy (def, home);
            if (strlen (def) > 0 && def[strlen (def) - 1] != _OS_PATHSEP)
                strcat (def, _OS_PATHSEPSTR);
            strcat (def, ".micq" _OS_PATHSEPSTR);
            userbasedir = strdup (def);
        }
        else
            userbasedir = strdup (path);
    }
    return userbasedir;
}

/*
 * Load the preferences into *pref
 */
void PrefLoad (Preferences *pref)
{
    FILE *rcf;
    
    pref->away_time = default_away_time;
    pref->tabs = TABS_SIMPLE;

    rcf = PrefOpenRC (pref);
    if (rcf)
        Read_RC_File (rcf);

#ifdef WIP
    {
        char extra[200];

        strcpy (extra, PrefUserDir ());
        strcat (extra, "contacts");
        rcf = fopen (extra, "r");
        if (rcf)
            Read_RC_File (rcf);
    }
#endif
    if (!SessionNr (0))
        Initalize_RC_File ();
}

/*
 * Set the color scheme to use.
 */
void PrefSetColorScheme (Preferences *pref, UBYTE scheme)
{
    switch (scheme)
    {
        case 1: /* former colors scheme A */
            s_repl (&pref->colors[CXNONE],     SGR0);
            s_repl (&pref->colors[CXSERVER],   BLUE BOLD);
            s_repl (&pref->colors[CXCLIENT],   RED BOLD);
            s_repl (&pref->colors[CXMESSAGE],  BLUE BOLD);
            s_repl (&pref->colors[CXCONTACT],  GREEN);
            s_repl (&pref->colors[CXSENT],     MAGENTA BOLD);
            s_repl (&pref->colors[CXACK],      GREEN BOLD);
            s_repl (&pref->colors[CXERROR],    RED BOLD);
            s_repl (&pref->colors[CXINCOMING], CYAN BOLD);
            s_repl (&pref->colors[CXDEBUG],    YELLOW);
            break;
        case 2:
            s_repl (&pref->colors[CXNONE],     SGR0);
            s_repl (&pref->colors[CXSERVER],   MAGENTA);
            s_repl (&pref->colors[CXCLIENT],   CYAN);
            s_repl (&pref->colors[CXMESSAGE],  CYAN);
            s_repl (&pref->colors[CXCONTACT],  CYAN);
            s_repl (&pref->colors[CXSENT],     MAGENTA BOLD);
            s_repl (&pref->colors[CXACK],      GREEN BOLD);
            s_repl (&pref->colors[CXERROR],    RED BOLD);
            s_repl (&pref->colors[CXINCOMING], CYAN BOLD);
            s_repl (&pref->colors[CXDEBUG],    YELLOW);
            break;
        case 3:
            s_repl (&pref->colors[CXNONE],     GREEN);
            s_repl (&pref->colors[CXSERVER],   SGR0);
            s_repl (&pref->colors[CXCLIENT],   GREEN);
            s_repl (&pref->colors[CXMESSAGE],  GREEN);
            s_repl (&pref->colors[CXCONTACT],  GREEN BOLD);
            s_repl (&pref->colors[CXSENT],     MAGENTA BOLD);
            s_repl (&pref->colors[CXACK],      GREEN BOLD);
            s_repl (&pref->colors[CXERROR],    RED BOLD);
            s_repl (&pref->colors[CXINCOMING], CYAN BOLD);
            s_repl (&pref->colors[CXDEBUG],    YELLOW);
            break;
        default:
            s_repl (&pref->colors[CXNONE],     SGR0);
            s_repl (&pref->colors[CXSERVER],   RED);
            s_repl (&pref->colors[CXCLIENT],   GREEN);
            s_repl (&pref->colors[CXMESSAGE],  BLUE BOLD);
            s_repl (&pref->colors[CXCONTACT],  MAGENTA BOLD);
            s_repl (&pref->colors[CXSENT],     MAGENTA BOLD);
            s_repl (&pref->colors[CXACK],      GREEN BOLD);
            s_repl (&pref->colors[CXERROR],    RED BOLD);
            s_repl (&pref->colors[CXINCOMING], CYAN BOLD);
            s_repl (&pref->colors[CXDEBUG],    YELLOW);
            scheme = 0;
    }
    pref->scheme = scheme;
}
