/*
 * Provides a build mark with the current version and current time of compilation.
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

#define MICQ_BUILD_NUM 0x00040a03

#include <string.h>
#include <stdio.h>

#include "micq.h"
#include "buildmark.h"
#include "util_str.h"
#include ".cvsupdate"

#ifdef __AMIGA__
#define EXTRAVERSION_DEF "AmigaOS"
#define BUILD_PLATFORM BUILD_PLATFORM_AMIGAOS
#elif defined (__QNX__)
#define EXTRAVERSION_DEF "QNX"
#define BUILD_PLATFORM BUILD_PLATFORM_QNX
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

#ifdef ENABLE_UTF8
#define EV_UTF8 "UTF8 "
#else
#define EV_UTF8
#endif

#ifdef ENABLE_ICONV
#define EV_ICONV "ICONV(" ENABLE_ICONV ") "
#else
#define EV_ICONV
#endif

#define EV EV_P2P EV_TCL EV_UTF8 EV_ICONV

const UDWORD BuildPlatformID = BUILD_PLATFORM;

static const char *ver = 0;

const char *BuildVersion (void)
{
    if (!ver)
        ver = strdup (s_sprintf (i18n (2327, "%smICQ (Matt's ICQ clone)%s version %s%s%s\n"),
            COLSERVER, COLNONE, COLSERVER, MICQ_VERSION, COLNONE));
    return ver;
}

const char *BuildAttribution (void)
{
#if ENABLE_UTF8
    return ("\xc2\xa9 1998,1999,2000 Matthew D. Smith, \xc2\xa9 2001,2002,2003 R\xc3\xbc" "diger Kuhlmann,\n"
            "released under version 2 of the GNU General Public License (GPL).\n");
#else
    return ("\xa9 1998,1999,2000 Matthew D. Smith, \xa9 2001,2002,2003 R\xfc" "diger Kuhlmann,\n"
            "released under version 2 of the GNU General Public License (GPL).\n");
#endif
}                  

const UDWORD BuildVersionNum = MICQ_BUILD_NUM;
const char  *BuildVersionText = "$VER: mICQ " VERSION " " EV EXTRAVERSION " (" CVSUPDATE " build " BUILDDATE ")";

/*
 i18n (1000, "iso-8859-1")       charset used
 i19n (1001, "en")               locale
 i19n (1002, "en_US")            locale 
 i19n (1003, "0-4-10")           MICQ_BUILD_NUM
 i19n (1004, "Ruediger Kuhlmann") all contributors
 i19n (1005, "Ruediger Kuhlmann") last contributor
 i19n (1006, "2002-05-02")       last change
 i19n (1007, "iso-8859-1")       charset used (obsolete)
 */
