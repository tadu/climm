
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

static FD_T PrefOpenRC (Preferences *pref);


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
#define _OS_PREFPATH "PROGDIR:"
#define _OS_PATHSEP  '/'
#define _OS_PATHSEPSTR  "/"
#else
#define _OS_PREFPATH NULL
#define _OS_PATHSEP  '/'
#define _OS_PATHSEPSTR  "/"
#endif
#endif

static const char *userbasedir = NULL;

FD_T PrefOpenRC (Preferences *pref)
{
    char *home, *path, def[200], olddef[200];
    FD_T rcf;
    
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
        strcpy (olddef, def);
        strcat (def, ".micq/");
    }
    else
    {
        strcpy (def, path);
        strcpy (olddef, path);
    }
    userbasedir = strdup (def);

    strcat (def, "micqrc");
    strcat (olddef, ".micqrc");
    
    if (pref->rcfile)
    {
        rcf = open (pref->rcfile, O_RDONLY);
        if (rcf > 0)
            return rcf;
        M_print (i18n (864, "Can't open rcfile %s."), pref->rcfile);
        exit (20);
    }
    
    pref->rcfile = strdup (def);

    rcf = open (pref->rcfile, O_RDONLY);
    if (rcf > 0)
    {
        pref->rcisdef = 1;
        return rcf;
    }
    
    rcf = open (olddef, O_RDONLY);
    if (rcf > 0)
    {
        M_print (i18n (819, "Can't find rc file %s - using old location %s\n"), def, olddef);
        return rcf;
    }
    return 0;
}

const char *PrefUserDir ()
{
    return userbasedir;
}

void PrefLoad (Preferences *pref)
{
    FD_T rcf;
    
    rcf = PrefOpenRC (pref);
    if (rcf)
        Read_RC_File (rcf);
    else
        Initalize_RC_File (NULL);
}


/* bla */
