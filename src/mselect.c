/*********************************************
**********************************************
This is for extensions to the select() function/

This software is provided AS IS to be used in
whatever way you see fit and is placed in the
public domain.

Author : Matthew Smith March 5, 1999
Contributors : 
Changes :
     3-5-99 Created
**********************************************
**********************************************/
#include "micq.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#ifdef _WIN32
    #include <conio.h>
    #include <io.h>
    #include <winsock2.h>
    #include <time.h>
#else
    #include <unistd.h>
    #include <netinet/in.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <sys/socket.h>

    #ifndef __BEOS__
        #include <arpa/inet.h>
    #endif

    #include <netdb.h>
    #include <sys/time.h>
    #include <sys/wait.h>
#endif

#include <fcntl.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>



static struct timeval tv;
static fd_set readfds;
static int max_fd;

void M_select_init( void )
{
      FD_ZERO(&readfds);
      max_fd = 0;
}

void M_set_timeout( DWORD sec, DWORD usec )
{
   tv.tv_sec = sec;
   tv.tv_usec = usec;
}

void M_Add_rsocket( FD_T sok )
{
      FD_SET(sok, &readfds);
      if ( sok > max_fd )
         max_fd = sok;
}

BOOL M_Is_Set( FD_T sok )
{
   return (FD_ISSET(sok, &readfds));
}

int M_select( void )
{
      /* don't care about writefds and exceptfds: */
      return select( max_fd+1, &readfds, NULL, NULL, &tv);
}
