/*
 * This file contains static string helper functions.
 *
 * This file is Copyright © Rüdiger Kuhlmann; it may be distributed under
 * version 2 of the GPL licence.
 *
 * $Id$
 */

#include "micq.h"
#include <stdarg.h>
#include <netinet/in.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include "preferences.h"

/*
 * Return a static formatted string.
 */
const char *s_sprintf (const char *fmt, ...)
{
    static char buf[1024];
    va_list args;

    va_start (args, fmt);
    vsnprintf (buf, sizeof (buf), fmt, args);
    va_end (args);

    return buf;
}

/*
 * Return a static string consisting of the given IP.
 */
const char *s_ip (UDWORD ip)
{
    static char buf[20];
    struct sockaddr_in sin;
    sin.sin_addr.s_addr = htonl (ip);
    snprintf (buf, sizeof (buf), "%s", inet_ntoa (sin.sin_addr));
    return buf;
}

/*
 * Return a static string describing the status.
 */
const char *s_status (UDWORD status)
{
    static char buf[200];
    
    if (status == STATUS_OFFLINE)
        return i18n (1969, "offline");
 
    if (status & STATUSF_INV)
        snprintf (buf, sizeof (buf), "%s-", i18n (1975, "invisible"));
    else
        buf[0] = '\0';
    
    if (status & STATUSF_DND)
        snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), "%s", i18n (1971, "do not disturb"));
    else if (status & STATUSF_OCC)
        snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), "%s", i18n (1973, "occupied"));
    else if (status & STATUSF_NA)
        snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), "%s", i18n (1974, "not available"));
    else if (status & STATUSF_AWAY)
        snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), "%s", i18n (1972, "away"));
    else if (status & STATUSF_FFC)
        snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), "%s", i18n (1976, "free for chat"));
    else
        snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), "%s", i18n (1970, "online"));
    
    if (prG->verbose)
        snprintf (buf + strlen (buf), sizeof(buf) - strlen (buf), " %08lx", status);
    
    return buf;
}

/*
 * Returns static string describing the given time.
 */
const char *s_time (time_t *stamp)
{
    struct timeval p = {0L, 0L};
    struct tm now;
    struct tm *thetime;
    static char tbuf[40];

#ifdef HAVE_GETTIMEOFDAY
    if (gettimeofday (&p, NULL) == -1)
#endif
    {
        p.tv_usec = 0L;
        p.tv_sec = time (NULL);
    }

    now = *localtime (&p.tv_sec);

    thetime = (!stamp || *stamp == NOW) ? &now : localtime (stamp);

    strftime(tbuf, sizeof (tbuf), thetime->tm_year == now.tm_year 
        && thetime->tm_mon == now.tm_mon && thetime->tm_mday == now.tm_mday
        ? "%X" : "%a %b %d %X %Y", thetime);

    if (prG->verbose > 7)
        snprintf (tbuf + strlen (tbuf), sizeof (tbuf) - strlen (tbuf), ".%.06ld", p.tv_usec);
    else if (prG->verbose > 1)
        snprintf (tbuf + strlen (tbuf), sizeof (tbuf) - strlen (tbuf), ".%.03ld", p.tv_usec / 1000);
    
    return tbuf;
}

