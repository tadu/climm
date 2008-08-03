
#include "climm.h"
#include <limits.h>   /* INT_MAX */
#if defined (HAVE_VALUES_H)
#include <values.h>
#endif
#if defined (HAVE_SYS_STAT_H)
#include <sys/stat.h>
#endif
#if defined (HAVE_PWD_H)
#include <pwd.h>
#endif
#include <unistd.h>
#if defined (HAVE_UTMP_H)
#include <utmp.h>
#endif
#if defined (HAVE_UTMPX_H)
#include <utmpx.h>
#endif
#if defined (HAVE_CTYPE_H)
#include <ctype.h>
#endif

#if HAVE_GETUTXENT
static int console_idle (time_t now, struct utmpx *u)
#else
static int console_idle (time_t now, struct utmp *u)
#endif
{
    struct stat sbuf;
    char tty[5 + sizeof u->ut_line + 1] = "/dev/";
    int i;

    for (i=0; i < sizeof u->ut_line; i++)      /* clean up tty if garbled */
        if (isalnum (u->ut_line[i]) || (u->ut_line[i] == '/'))
            tty[i + 5] = u->ut_line[i];
        else
            tty[i + 5] = '\0';

    if (*u->ut_line == ':')
        return INT_MAX;

    if (stat (tty, &sbuf) != 0)
        return INT_MAX;

    return now - sbuf.st_atime;
}
  
/*
  Return minimum idle time from all consoles of user owning this process.
*/
UDWORD os_DetermineIdleTime (time_t now, time_t last)
{
#if HAVE_GETUTXENT
    struct utmpx *u;
#else
    struct utmp *u;
#endif
    struct passwd *pass;
    uid_t uid;
    int tmp, min = INT_MAX;

    uid = geteuid ();
    pass = getpwuid (uid);

    if (!pass)
        return -1;

#if HAVE_GETUTXENT
    setutxent();
    while ((u = getutxent()))
#else
    setutent();
    while ((u = getutent()))
#endif
    {
        if (u->ut_type != USER_PROCESS)
            continue;

#ifdef UT_NAMESIZE
        if (strncmp (u->ut_user, pass->pw_name, UT_NAMESIZE))
#else
        if (strncmp (u->ut_user, pass->pw_name, sizeof (u->ut_user)))
#endif
            continue;
        
        tmp = console_idle (now, u);
        min = (tmp < min) ? tmp : min ;
    }
#if HAVE_GETUTXENT
    endutxent();
#else
    endutent();
#endif
    if (min == INT_MAX || now - last < min)
        min = now - last;
    return min;
}
