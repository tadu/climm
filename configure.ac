
# $Id$ 

# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT([micq], [0.4.12])
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
AC_SYS_LARGEFILE

# Check for arguments.

AC_ARG_ENABLE([peer2peer], AC_HELP_STRING([--disable-peer2peer], [disable peer to peer connections]),
              ac_arg_use_dc=$enableval, ac_arg_use_dc=yes)
AC_ARG_ENABLE([utf8], AC_HELP_STRING([--disable-utf8], [disable use of UTF-8 internally]),
              ac_arg_use_utf8=$enableval, ac_arg_use_utf8=yes)
AC_ARG_ENABLE([iconv], AC_HELP_STRING([--disable-iconv], [do not use iconv() at all]),
              ac_arg_use_iconv=$enableval, ac_arg_use_iconv=auto)
AC_ARG_ENABLE([remote], AC_HELP_STRING([--disable-remote], [disable remote control FIFO]),
              ac_arg_use_remote=$enableval, ac_arg_use_remote=auto)
AC_ARG_ENABLE([tcl], AC_HELP_STRING([--disble-tcl], [disable Tcl scripting (EXPERIMENTAL)]),
              ac_arg_use_tcl=$enableval, ac_arg_use_tcl=auto)
AC_ARG_ENABLE([ssl], AC_HELP_STRING([--disable-ssl], [disable SSL encryption (EXPERIMENTAL)]),
              [ac_arg_use_ssl=$enableval], ac_arg_use_ssl=auto)
AC_ARG_ENABLE([ssl], AC_HELP_STRING([[[[--enable-ssl=gnutls,openssl]]]], [choose SSL library]))

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
AC_CHECK_HEADERS([locale.h sys/select.h arpa/inet.h ctype.h errno.h fcntl.h limits.h netdb.h netinet/in.h sys/ioctl.h sys/socket.h sys/time.h sys/wait.h termios.h unistd.h sys/utsname.h sys/un.h])

if test "x$ac_arg_use_dc" != "xno"; then
  if test "x$ac_arg_use_ssl" != "xno"; then
    ac_use_ssl=no
    if test "x$ac_arg_use_ssl" != "xopenssl"; then
      AM_PATH_LIBGNUTLS(0.8.8, ac_use_ssl=gnutls)
      if test "x$ac_use_ssl" = "xgnutls"; then
        LIBS="$LIBGNUTLS_LIBS $LIBS"
        CFLAGS="$LIBGNUTLS_CFLAGS $CFLAGS"
        AC_CHECK_FUNC(gnutls_dh_params_generate2, [AC_DEFINE(HAVE_DH_GENPARAM2, 1, [Whether new DH prime generator is available])],)
      fi
    fi
    if test "x$ac_use_ssl" = "xno"; then
      if test "x$ac_arg_use_ssl" != "xgnutls"; then
        AC_CHECK_HEADERS([openssl/ssl.h], [ac_use_ssl=openssl])
        temp_LIBS="$LIBS"
        AC_CHECK_LIB([crypto], [DH_free], , [ac_use_ssl=no])
        AC_CHECK_LIB([ssl], [SSL_new], , [ac_use_ssl=no
                                          LIBS="$temp_LIBS"])
      fi
    fi
    if test "x$ac_use_ssl" = "xno"; then
      case "$ac_arg_use_ssl" in
        gnutls)  AC_MSG_FAILURE([cannot find GnuTLS library]) ;;
        openssl) AC_MSG_FAILURE([cannot find OpenSSL library]) ;;
        yes)     AC_MSG_FAILURE([cannot find any SSL library]) ;;
        *)       AC_MSG_WARN([cannot find a suitable SSL library -- encrypted connection support disabled]) ;;
      esac
    fi
    ac_arg_use_ssl=$ac_use_ssl
  fi
else
  ac_arg_use_ssl=no
fi

if test "x$ac_arg_use_tcl" != "xno"; then
  ac_use_tcl=no
  AC_CHECK_HEADERS([tcl8.4/tcl.h tcl8.3/tcl.h tcl.h], [ac_use_tcl=yes
                                                        break])
  if test "x$ac_use_tcl" = "xyes"; then
    AC_SEARCH_LIBS([Tcl_CreateInterp], [tcl8.4 tcl8.3], [ac_use_tcl=yes], [ac_use_tcl=no])
  fi
  if test "x$ac_use_tcl$ac_arg_use_tcl" = "xnoyes"; then
    AC_MSG_FAILURE([cannot find a suitable Tcl interpreter library])
  fi
  if test "x$ac_use_tcl$ac_arg_use_tcl" = "xnoauto"; then
    AC_MSG_WARN([cannot find a suitable Tcl interpreter library -- TCL scripting disabled])
  fi
  ac_arg_use_tcl=$ac_use_tcl
fi

if test "x$ac_arg_use_utf8" = "xno"; then
  ac_arg_use_iconv=no
fi

if test "x$ac_arg_use_iconv" != "xno"; then
  AC_CHECK_FUNC([iconv], [ac_arg_use_iconv=system], [ac_arg_use_iconv=builtin])
  if test "x$ac_arg_use_iconv" = "xbuiltin"; then
    AC_CHECK_LIB([iconv], [iconv], [ac_arg_use_iconv=iconv])
    if test "x$ac_arg_use_iconv" = "xbuiltin"; then
      AC_CHECK_LIB([iconv], [libiconv], [ac_arg_use_iconv=libiconv])
    fi
    if test "x$ac_arg_use_iconv" != "xbuiltin"; then
      LIBS="-liconv $LIBS"
    fi
  fi
else
  ac_arg_use_iconv=builtin
fi

if test "x$ac_arg_use_iconv" != "xbuiltin"; then
  AC_CACHE_CHECK(for iconv minimal functionality, ac_cv_iconv_min,
    [AC_RUN_IFELSE([AC_LANG_PROGRAM([[#include <stdlib.h>
          #include <iconv.h>]],
        [[iconv_t res;
          res = iconv_open ("UTF-8", "ISO-8859-1");
          if (res == (iconv_t)-1 || !res)
          {
              res = iconv_open ("UTF-8", "ISO8859-1");
              if (res == (iconv_t)-1 || !res) exit (2);
              res = iconv_open ("ISO8859-1", "UTF-8");
              if (res == (iconv_t)-1 || !res) exit (3);
          }
          else
          {
              res = iconv_open ("ISO-8859-1", "UTF-8");
              if (res == (iconv_t)-1 || !res) exit (4);
          }
          res = iconv_open ("UTF-8", "ISO-8859-15");
          if (res == (iconv_t)-1 || !res)
          {
              res = iconv_open ("UTF-8", "ISO8859-15");
              if (res == (iconv_t)-1 || !res) exit (5);
              res = iconv_open ("ISO8859-15", "UTF-8");
              if (res == (iconv_t)-1 || !res) exit (6);
          }
          else
          {
              res = iconv_open ("ISO-8859-15", "UTF-8");
              if (res == (iconv_t)-1 || !res) exit (7);
          }
          res = iconv_open ("UTF-8", "KOI8-U");
          if (res == (iconv_t)-1 || !res) exit (8);
          res = iconv_open ("KOI8-U", "UTF-8");
          if (res == (iconv_t)-1 || !res) exit (9);
          res = iconv_open ("UTF-8", "UCS-2BE");
          if (res == (iconv_t)-1 || !res)
          {
              res = iconv_open ("UTF-8", "UNICODEBIG");
              if (res == (iconv_t)-1 || !res) exit (10);
              res = iconv_open ("UNICODEBIG", "UTF-8");
              if (res == (iconv_t)-1 || !res) exit (11);
          }
          else
          {
              res = iconv_open ("UCS-2BE", "UTF-8");
              if (res == (iconv_t)-1 || !res) exit (12);
          }
          exit (0);]])],
      [ac_cv_iconv_min=ok],
      [ac_cv_iconv_min=broken],
      [ac_cv_iconv_min=cross])
  ])
  if test $ac_cv_iconv_min != ok; then
    ac_arg_use_iconv=builtin
  else
    AC_CACHE_CHECK(for iconv declaration, ac_cv_iconv_const,
      [AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <stdlib.h>
          #include <iconv.h>]],
          [[#ifdef __cplusplus
            "C"
            #endif
            #if defined(__STDC__) || defined(__cplusplus)
            size_t iconv (iconv_t cd, char * *inbuf, size_t *inbytesleft, char * *outbuf, size_t *outbytesleft);
            #else
            size_t iconv();
            #endif]])],
        [ac_cv_iconv_const=no-const],
        [ac_cv_iconv_const=const])])
    if test $ac_cv_iconv_const = const; then
       AC_DEFINE([ICONV_CONST], const, [Define as const if the declaration of iconv() needs const.])
    else
       AC_DEFINE([ICONV_CONST], /* no-const */)
    fi
    AC_CACHE_CHECK(for iconv name of latin-1 encoding, ac_cv_iconv_latin1_name,
      [AC_RUN_IFELSE([AC_LANG_PROGRAM([[#include <stdlib.h>
            #include <iconv.h>]],
          [[iconv_t res;
            res = iconv_open ("UTF-8", "ISO-8859-1");
            if (res == (iconv_t)-1 || !res) exit (1);
            exit (0);]])],
        [ac_cv_iconv_latin1_name=ISO-8859-1],
        [ac_cv_iconv_latin1_name=ISO8859-1],
        [ac_cv_iconv_latin1_name=cannothappen])
    ])
    AC_DEFINE_UNQUOTED([ICONV_LATIN1_NAME], ["$ac_cv_iconv_latin1_name"], [the name of the latin1 encoding])
    AC_CACHE_CHECK(for iconv name of latin-9 encoding, ac_cv_iconv_latin9_name,
      [AC_RUN_IFELSE([AC_LANG_PROGRAM([[#include <stdlib.h>
            #include <iconv.h>]],
          [[iconv_t res;
            res = iconv_open ("UTF-8", "ISO-8859-15");
            if (res == (iconv_t)-1 || !res) exit (1);
            exit (0);]])],
        [ac_cv_iconv_latin9_name=ISO-8859-15],
        [ac_cv_iconv_latin9_name=ISO8859-15],
        [ac_cv_iconv_latin9_name=cannothappen])
    ])
    AC_DEFINE_UNQUOTED([ICONV_LATIN9_NAME], ["$ac_cv_iconv_latin9_name"], [the name of the latin9 encoding])
    AC_CACHE_CHECK(for iconv name of ucs-2be encoding, ac_cv_iconv_ucs2be_name,
      [AC_RUN_IFELSE([AC_LANG_PROGRAM([[#include <stdlib.h>
            #include <iconv.h>]],
          [[iconv_t res;
            res = iconv_open ("UTF-8", "UCS-2BE");
            if (res == (iconv_t)-1 || !res) exit (1);
            exit (0);]])],
        [ac_cv_iconv_ucs2be_name=UCS-2BE],
        [ac_cv_iconv_ucs2be_name=UNICODEBIG],
        [ac_cv_iconv_ucs2be_name=cannothappen])
    ])
    AC_DEFINE_UNQUOTED([ICONV_UCS2BE_NAME], ["$ac_cv_iconv_ucs2be_name"], [the name of the ucs-2be encoding])
  fi
fi

if test "x$ac_arg_use_iconv" = "xbuiltin"; then
  AC_DEFINE([ICONV_LATIN1_NAME], ["ISO-8859-1"])
  AC_DEFINE([ICONV_LATIN9_NAME], ["ISO-8859-15"])
  AC_DEFINE([ICONV_UCS2BE_NAME], ["UCS-2BE"])
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

AC_MSG_CHECKING([whether to enable usage of utf-8 internally])
AC_MSG_RESULT([$ac_arg_use_utf8])
AC_MSG_CHECKING([whether to enable iconv and which iconv lib to use])
AC_MSG_RESULT([$ac_arg_use_iconv])
AC_MSG_CHECKING([whether to enable peer to peer connections])
AC_MSG_RESULT([$ac_arg_use_dc])
AC_MSG_CHECKING([whether to enable remote control FIFO])
AC_MSG_RESULT([$ac_arg_use_remote])
AC_MSG_CHECKING([whether to enable Tcl scripting])
AC_MSG_RESULT([$ac_arg_use_tcl])
AC_MSG_CHECKING([whether to enable SSL/TLS support])
AC_MSG_RESULT([$ac_arg_use_ssl])


if test "x$ac_arg_use_tcl" = "xyes"; then
  AC_DEFINE(ENABLE_TCL, 1, [Whether to support Tcl scripting])
fi

if test "x$ac_arg_use_ssl" = "xgnutls"; then
  AC_DEFINE(ENABLE_SSL, 1, [Whether to enable SSL/TLS support])
  AC_DEFINE(ENABLE_GNUTLS, 1, [Whether to use GnuTLS for SSL/TLS support])
else
  if test "x$ac_arg_use_ssl" = "xopenssl"; then
    AC_DEFINE(ENABLE_SSL, 1, [Whether to enable SSL/TLS support])
  fi
  AC_DEFINE(ENABLE_GNUTLS, 0)
fi

if test "x$ac_arg_use_utf8" = "xyes"; then
  AC_DEFINE(ENABLE_UTF8, 1, [Whether to use UTF-8 internally])
fi

if test "x$ac_arg_use_iconv" != "xbuiltin"; then
  AC_DEFINE_UNQUOTED(ENABLE_ICONV, "$ac_arg_use_iconv", [Which lib to use for iconv() functions])
fi

if test "x$ac_arg_use_dc" = "xyes"; then
  AC_DEFINE(ENABLE_PEER2PEER, 1, [Whether to enable peer to peer connections])
fi

if test "x$ac_arg_use_remote" = "xyes"; then
  AC_DEFINE(ENABLE_REMOTECONTROL, 1, [Whether to enable remote control FIFO])
fi

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
AC_CHECK_FUNCS([localtime_r timegm atexit memcmp mkdir strcasecmp uname fcntl \
                strchr memchr strdup strncasecmp strpbrk strrchr strstr strtol strtok_r \
                symlink chmod tcgetattr gettimeofday setlocale select sigprocmask])

for f in mkdir atexit \
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
AC_CONFIG_FILES([doc/pt_BR/Makefile doc/sr/Makefile doc/it/Makefile doc/sk/Makefile])
AC_OUTPUT
