/*
 * Provides a build mark with the current version and current time of compilation.
 *
 * This file is Copyright © Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 */

#define MICQ_BUILD_NUM 0x00040903

#include <string.h>
#include <stdio.h>

#include "micq.h"
#include "buildmark.h"

static const char *ver = 0;

const char *BuildVersion (void)
{
    if (!ver)
    {
        char buf[2048];
        
        snprintf (buf, sizeof (buf), COLSERVER "%s" COLNONE " %s " COLSERVER MICQ_VERSION COLNONE " (%s " \
                                     __DATE__ " " __TIME__ ")\n%s\n",
                  i18n (1074, "mICQ (Matt's ICQ clone)"),
                  i18n (1075, "version"),
                  i18n (1076, "compiled on"),
                  i18n (1077, "in dedication to Matthew D. Smith."));
        ver = strdup (buf);
    }
    return ver;
}

const UDWORD BuildVersionNum = MICQ_BUILD_NUM;
const char  *BuildVersionText = "$VER: mICQ " VERSION " Official-0-4-9-3 (" __DATE__ " " __TIME__ ")";

/*
 i19n (1001, "en")               locale
 i19n (1002, "en_US")            locale 
 i19n (1003, "0-4-9-3")          MICQ_BUILD_NUM
 i19n (1004, "Rüdiger Kuhlmann") all contributors
 i19n (1005, "Rüdiger Kuhlmann") last contributor
 i19n (1006, "2002-05-02")       last change
 i18n (1007, "iso-8859-1")       charset used. must be iso-8859-1 if possible
 */
