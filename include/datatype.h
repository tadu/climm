/* $Id$ */

#ifndef MICQ_DATATYPE_H
#define MICQ_DATATYPE_H

#if HAVE_UNISTD_H
   #include <unistd.h>
#endif

#if !HAVE_SOCKLEN_T
typedef unsigned int socklen_t;
#endif

typedef unsigned SIZE_4_TYPE UDWORD;
#ifndef __amigaos__
typedef unsigned SIZE_2_TYPE UWORD;
typedef unsigned SIZE_1_TYPE UBYTE;
#endif
typedef signed   SIZE_4_TYPE SDWORD;
typedef signed   SIZE_2_TYPE SWORD;
typedef signed   SIZE_1_TYPE SBYTE;
typedef signed   SIZE_4_TYPE SINT32;
typedef signed   SIZE_2_TYPE SINT16;
typedef signed   SIZE_1_TYPE SINT8;
typedef unsigned SIZE_4_TYPE UINT32;
typedef unsigned SIZE_2_TYPE UINT16;
typedef unsigned SIZE_1_TYPE UINT8;

typedef int FD_T;
typedef int SOK_T;

#ifdef _WIN32
  typedef unsigned int ssize_t;
  typedef int BOOL;
  #define sockread(s,p,l) recv(s,(char *) p,l,0)
  #define sockwrite(s,p,l) send(s,(char *) p,l,0)
  #define sockclose(s) closesocket(s)

  #define strcasecmp(s,s1)  stricmp(s,s1)
  #define strncasecmp(s,s1,l)  strnicmp(s,s1,l)
  #define Get_Config_Info(x) Get_Unix_Config_Info(x)
  #define __os_has_input _kbhit ()
#else
  #ifndef __amigaos__
    typedef unsigned char BOOL;
  #endif

  #ifdef __BEOS__
    #define sockread(s,p,l) recv(s,p,l,0)
    #define sockwrite(s,p,l) send(s,p,l,0)
    #define sockclose(s) closesocket(s)
    #define __os_has_input 1
  #else
    #define sockread(s,p,l) read(s,p,l)
    #define sockwrite(s,p,l) write(s,p,l)
    #define sockclose(s) close(s)
    #define __os_has_input M_Is_Set (STDIN_FILENO)
  #endif

  #define Get_Config_Info(x) Get_Unix_Config_Info(x)
#endif

#ifndef TRUE
  #define TRUE 1
#endif

#ifndef FALSE
  #define FALSE 0
#endif

#endif /* MICQ_DATATYPE_H */
