
# $Id$ 

# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT([micq], [0.4.99.3])
AC_CONFIG_SRCDIR([include/micq.h])
AC_CONFIG_LIBOBJ_DIR(replace)
AC_CONFIG_HEADER(include/config.h)
AC_CANONICAL_HOST

AC_CHECK_PROG(AUTOMAKE, automake-1.7, automake-1.7)
AC_CHECK_PROG(ACLOCAL, aclocal-1.7, aclocal-1.7)
AC_CHECK_PROG(AUTOHEADER, autoheader, autoheader)
AC_CHECK_PROG(AUTOCONF, autoconf, autoconf)

AM_INIT_AUTOMAKE(1.7)
AM_MAINTAINER_MODE

AC_DEFINE(MICQ_VERSION,VERSION,[mICQ's current version])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S

# Check for arguments.

AC_ARG_ENABLE([peer2peer], AC_HELP_STRING([--disable-peer2peer], [disable peer to peer connections]),
              ac_arg_use_peer2peer=$enableval, ac_arg_use_peer2peer=yes)
AC_ARG_ENABLE([iconv], AC_HELP_STRING([--disable-iconv], [do not use iconv() at all]),
              ac_arg_use_iconv=$enableval, ac_arg_use_iconv=yes)
AC_ARG_ENABLE([iconvrepl], AC_HELP_STRING([--enable-iconvrepl], [compile in iconv() replacement fallbacks]),
              ac_arg_use_iconvrepl=$enableval, ac_arg_use_iconvrepl=no)
AC_ARG_ENABLE([remote], AC_HELP_STRING([--disable-remote], [disable remote control FIFO]),
              ac_arg_use_remote=$enableval, ac_arg_use_remote=auto)

echo "#define ENABLE_FALLBACK_ASCII   1" > include/iconvfb.h
echo "#define ENABLE_FALLBACK_UCS2BE  1" >> include/iconvfb.h
echo "#define ENABLE_FALLBACK_WIN1251 1" >> include/iconvfb.h
echo "#define ENABLE_FALLBACK_KOI8    1" >> include/iconvfb.h
echo "#define ENABLE_FALLBACK_LATIN9  1" >> include/iconvfb.h
echo "#define ENABLE_FALLBACK_LATIN1  1" >> include/iconvfb.h
echo "#define ENABLE_FALLBACK_UTF8    1" >> include/iconvfb.h

if test "x$ac_arg_use_iconv" != "xno"; then
  AM_ICONV
  if test "x$am_cv_func_iconv$ac_arg_use_iconvrepl" = "xyesno"; then
    AC_CACHE_CHECK(for iconv() fallbacks to compile in, ac_cv_iconv_fb,
      [ac_cv_iconv_min="UCS2BE WIN1251 KOI8 LATIN9 LATIN1 UTF8"
       AC_RUN_IFELSE([AC_LANG_PROGRAM([[#include <stdlib.h>
            #include <iconv.h>
            #include <stdio.h>]],
          [[iconv_t resa, resb;
            FILE *hf = fopen ("include/iconvfb.h", "w");
            resa = iconv_open ("UTF-8", "US-ASCII");
            resb = iconv_open ("US-ASCII", "UTF-8");
            if (resa == (iconv_t)-1 || !resa || resb == (iconv_t)-1 || !resb)
                fprintf (hf, "#define ENABLE_FALLBACK_ASCII  1\n");
            else
                fprintf (hf, "#undef  ENABLE_FALLBACK_ASCII\n");
            resa = iconv_open ("UTF-8", "UCS-2BE");
            resb = iconv_open ("UCS-2BE", "UTF-8");
            if (resa == (iconv_t)-1 || !resa || resb == (iconv_t)-1 || !resb)
            {
                resa = iconv_open ("UTF-8", "UNICODEBIG");
                resb = iconv_open ("UNICODEBIG", "UTF-8");
            }
            if (resa == (iconv_t)-1 || !resa || resb == (iconv_t)-1 || !resb)
                fprintf (hf, "#define ENABLE_FALLBACK_UCS2BE  1\n");
            else
                fprintf (hf, "#undef  ENABLE_FALLBACK_UCS2BE\n");
            resa = iconv_open ("UTF-8", "CP1251");
            resb = iconv_open ("CP1251", "UTF-8");
            if (resa == (iconv_t)-1 || !resa || resb == (iconv_t)-1 || !resb)
            {
                resa = iconv_open ("UTF-8", "WINDOWS-1251");
                resb = iconv_open ("WINDOWS-1251", "UTF-8");
            }
            if (resa == (iconv_t)-1 || !resa || resb == (iconv_t)-1 || !resb)
                fprintf (hf, "#define ENABLE_FALLBACK_WIN1251 1\n");
            else
                fprintf (hf, "#undef  ENABLE_FALLBACK_WIN1251\n");
            resa = iconv_open ("UTF-8", "KOI8-U");
            resb = iconv_open ("KOI8-U", "UTF-8");
            if (resa == (iconv_t)-1 || !resa || resb == (iconv_t)-1 || !resb)
                fprintf (hf, "#define ENABLE_FALLBACK_KOI8     1\n");
            else
                fprintf (hf, "#undef  ENABLE_FALLBACK_KOI8\n");
            resa = iconv_open ("UTF-8", "ISO-8859-15");
            resb = iconv_open ("ISO-8859-15", "UTF-8");
            if (resa == (iconv_t)-1 || !resa || resb == (iconv_t)-1 || !resb)
            {
                resa = iconv_open ("UTF-8", "ISO8859-15");
                resb = iconv_open ("ISO8859-15", "UTF-8");
            }
            if (resa == (iconv_t)-1 || !resa || resb == (iconv_t)-1 || !resb)
                fprintf (hf, "#define ENABLE_FALLBACK_LATIN9  1\n");
            else
                fprintf (hf, "#undef  ENABLE_FALLBACK_LATIN9\n");
            resa = iconv_open ("UTF-8", "ISO-8859-1");
            resb = iconv_open ("ISO-8859-1", "UTF-8");
            if (resa == (iconv_t)-1 || !resa || resb == (iconv_t)-1 || !resb)
            {
                resa = iconv_open ("UTF-8", "ISO8859-1");
                resb = iconv_open ("ISO8859-1", "UTF-8");
            }
            if (resa == (iconv_t)-1 || !resa || resb == (iconv_t)-1 || !resb)
                fprintf (hf, "#define ENABLE_FALLBACK_LATIN1  1\n");
            else
                fprintf (hf, "#undef  ENABLE_FALLBACK_LATIN1\n");
            resa = iconv_open ("UTF-8", "UTF-8");
            if (resa == (iconv_t)-1)
                resa = 0;
            else
            {
                size_t inl = 2, outl = 10;
                char inb[10], outb[10], *inp = inb, *outp = outb;
                strcpy (inp, "\xfc.\xc0\xaf");
                if (iconv (resa, &inp, &inl, &outp, &outl) != (size_t)-1)
                    resa = 0;
                else
                {
                    inp = inb + 2;
                    iconv (resa, NULL, NULL, NULL, NULL);
                    if ((iconv (resa, &inp, &inl, &outp, &outl) != (size_t)-1) && *outp != '/')
                        resa = 0;
                }
            }
            if (resa)
                fprintf (hf, "#undef  ENABLE_FALLBACK_UTF8\n");
            else
                fprintf (hf, "#define ENABLE_FALLBACK_UTF8    1\n");
            fclose (hf);
            exit (0);]])],
        [ac_cv_iconv_min="`grep ^#define include/iconvfb.h | sed 's/.*_//;s/1$//'`"],
        [], [])
    ])
  else
    AC_CACHE_CHECK(for iconv() fallbacks to compile in, ac_cv_iconv_fb, [])
  fi
fi

if test "x$ac_arg_use_remote" != "xno"; then
  AC_CACHE_CHECK(for FIFO functionality, ac_cv_fifo_stuff,
    [AC_RUN_IFELSE([AC_LANG_PROGRAM([[#include <stdlib.h>
          #include <sys/types.h>
          #include <stdio.h>
          
          #include <sys/stat.h>
          #include <fcntl.h>
          #include <signal.h>
          void timeout (int i) { exit (11); } ]],
        [[int res, sok, so2;
          char buf[10];
          unlink ("./conftest.fifo");
          signal (SIGALRM, &timeout);
          alarm (5);
          res = mkfifo ("./conftest.fifo", 0600);
          if (res < 0) exit (1);
          sok = open ("./conftest.fifo", O_RDWR | O_NONBLOCK);
          if (sok < 0) exit (2);
          so2 = open ("./conftest.fifo", O_RDONLY | O_NONBLOCK);
          if (so2 < 0) exit (3);
          res = write (sok, "bla", 3);
          if (res < 3) exit (4);
          buf[0] = buf[3] = buf[5] = 0;
          res = read (so2, buf, 3);
          if (res < 3) exit (5);
          if (strcmp (buf, "bla")) exit (6);
          close (sok);
          close (so2);
          unlink ("./conftest.fifo");
          exit (0);]])],
      [ac_cv_fifo_stuff=yes],
      [ac_cv_fifo_stuff=no],
      [ac_cv_fifo_stuff=cross])
  ])
  if test "x$ac_cv_fifo_stuff" != xyes && test "x$ac_cv_fifo_stuff$ac_arg_use_remote" != "crossyes"; then
    ac_arg_use_remote=none
  else
    ac_arg_use_remote=yes
  fi
else
  ac_arg_use_remote=none
fi

AC_MSG_CHECKING([whether to enable peer to peer connections])
AC_MSG_RESULT([$ac_arg_use_peer2peer])
AC_MSG_CHECKING([whether to enable remote control FIFO])
AC_MSG_RESULT([$ac_arg_use_remote])

if test "x$ac_arg_use_peer2peer" = "xyes"; then
  AC_DEFINE(ENABLE_PEER2PEER, 1, [Whether to enable peer to peer connections])
fi

if test "x$ac_arg_use_remote" = "xyes"; then
  AC_DEFINE(ENABLE_REMOTECONTROL, 1, [Whether to enable remote control FIFO])
fi

# Checks for libraries.
AC_CHECK_FUNCS([inet_ntoa getpeername])
if test "x$ac_cv_func_inet_ntoa" = "xno"; then
  AC_CHECK_LIB(nsl, inet_ntoa)
fi
if test "x$ac_cv_func_getpeername" = "xno"; then
  AC_CHECK_LIB(socket, getpeername)
fi

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([locale.h langinfo.h sys/select.h arpa/inet.h ctype.h errno.h fcntl.h limits.h netdb.h netinet/in.h sys/ioctl.h sys/socket.h sys/time.h sys/wait.h termios.h unistd.h sys/utsname.h sys/un.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL

AC_TYPE_SIGNAL

AC_STRUCT_TIMEZONE
AC_CACHE_CHECK(for timezone external, ac_cv_var_timezone,
  [AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <time.h>]], [[int x = - timezone;]])],
      [ac_cv_var_timezone=yes], [ac_cv_var_timezone=no])])
if test "x$ac_cv_var_timezone" = xyes; then
   AC_DEFINE([HAVE_TIMEZONE], 1, [Define if you have the external 'timezone' variable.])
else
  AC_CACHE_CHECK(for tm_gmtoff member, ac_cv_tm_gmtoff,
    [AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <time.h>]], [[ struct tm stamp; stamp.tm_gmtoff = 0; ]])],
      [ac_cv_tm_gmtoff=yes], [ac_cv_tm_gmtoff=no])])
  if test "x$ac_cv_tm_gmtoff" = xyes; then
    AC_DEFINE([HAVE_TM_GMTOFF], 1, [Define if struct tm has a tm_gmt member.])
  fi
fi

AC_FUNC_VPRINTF
AC_FUNC_SNPRINTF
AC_FUNC_MEMMOVE
AC_CHECK_FUNCS([atexit memset memcmp mkdir strcasecmp uname fcntl \
                strchr memchr strdup strncasecmp strpbrk strrchr strstr strtol strtok_r \
                symlink chmod tcgetattr gettimeofday setlocale nl_langinfo select])

for f in memset mkdir atexit \
         strcasecmp strchr memchr strdup strncasecmp strpbrk strrchr strstr strtol; do
    if eval test "x\$ac_cv_func_$f" != "xyes"; then
        AC_MSG_ERROR([Required function $f is missing. Sorry.])
    fi
done

if test "x$ac_cv_func_select" != "xyes"; then
    AC_CHECK_HEADERS([winsock2.h conio.h windef.h])
    my_old_LIBS=$LIBS
    LIBS="-lws2_32 $LIBS"
    AC_CACHE_CHECK(for library containing select, ac_cv_search_select,
      [AC_LINK_IFELSE([AC_LANG_PROGRAM([[/* argl */
          #if HAVE_SYS_TYPES_H
          #include <sys/types.h>
          #endif
          #if TIME_WITH_SYS_TIME
          #include <sys/time.h>
          #include <time.h>
          #elif HAVE_SYS_TIME_H
          #include <sys/time.h>
          #else
          #include <time.h>
          #endif
          #if HAVE_UNISTD_H
          #include <unistd.h>
          #endif
          #if HAVE_SYS_SOCKET_H
          #include <sys/socket.h>
          #endif
          #if HAVE_NETDB_H
          #include <netdb.h>
          #endif
          #if HAVE_WINSOCK2_H
          #include <winsock2.h>
          #endif]],
          [[struct timeval *tm;
            fd_set *fds;
            int *i;
            select (1, fds, fds, fds, tm);
            socket (1, 1, 1);
            gethostbyname ("www.micq.org");
            ioctlsocket(1, 1, i);]])],
        [ac_cv_search_select=ws2_32],
        [ac_cv_search_select=no])])
    LIBS=$my_old_LIBS
    if test x$ac_cv_search_select != xno; then
       AC_DEFINE([ICONV_CONST], const, [Define as const if the declaration of iconv() needs const.])
       LIBS="-lws2_32 $LIBS"
       ac_cv_func_select=yes
       ac_cv_func_socket=yes
       ac_cv_func_gethostbyname=yes
       ac_cv_func_ioctlsocket=yes
       AC_DEFINE(HAVE_SELECT, 1)
       AC_DEFINE(HAVE_SOCKET, 1)
       AC_DEFINE(HAVE_GETHOSTBYNAME, 1)
       AC_DEFINE(HAVE_IOCTLSOCKET, 1)
    else
       AC_MSG_ERROR([Required functions select and/or socket is missing. Sorry.])
    fi
fi

AC_CHECK_FUNCS([select socket gethostbyname hstrerror ioctlsocket])

if test "x$ac_cv_func_gethostbyname" != "xyes"; then
    AC_MSG_WARN([Function gethostbyname() not found; you will need to specify all host names as IP addresses.])
fi

AC_CHECK_DECLS(h_errno,,,[$ac_includes_default
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_WINDEF_H
#include <windef.h>
#endif
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif])

AC_CHECK_TYPES([socklen_t, BOOL, UBYTE, UWORD, UDWORD],,,[$ac_includes_default
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif])

AC_C_BIGENDIAN
AC_CHECK_SIZEOF(char)
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)

if test "x$ac_cv_sizeof_long" = "x4"; then
  AC_DEFINE(SIZE_4_TYPE, long, [Type of length 4 bytes])
elif test "x$ac_cv_sizeof_int" = "x4"; then
  AC_DEFINE(SIZE_4_TYPE, int)
else
  AC_MSG_ERROR([No type of length 4 bytes found.])
fi  

if test "x$ac_cv_sizeof_int" = "x2"; then
  AC_DEFINE(SIZE_2_TYPE, int, [Type of length 2 bytes])
elif test "x$ac_cv_sizeof_short" = "x2"; then
  AC_DEFINE(SIZE_2_TYPE, short)
else
  AC_MSG_ERROR([No type of length 2 bytes found.])
fi  

if test "x$ac_cv_sizeof_short" = "x1"; then
  AC_DEFINE(SIZE_1_TYPE, short, [Type of length 1 bytes])
elif test "x$ac_cv_sizeof_char" = "x1"; then
  AC_DEFINE(SIZE_1_TYPE, char)
else
  AC_MSG_ERROR([No type of length 1 bytes found.])
fi  

# Optimize.

if test "x$GCC" = xyes; then
    CFLAGS="$CFLAGS -Wall"
fi

if test "x$WIP" != "x"; then
    AC_DEFINE(WIP,1,[Include probably buggy work-in-progress code])
fi

if test "x${DEB_HOST_GNU_SYSTEM}" != "x"; then
    AC_DEFINE(__Dbn__,1,"")
else
    AC_DEFINE(__Dbn__,-1)
fi

if test "x$MICQ_EXTRAVERSION" != "x"; then
    AC_DEFINE_UNQUOTED(EXTRAVERSION,"$MICQ_EXTRAVERSION",[Extra version information like the distribution used])
fi

AC_CONFIG_FILES([src/Makefile Makefile lang/Makefile doc/Makefile])
AC_CONFIG_FILES([doc/de/Makefile doc/ru/Makefile doc/fr/Makefile doc/es/Makefile doc/uk/Makefile])
AC_CONFIG_FILES([doc/pt_BR/Makefile doc/sr/Makefile])
AC_OUTPUT
