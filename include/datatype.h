#ifndef __MATT_DATATYPE__
#define __MATT_DATATYPE__
#if HAVE_UNISTD_H
   #include <unistd.h>
#endif

typedef unsigned long  UDWORD;
#ifndef __amigaos__
typedef unsigned short UWORD;
typedef unsigned char  UBYTE;
#endif
typedef signed long    SDWORD;
typedef signed short   SWORD;
typedef signed char    SBYTE;
typedef signed long    SINT32;
typedef signed short   SINT16;
typedef signed char    SINT8;
typedef unsigned long  UINT32;
typedef unsigned short UINT16;
typedef unsigned char  UIN8;

#ifdef _WIN32
#define 	snprintf 	_snprintf
#define 	vsnprintf 	_vsnprintf

  typedef int FD_T;
  typedef int SOK_T;
  typedef unsigned int ssize_t;
  typedef int BOOL;
  #define sockread(s,p,l) recv(s,(char *) p,l,0)

/* use SOCKWRITE !!!!! */
  #define sockwrite(s,p,l) send(s,(char *) p,l,0)
  #define sockclose(s) closesocket(s)
  #define strcasecmp(s,s1)  stricmp(s,s1)
  #define strncasecmp(s,s1,l)  strnicmp(s,s1,l)
  #define Get_Config_Info(x) Get_Unix_Config_Info(x)
#else
  #ifndef __amigaos__
    typedef unsigned char BOOL;
  #endif

  #ifdef __BEOS__
    #define sockread(s,p,l) recv(s,p,l,0)
  #else
    #define sockread(s,p,l) read(s,p,l)
  #endif
  /* use SOCKWRITE !!!!! */
  #ifdef __BEOS__
    #define sockwrite(s,p,l) send(s,p,l,0)
  #else
    #define sockwrite(s,p,l) write(s,p,l)
  #endif

  #ifdef __BEOS__
    #define sockclose(s) closesocket(s)
  #else
    #define sockclose(s) close(s)
  #endif
  #define Get_Config_Info(x) Get_Unix_Config_Info(x)
  typedef int FD_T;
  typedef int SOK_T;
#endif

#ifndef TRUE
  #define TRUE 1
#endif

#ifndef FALSE
  #define FALSE 0
#endif

#endif /* Matt's datatype */
