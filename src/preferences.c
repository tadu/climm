/*
 * Reads and writes preferences.
 *
 * Note: Most stuff is still in file_util.c
 *
 * mICQ Copyright (C) © 2001,2002,2003 Rüdiger Kuhlmann
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
 * You should have received a copy of the GNU General Public License
 * along with this package; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * $Id$
 */

#include "micq.h"
#include <stdlib.h>
#include <assert.h>
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <fcntl.h>
#include <string.h>
#if HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif
#include "file_util.h"
#include "preferences.h"
#include "util_ui.h"
#include "session.h"
#include "contact.h"

static FILE *PrefOpenRC (Preferences *pref);

/*
 * Create a (global) preference structure
 */
Preferences *PreferencesC ()
{
    Preferences *pref = calloc (1, sizeof (Preferences));
    assert (pref);
    return pref;
}

/*
 * Set some defaults for the given preferences structure
 */
void PreferencesInit (Preferences *pref)
{    
    pref->flags = FLAG_DELBS;
    ContactOptionsImport (&pref->copts, PrefSetColorScheme (4));
    ContactOptionsSetVal (&pref->copts, CO_SHOWCHANGE, 1);
    ContactOptionsSetVal (&pref->copts, CO_SHOWONOFF, 1);
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
#ifndef L_cuserid
#define L_cuserid 20
#endif

const char *PrefLogNameReal (Preferences *pref)
{
    if (!pref->logname)
    {
        const char *me;
        char *hostname;
#if HAVE_UNAME
        struct utsname name;
#endif
        char username[L_cuserid + SYS_NMLN] = "";

        hostname = username;
        me = getenv ("LOGNAME");
        if (!me)
            me = getenv ("USER");
        if (me != NULL)
        {
            snprintf (username, sizeof (username), "%s", me);
            hostname += strlen (username);
        }

#if HAVE_UNAME
        if (!uname (&name))
            snprintf (hostname, username + sizeof (username) - hostname, "@%s", name.nodename);
        else
            *hostname = '\0';
#elif HAVE_GETHOSTNAME
        if (!gethostname (hostname + 1, username + sizeof (username) - hostname - 1))
            *hostname = '@';
        else
            *hostname = '\0';
        username[sizeof (username) - 1] = '\0';
#else
        snprintf (hostname, username + sizeof (username) - hostname, "@(unknown)");
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
    if (!pref->rcfile)
        pref->rcfile = strdup (s_sprintf ("%smicqrc", PrefUserDir (pref)));
    
    return fopen (pref->rcfile, "r");
}

/*
 * Open status file.
 */
static FILE *PrefOpenStat (Preferences *pref)
{
    if (!pref->statfile)
        pref->statfile = strdup (s_sprintf ("%sstatus", PrefUserDir (pref)));

    return fopen (pref->statfile, "r");
}

/*
 * Load the preferences into *pref
 */
BOOL PrefLoad (Preferences *pref)
{
    FILE *rcf, *stf;
    Contact *cont;
    int i;
    BOOL ok = TRUE;
    
    pref->away_time = default_away_time;

    rcf = PrefOpenRC (pref);
    if (rcf)
    {
        i = Read_RC_File (rcf);
        if (i == 2)
        {
            stf = PrefOpenStat (pref);
            if (stf)
                PrefReadStat (stf);
            else
                ok = FALSE;
        }
    }
    else
        ok = FALSE;
    for (i = 0; (cont = ContactIndex (NULL, i)); i++)
        ContactMetaLoad (cont);
    return ok;
}

/*
 * Set the color scheme to use.
 */
const char *PrefSetColorScheme (UBYTE scheme)
{
    switch (scheme)
    {
        case 1: /* former colors scheme A */
            return "colornone none "           "colorserver \"blue bold\" "
                   "colorclient \"red bold\" " "colormessage \"blue bold\" "
                   "colorcontact green "       "colorsent \"magenta bold\" "
                   "colorack \"green bold\" "  "colorerror \"red bold\" "
                   "colorincoming \"cyan bold\" colordebug yellow "
                   "colorquote blue "          "colorinvchar \"red bold\"";
        case 2:
            return "colornone none "           "colorserver magenta "
                   "colorclient cyan "         "colormessage cyan "
                   "colorcontact cyan "        "colorsent \"magenta bold\" "
                   "colorack \"green bold\" "  "colorerror \"red bold\" "
                   "colorincoming \"cyan bold\" colordebug yellow "
                   "colorquote blue "          "colorinvchar \"red bold\"";
        case 3:
            return "colornone green "          "colorserver none "
                   "colorclient green "        "colormessage green "
                   "colorcontact \"green bold\" colorsent \"magenta bold\" "
                   "colorack \"green bold\" "  "colorerror \"red bold\" "
                   "colorincoming \"cyan bold\" colordebug yellow "
                   "colorquote blue "          "colorinvchar \"red bold\"";
        case 4:
            return "colornone none "           "colorserver red "
                   "colorclient green "        "colormessage \"blue bold\" "
                 "colorcontact \"magenta bold\" colorsent \"magenta bold\" "
                   "colorack \"green bold\" "  "colorerror \"red bold\" "
                   "colorincoming \"cyan bold\" colordebug yellow "
                   "colorquote blue "          "colorinvchar \"red bold\"";
        default:
            return "colornone none "           "colorserver none "
                   "colorclient none "         "colormessage none "
                   "colorcontact none "        "colorsent none "
                   "colorack none "            "colorerror none "
                   "colorincoming none "       "colordebug none "
                   "colorquote none "          "colorinvchar none ";
    }
}
