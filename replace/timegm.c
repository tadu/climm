/*
 * Replacement time conversion functions for non-standard timegm ().
 * Replacement function timegm is copied from Linux timegm man page.
 */

#include "micq.h"

time_t portable_timegm (struct tm *tm)
{
    time_t ret;
    char *tz;

    tz = getenv ("TZ");
    setenv ("TZ", "", 1);
    tzset ();
    ret = mktime (tm);
    if (tz)
        setenv ("TZ", tz, 1);
    else
        unsetenv ("TZ");
    tzset ();
    return ret;
}
