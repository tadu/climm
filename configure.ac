
# $Id$ 

# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.50)
AC_INIT(micq, 0.4.9.3.1)
AC_CONFIG_SRCDIR([include/micq.h])
AM_INIT_AUTOMAKE(1.6)

AM_MAINTAINER_MODE
AM_CONFIG_HEADER(include/config.h)

AC_CANONICAL_HOST
AC_DEFINE(MICQ_VERSION,VERSION,[mICQ's current version])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S

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
AC_CHECK_HEADERS([arpa/inet.h errno.h fcntl.h limits.h netdb.h netinet/in.h sys/ioctl.h sys/socket.h sys/time.h sys/wait.h termios.h unistd.h])

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
AC_TYPE_SIGNAL
AC_FUNC_VPRINTF
AC_FUNC_SNPRINTF
AC_CHECK_FUNCS([atexit gethostbyname memmove memset mkdir select socket strcasecmp \
                strchr strdup strncasecmp strpbrk strrchr strstr strtol strtok_r \
                symlink chmod tcgetattr hstrerror gettimeofday])


# vprintf stat malloc \
for f in memset memmove mkdir select socket atexit \
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

AC_CONFIG_FILES([doc/Makefile src/Makefile Makefile lang/Makefile doc/de/Makefile doc/ru/Makefile doc/fr/Makefile doc/es/Makefile])
AC_OUTPUT