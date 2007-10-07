
#include "climm.h"
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
#if defined (HAVE_CTYPE_H)
#include <ctype.h>
#endif

static int console_idle (time_t now, struct utmp *u)
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
        return MAXINT;

    if (stat (tty, &sbuf) != 0)
        return MAXINT;

    return now - sbuf.st_atime;
}
  
/*
  Return minimum idle time from all consoles of user owing this process.
*/
UDWORD os_DetermineIdleTime (time_t now, time_t last)
{
    struct utmp *u;
    struct passwd *pass;
    uid_t uid;
    int tmp, min = MAXINT;

    uid = geteuid ();
    pass = getpwuid (uid);

    if (!pass)
        return -1;

    utmpname (UTMP_FILE);
    setutent();
    while ((u = getutent()))
    {
        if (u->ut_type != USER_PROCESS)
            continue;

        if (strncmp (u->ut_user, pass->pw_name, UT_NAMESIZE))
            continue;
        
        tmp = console_idle (now, u);
        min = (tmp < min) ? tmp : min ;
    }
    endutent();
    if (min == MAXINT || now - last < min)
        min = now - last;
    return min;
}
