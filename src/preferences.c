/*
 * Reads and writes preferences.
 *
 * Note: Most stuff is still in file_util.c
 *
 * climm Copyright (C) © 2001-2007 Rüdiger Kuhlmann
 *
 * climm is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 dated June, 1991.
 *
 * climm is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * In addition, as a special exception permission is granted to link the
 * code of this release of climm with the OpenSSL project's "OpenSSL"
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

#include "climm.h"
#include <assert.h>
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <fcntl.h>
#if HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif
#if HAVE_PWD_H
#include <pwd.h>
#endif
#include "file_util.h"
#include "preferences.h"
#include "connection.h"
#include "util_ui.h"
#include "conv.h"
#include "contact.h"
#include "os.h"
#include "im_icq8.h"

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
    OptImport (&pref->copts, PrefSetColorScheme (4));
    OptSetVal (&pref->copts, CO_SHOWCHANGE, 1);
    OptSetVal (&pref->copts, CO_SHOWONOFF, 1);
    OptSetVal (&pref->copts, CO_WANTSBL, 1);
}

/*
 * Return the default preference base directory
 */
const char *PrefDefUserDirReal (Preferences *pref)
{
    if (!pref->defaultbasedir)
    {
        const char *home;
#if HAVE_GETPWUID && HAVE_GETUID && !__AMIGA__
        struct passwd *pwd = getpwuid (getuid ());
#endif
#if defined(_WIN32) || (defined(__CYGWIN__) && defined(_X86_))
        home = getenv ("USERPROFILE");
        if (!home || !*home)
#endif
        home = getenv ("HOME");
#if HAVE_GETPWUID && HAVE_GETUID && !__AMIGA__
        if ((!home || !*home) && pwd && pwd->pw_dir)
            home = pwd->pw_dir;
#endif
#if defined(_WIN32) || (defined(__CYGWIN__) && defined(_X86_))
        if (!home || !*home)
            home = os_packagehomedir ();
#endif
        if (!home || !*home)
            home = _OS_PREFPATH;

        if (!home || !*home)
            pref->defaultbasedir = strdup (".climm" _OS_PATHSEPSTR);
        else if (home[strlen (home) - 1] != _OS_PATHSEP)
            pref->defaultbasedir = strdup (s_sprintf ("%s%c.climm%c", home, _OS_PATHSEP, _OS_PATHSEP));
        else
            pref->defaultbasedir = strdup (s_sprintf ("%s.climm%c", home, _OS_PATHSEP));
    }
    return pref->defaultbasedir;
}

/*
 * Return the preference base directory
 */
const char *PrefUserDirReal (Preferences *pref)
{
    if (!pref->basedir)
    {
        pref->basedir = strdup (PrefDefUserDirReal (pref));
        if (access (pref->basedir, F_OK))
        {
            char *oldbase = strdup (pref->basedir);
            char *p = strrchr (oldbase, 'c');
            strcpy (p, "micq");
            if (!access (oldbase, F_OK))
                rename (oldbase, pref->basedir);
        }
    }
    return pref->basedir;
}

/*
 * Expand ~ and make absolute path.
 */
const char *PrefRealPath (const char *path)
{
    char *f = NULL;

    if (!*path)
        path = "";
    if (*path == '~' && path[1] == '/' && getenv ("HOME"))
        return s_sprintf ("%s%s", getenv ("HOME"), path + 1);
    if (*path == '/')
        return path;
#ifdef AMIGA
    if (strchr (path, ':') && (!strchr (path, '/') || strchr (path, '/') > strchr (path, ':')))
        return path;
#endif
    path = s_sprintf ("%s%s", PrefUserDir (prG), f = strdup (path));
    free (f);
    if (*path != '~' || path[1] != '/' || !getenv ("HOME"))
        return path;
    path = s_sprintf ("%s%s", getenv ("HOME"), (f = strdup (path)) + 1);
    free (f);
    return path;
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
    FILE *f = NULL;
    if (!pref->rcfile)
    {
        pref->rcfile = strdup (s_sprintf ("%sclimmrc", PrefUserDir (pref)));
        f = fopen (pref->rcfile, "r");
        if (!f)
        {
            char *oldrc = strdup (pref->rcfile);
            char *p = strrchr (oldrc, 'l') - 1;
            strcpy (p, "micqrc");
            if (!access (oldrc, F_OK))
                rename (oldrc, pref->rcfile);
        }
        else
            return f;
        
    }
    
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
    Server *serv;
    Contact *cont;
    int i;
    BOOL ok = TRUE;
    
    pref->away_time = default_away_time;

    rcf = PrefOpenRC (pref);
    if (rcf)
    {
        i = Read_RC_File (rcf);
        if (i >= 2)
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
    
    switch (pref->autoupdate)
    {
        case 0:
            pref->flags &= ~FLAG_AUTOFINGER;
        case 1:
            OptSetVal (&pref->copts, CO_REVEALTIME, 600);
            if (0)
        case 2:
            pref->enc_loc = ENC_AUTO;
        case 3:
            for (i = 0; (serv = ServerNr (i)); i++)
            {
                cont = ContactFindUIN (serv->contacts, 82274703);
                if (cont)
                    OptSetVal (&cont->copts, CO_WANTSBL, 0);
            }
        case 4:
            for (i = 0; (serv = ServerNr (i)); i++)
                OptSetVal (&serv->copts, CO_OBEYSBL, 1);
            pref->autoupdate = 5;
        case 5:
            for (i = 0; (serv = ServerNr (i)); i++)
                OptSetVal (&serv->copts, CO_WANTSBL, 1);
            break;

        case AUTOUPDATE_CURRENT:
        default:
            pref->autoupdate = AUTOUPDATE_CURRENT;
    }
    
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
