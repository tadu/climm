/* $Id$ */

/*********************************************
**********************************************
This is for extensions to the select() function/

This software is provided AS IS to be used in
whatever way you see fit and is placed in the
public domain.
Copyright: This file may be distributed under version 2 of the GPL licence.

Author : Matthew Smith March 5, 1999
Contributors : 
Changes :
     3-5-99 Created
**********************************************
**********************************************/
#include "micq.h"
#include "mselect.h"

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#if HAVE_WINSOCK2_H
#include <winsock2.h>
#endif

#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>


static struct timeval tv;
static fd_set readfds;
static fd_set writefds;
static fd_set exceptfds;
static int max_fd;

void M_select_init (void)
{
    FD_ZERO (&readfds);
    FD_ZERO (&writefds);
    FD_ZERO (&exceptfds);
    max_fd = 0;
}

void M_set_timeout (UDWORD sec, UDWORD usec)
{
    tv.tv_sec = sec;
    tv.tv_usec = usec;
}

void M_Add_rsocket (FD_T sok)
{
    FD_SET (sok, &readfds);
    if (sok > max_fd)
        max_fd = sok;
}

void M_Add_wsocket (FD_T sok)
{
    FD_SET (sok, &writefds);
    if (sok > max_fd)
        max_fd = sok;
}

void M_Add_xsocket (FD_T sok)
{
    FD_SET (sok, &exceptfds);
    if (sok > max_fd)
        max_fd = sok;
}

BOOL M_Is_Set (FD_T sok)
{
    return ((FD_ISSET (sok, &readfds)   ? 1 : 0) 
          | (FD_ISSET (sok, &writefds)  ? 2 : 0)
          | (FD_ISSET (sok, &exceptfds) ? 4 : 0));
}

int M_select (void)
{
    int res, rc;

    errno = 0;
    res = select (max_fd + 1, &readfds, &writefds, &exceptfds, &tv);
    rc = errno;
    if (res == -1)
    {
        FD_ZERO (&readfds);
        FD_ZERO (&writefds);
        FD_ZERO (&exceptfds);
        if (rc == EINTR)
            return 0;
        printf (i18n (1849, "Error on select: %s (%d)\n"), strerror (rc), rc);
        return -1;
    }
    return res;
}
