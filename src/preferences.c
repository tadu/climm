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

#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "micq.h"
#include "file_util.h"
#include "preferences.h"
#include "util_ui.h"

static FILE *PrefOpenRC (Preferences *pref);


Preferences *PreferencesC ()
{
    Preferences *pref = calloc (1, sizeof (Preferences));
    assert (pref);
    
    pref->flags = FLAG_DELBS;
    
    return pref;
}

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

FILE *PrefOpenRC (Preferences *pref)
{
    char def[200];
    FILE *rcf;
    
    if (pref->rcfile)
    {
        rcf = fopen (pref->rcfile, "r");
        if (rcf)
            return rcf;
        M_print (i18n (864, "Can't open rcfile %s."), pref->rcfile);
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

void PrefLoad (Preferences *pref)
{
    FILE *rcf;
    
    rcf = PrefOpenRC (pref);
    if (rcf)
        Read_RC_File (rcf);
    else
        Initalize_RC_File (NULL);
}
