
# $Id$ 

# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.53)
AC_INIT(micq, 0.4.9.4)
AC_CONFIG_SRCDIR([include/micq.h])
AC_CANONICAL_HOST

AC_CHECK_PROG(AUTOMAKE, automake-1.6, automake-1.6)
AC_CHECK_PROG(ACLOCAL, aclocal-1.6, aclocal-1.6)
AC_CHECK_PROG(AUTOHEADER, autoheader, autoheader)
AC_CHECK_PROG(AUTOCONF, autoconf, autoconf)

AM_INIT_AUTOMAKE(1.6)
AM_CONFIG_HEADER(include/config.h)
AM_MAINTAINER_MODE

AC_DEFINE(MICQ_VERSION,VERSION,[mICQ's current version])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S

# Check for arguments.


AC_ARG_ENABLE(peer2peer, AC_HELP_STRING([--disable-peer2peer], [disable peer to peer connections]),
              ac_arg_use_peer2peer=$enableval, ac_arg_use_peer2peer=yes)
AC_ARG_ENABLE(utf8, AC_HELP_STRING([--enable-utf8], [enable use of UTF-8 internally]),
              ac_arg_use_utf8=$enableval, ac_arg_use_utf8=no)
AC_ARG_ENABLE([iconv], AC_HELP_STRING([--enable-iconv], [use libiconv for character conversion]),
              ac_arg_use_iconv=$enableval, ac_arg_use_iconv=auto)
AC_ARG_ENABLE([iconv], AC_HELP_STRING([--disable-iconv], [do not use iconv() at all]),
              ac_arg_use_iconv=$enableval)

if test "x$ac_arg_use_iconv" != "xno"; then
  if test "x$ac_arg_use_iconv" = "xauto"; then
    AC_CHECK_FUNC([iconv], [ac_arg_use_iconv=glibc], [ac_arg_use_iconv=none])
  else
    AC_CHECK_LIB([iconv], [iconv], [ac_arg_use_iconv=iconv], [ac_arg_use_iconv=none])
    if test "x$ac_arg_use_iconv" = "xnone"; then
      AC_CHECK_LIB([libiconv], [iconv], [ac_arg_use_iconv=libiconv])
    fi
    if test "x$ac_arg_use_iconv" = "xnone"; then
      AC_CHECK_FUNC([iconv], [ac_arg_use_iconv=glibc])
    fi
  fi
else
  ac_arg_use_iconv=none
fi

AC_MSG_CHECKING([whether to enable peer to peer connections])
AC_MSG_RESULT([$ac_arg_use_peer2peer])
AC_MSG_CHECKING([whether to enable usage of utf-8 internally])
AC_MSG_RESULT([$ac_arg_use_utf8])
AC_MSG_CHECKING([whether to enable iconv and which iconv to use])
AC_MSG_RESULT([$ac_arg_use_iconv])

if test "x$ac_arg_use_peer2peer" = "xyes"; then
  AC_DEFINE(ENABLE_PEER2PEER, 1, [Whether to enable peer to peer connections])
fi

if test "x$ac_arg_use_utf8" = "xyes"; then
  AC_DEFINE(ENABLE_UTF8, 1, [Whether to use UTF-8 internally])
fi

if test "x$ac_arg_use_iconv" != "xnone"; then
  AC_DEFINE(ENABLE_ICONV, 1, [Whether to use iconv functions])
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
AC_CHECK_HEADERS([sys/select.h arpa/inet.h errno.h fcntl.h limits.h netdb.h netinet/in.h sys/ioctl.h sys/socket.h sys/time.h sys/wait.h termios.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM

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

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL

AC_CHECK_DECLS(h_errno,,,[$ac_includes_default
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif])

AC_TYPE_SIGNAL

AC_STRUCT_TIMEZONE
AC_CACHE_CHECK(for timezone external, ac_cv_var_timezone,
[  AC_TRY_LINK([#include <time.h>], [int x = - timezone;],
      ac_cv_var_timezone=yes,
      ac_cv_var_timezone=no)
])
if test $ac_cv_var_timezone = yes; then
   AC_DEFINE([HAVE_TIMEZONE], 1,
           [Define if you have the external 'timezone' variable.])
fi

AC_CHECK_TYPES(socklen_t,,,[$ac_includes_default
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif])
AC_FUNC_VPRINTF
AC_FUNC_SNPRINTF
AC_FUNC_MEMMOVE
AC_CHECK_FUNCS([atexit gethostbyname memset mkdir select socket strcasecmp \
                strchr strdup strncasecmp strpbrk strrchr strstr strtol strtok_r \
                symlink chmod tcgetattr hstrerror gettimeofday])


# vprintf stat malloc \
for f in memset mkdir select socket atexit \
         strcasecmp strchr strdup strncasecmp strpbrk strrchr strstr strtol; do
    if eval test "x\$ac_cv_func_$f" != "xyes"; then
        AC_MSG_ERROR([Required function $f is missing. Sorry.])
    fi
done

if test "x$ac_cv_func_gethostbyname" != "xyes"; then
    AC_MSG_WARN([Function gethostbyname() not found; you will need to specify all host names as IP addresses.])
fi

# Checks for headers.

AC_CHECK_HEADERS(arpa/inet.h)

# Optimize.

if test "x$GCC" = xyes; then
    CFLAGS="$CFLAGS -Wall"
fi

if test "x$WIP" != "x"; then
    AC_DEFINE(WIP,1,[Include probably buggy work-in-progress code])
fi

if test "x$MICQ_EXTRAVERSION" != "x"; then
    AC_DEFINE_UNQUOTED(EXTRAVERSION,"$MICQ_EXTRAVERSION",[Extra version information like the distribution used])
else
    AC_DEFINE(EXTRAVERSION,["hand compiled"])
fi

AC_CONFIG_FILES([doc/Makefile src/Makefile Makefile lang/Makefile])
AC_CONFIG_FILES([doc/de/Makefile doc/ru/Makefile doc/fr/Makefile doc/es/Makefile doc/pt_BR/Makefile])
AC_OUTPUT
