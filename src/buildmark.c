/*
 * Provides a build mark with the current version and current time of compilation.
 *
 * mICQ Copyright (C) © 2001-2006 Rüdiger Kuhlmann
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

/*                        0.5.2.90 */
#define MICQ_BUILD_NUM 0x0005025a

#include "micq.h"
#include "buildmark.h"
#include "conv.h"
#include "contact.h"
#include "preferences.h"
#include ".cvsupdate"

#ifdef __AMIGA__
#define EXTRAVERSION_DEF "AmigaOS"
#define BUILD_PLATFORM BUILD_PLATFORM_AMIGAOS
#elif defined (__QNX__)
#define EXTRAVERSION_DEF "QNX"
#define BUILD_PLATFORM BUILD_PLATFORM_QNX
#elif defined (__APPLE__)
#define EXTRAVERSION_DEF "Apple Mac OS X"
#define BUILD_PLATFORM BUILD_PLATFORM_MACOSX
#elif defined (__BEOS__)
#define EXTRAVERSION_DEF "BeOS"
#define BUILD_PLATFORM BUILD_PLATFORM_BEOS
#elif defined (__Cygwin__)
#define EXTRAVERSION_DEF "Cygwin"
#define BUILD_PLATFORM BUILD_PLATFORM_CYGWIN
#elif defined (_WIN32)
#define EXTRAVERSION_DEF "Win32"
#define BUILD_PLATFORM BUILD_PLATFORM_WINDOWS
#elif defined (__OpenBSD__)
#define EXTRAVERSION_DEF "OpenBSD"
#define BUILD_PLATFORM BUILD_PLATFORM_OPENBSD
#elif defined (__NetBSD__)
#define EXTRAVERSION_DEF "NetBSD"
#define BUILD_PLATFORM BUILD_PLATFORM_NETBSD
#elif defined (__FreeBSD__)
#define EXTRAVERSION_DEF "FreeBSD"
#define BUILD_PLATFORM BUILD_PLATFORM_FREEBSD
#elif defined (_AIX)
#define EXTRAVERSION_DEF "AIX"
#define BUILD_PLATFORM BUILD_PLATFORM_AIX
#elif defined (__hpux)
#define EXTRAVERSION_DEF "HPUX"
#define BUILD_PLATFORM BUILD_PLATFORM_HPUX
#elif defined (__sun__) || defined (sun)
#define EXTRAVERSION_DEF "Solaris"
#define BUILD_PLATFORM BUILD_PLATFORM_SOLARIS
#elif __Dbn__ != -1
#define EXTRAVERSION_DEF "De" "bi" "an"
#define BUILD_PLATFORM BUILD_PLATFORM_DEBIAN
#elif defined (__linux__)
#define EXTRAVERSION_DEF "Linux"
#define BUILD_PLATFORM BUILD_PLATFORM_LINUX
#elif defined (__unix__)
#define EXTRAVERSION_DEF "Unix"
#define BUILD_PLATFORM BUILD_PLATFORM_UNIX
#else
#define EXTRAVERSION_DEF "unknown"
#define BUILD_PLATFORM 0x00000000
#endif

#ifndef EXTRAVERSION
#define EXTRAVERSION EXTRAVERSION_DEF " hand compiled"
#endif

#ifdef ENABLE_AUTOPACKAGE
#define EV_AP "AP "
#else
#define EV_AP
#endif

#ifdef ENABLE_PEER2PEER
#define EV_P2P "P2P "
#else
#define EV_P2P
#endif

#ifdef ENABLE_TCL
#define EV_TCL "TCL "
#else
#define EV_TCL
#endif

#ifdef ENABLE_SSL
#define EV_SSL "SSL "
#else
#define EV_SSL
#endif

#ifdef ENABLE_XMPP
#define EV_XMPP "XMPP "
#else
#define EV_XMPP
#endif

#ifdef ENABLE_MSN
#define EV_MSN "MSN "
#else
#define EV_MSN
#endif

#define EV EV_AP EV_SSL EV_TCL EV_P2P EV_XMPP EV_MSN

const UDWORD BuildPlatformID = BUILD_PLATFORM;
const char  *BuildPlatformStr = EXTRAVERSION_DEF;

const char *BuildVersion (void)
{
    return s_sprintf (i18n (2327, "%smICQ (Matt's ICQ clone)%s version %s%s%s\n"),
            COLSERVER, COLNONE, COLSERVER, MICQ_VERSION " (" CVSUPDATE ")", COLNONE);
}

const char *BuildAttribution (void)
{
    char *name;
    const char *full;
    
    name = strdup (s_sprintf ("%s 1998-2000 %sMatthew D. Smith%s, %s 2001-2005 %sR%sdiger Kuhlmann%s.\n",
                   ConvTranslit ("\xc2\xa9", "(c)"), COLQUOTE, COLNONE,
                   ConvTranslit ("\xc2\xa9", "(c)"), COLQUOTE,
                   ConvTranslit ("\xc3\xbc", "ue"), COLNONE));
    full = s_sprintf (i18n (2574, "%sReleased under version 2 of the GNU General Public License (%sGPL v2%s).\n"),
                   name, COLQUOTE, COLNONE);
    free (name);
    return full;
}                  

const UDWORD BuildVersionNum = MICQ_BUILD_NUM;
const char *BuildVersionText = "$VER: mICQ " VERSION " " EV EXTRAVERSION "\n(" CVSUPDATE " build " BUILDDATE ")";
const char *BuildVersionStr  = VERSION MICQ_IS_CVS;

/*
 i18n (1000, "UTF-8")            charset used
 i19n (1001, "en")               locale
 i19n (1002, "en_US")            locale 
 i19n (1003, "0-4-10")           MICQ_BUILD_NUM
 i19n (1004, "Ruediger Kuhlmann") all contributors
 i19n (1005, "Ruediger Kuhlmann") last contributor
 i19n (1006, "2002-05-02")       last change
 i19n (1007, "iso-8859-1")       charset used (obsolete)
 i19n (1008, "US-ASCII")         default language's charset
 */
