/*
 * Provides a build mark with the current version and current time of compilation.
 *
 * This file is Copyright © Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 */

#define MICQ_BUILD_NUM 0x80040a00

#include <string.h>
#include <stdio.h>

#include "micq.h"
#include "buildmark.h"
#include "util_str.h"
#include ".cvsupdate"

#ifndef EXTRAVERSION
#ifdef __AMIGA__
#define EXTRAVERSION "AmigaOS hand compiled"
#elif defined (__BEOS__)
#define EXTRAVERSION "BeOS hand compiled"
#elif defined (_WIN32)
#define EXTRAVERSION "Win32 hand compiled"
#elif defined (__OpenBSD__)
#define EXTRAVERSION "OpenBSD hand compiled"
#elif defined (__NetBSD__)
#define EXTRAVERSION "NetBSD hand compiled"
#elif defined (__FreeBSD__)
#define EXTRAVERSION "FreeBSD hand compiled"
#elif __Dbn__ != -1
#define EXTRAVERSION "De" "bi" "an hand compiled"
#elif defined (__linux__)
#define EXTRAVERSION "Linux hand compiled"
#elif defined (__Cygwin__)
#define EXTRAVERSION "Cygwin hand compiled"
#else
#define EXTRAVERSION "hand compiled"
#endif
#endif

#ifdef ENABLE_PEER2PEER
#define EV_P2P "P2P "
#else
#define EV_P2P
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

#define EV EV_P2P EV_UTF8 EV_ICONV

static const char *ver = 0;

const char *BuildVersion (void)
{
    if (!ver)
        ver = strdup (s_sprintf (i18n (2210, "%smICQ (Matt's ICQ clone)%s version %s%s%s (compiled on %s)\n"),
            COLSERVER, COLNONE, COLSERVER, MICQ_VERSION "CVS " CVSUPDATE, COLNONE, BUILDDATE));
    return ver;
}

const char *BuildAttribution (void)
{
    return (s_sprintf ("%s\n", i18n (1077, "in dedication to Matthew D. Smith.")));
}                  

const UDWORD BuildVersionNum = MICQ_BUILD_NUM;
const char  *BuildVersionText = "$VER: mICQ " VERSION " " EV EXTRAVERSION " (" CVSUPDATE " build " BUILDDATE ")";

/*
 i19n (1000, "iso-8859-1")       charset used
 i19n (1001, "en")               locale
 i19n (1002, "en_US")            locale 
 i19n (1003, "0-4-10")           MICQ_BUILD_NUM
 i19n (1004, "Ruediger Kuhlmann") all contributors
 i19n (1005, "Ruediger Kuhlmann") last contributor
 i19n (1006, "2002-05-02")       last change
 i19n (1007, "iso-8859-1")       charset used (obsolete)
 */
