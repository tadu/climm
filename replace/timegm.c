/*
 * Replacement time conversion functions for non-standard timegm ().
 * Replacement function timegm is copied from Linux timegm man page.
 */

#include "climm.h"

#if HAVE_SETENV && HAVE_UNSETENV
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
#elif HAVE_PUTENV && HAVE_UNSETENV
time_t portable_timegm (struct tm *tm)
{
    static char *envblank = NULL;
    static char *envset = NULL;
    time_t ret;
    char *tz;

    if (!envblank)
        envblank = strdup ("TZ=");
    if (!envset)
    {
        tz = getenv ("TZ"); /* climm doesn't change TZ */
        envset = malloc (strlen (tz) + 4);
        strcpy (envset, "TZ=");
        strcat (envset, tz);
    }
    putenv (envblank);

    tzset ();
    ret = mktime (tm);
    if (tz)
        putenv (envset);
    else
        unsetenv ("TZ");
    tzset ();
    return ret;
}
#else
time_t portable_timegm (struct tm *tm)
{
    struct tm stamp;
#if HAVE_TIMEZONE
    stamp = *tm;
    stamp.tm_sec -= timezone;
    stamp.tm_isdst = 0;
#elif HAVE_TM_GMTOFF
    time_t now = time (NULL);
    unsigned long s;
    stamp = *localtime (&now);
    s = -stamp.tm_gmtoff;
    stamp = *tm;
    stamp.tm_sec += s;
#else
    stamp = *tm;    
#endif
    return mktime (&stamp);
}
#endif
