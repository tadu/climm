/*
 * Provides a build mark with the current version and current time of compilation.
 *
 * This file is Copyright © Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 */

#define MICQ_BUILD_NUM 486

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
        
        snprintf (buf, sizeof (buf), COLSERV "%s" COLNONE " %s " COLSERV MICQ_VERSION COLNONE " (%s " \
                                     __DATE__ " " __TIME__ ")\n%s\n",
                  i18n (74, "mICQ (Matt's ICQ clone)"),
                  i18n (75, "version"),
                  i18n (76, "compiled on"),
                  i18n (77, "in dedication to Matthew D. Smith."));
        ver = strdup (buf);
    }
    return ver;
}

const int BuildVersionNum = MICQ_BUILD_NUM;

/*
 i18n (1, "en")               locale
 i18n (2, "en_US")            locale 
 i18n (3, "486")              MICQ_BUILD_NUM
 i18n (4, "Rüdiger Kuhlmann") all contributors
 i18n (5, "Rüdiger Kuhlmann") last contributor
 i18n (6, "2002-05-02")       last change
 i18n (7, "iso-8859-1")       charset used. must be iso-8859-1 if possible


 */
