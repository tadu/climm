/* $Id$ */

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
                  i18n (74, "Micq (Matt's ICQ clone)"),
                  i18n (75, "version"),
                  i18n (76, "compiled on"),
                  i18n (77, "in dedication to Matthew D. Smith."));
        ver = strdup (buf);
    }
    return ver;
}
