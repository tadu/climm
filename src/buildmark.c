/*
 * Provides a build mark with the current version and current time of compilation.
 *
 * This file is Copyright © Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 */

#define MICQ_BUILD_NUM 0x00040904

#include <string.h>
#include <stdio.h>

#include "micq.h"
#include "buildmark.h"
#include "util_str.h"
#include ".cvsupdate"

static const char *ver = 0;

const char *BuildVersion (void)
{
    if (!ver)
        ver = strdup (s_sprintf (i18n (2210, "%smICQ (Matt's ICQ clone)%s version %s%s%s (compiled on %s)\n"),
            COLSERVER, COLNONE, COLSERVER, MICQ_VERSION " cvs " CVSUPDATE, COLNONE, BUILDDATE));
    return ver;
}

const char *BuildAttribution (void)
{
    return (s_sprintf ("%s\n", i18n (1077, "in dedication to Matthew D. Smith.")));
}                  

const UDWORD BuildVersionNum = MICQ_BUILD_NUM;
const char  *BuildVersionText = "$VER: mICQ " VERSION " CVS " EXTRAVERSION " (" CVSUPDATE " build " BUILDDATE ")";

/*
 i19n (1001, "en")               locale
 i19n (1002, "en_US")            locale 
 i19n (1003, "0-4-9-4")          MICQ_BUILD_NUM
 i19n (1004, "Rüdiger Kuhlmann") all contributors
 i19n (1005, "Rüdiger Kuhlmann") last contributor
 i19n (1006, "2002-05-02")       last change
 i18n (1007, "iso-8859-1")       charset used. must be iso-8859-1 if possible
 */
