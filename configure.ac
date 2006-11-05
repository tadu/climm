
# $Id$ 

# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([micq], [0.5.2])
AC_CONFIG_SRCDIR([include/micq.h])
AC_CONFIG_LIBOBJ_DIR(replace)
AC_CONFIG_HEADER(include/config.h)
AC_CANONICAL_HOST

AC_CHECK_PROG(AUTOMAKE, automake-1.9, automake-1.9)
AC_CHECK_PROG(ACLOCAL, aclocal-1.9, aclocal-1.9)
AC_CHECK_PROG(AUTOHEADER, autoheader, autoheader)
AC_CHECK_PROG(AUTOCONF, autoconf, autoconf)
PKG_PROG_PKG_CONFIG

AM_INIT_AUTOMAKE(1.9)
AM_MAINTAINER_MODE

AC_DEFINE(MICQ_VERSION,VERSION,[mICQ's current version])

# Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB
AC_PROG_INSTALL
AC_PROG_LN_S
AC_SYS_LARGEFILE

# Check for arguments.

AC_ARG_ENABLE([peer2peer], AC_HELP_STRING([--disable-peer2peer], [disable peer to peer connections]),
              ac_arg_use_dc=$enableval, ac_arg_use_dc=yes)
AC_ARG_ENABLE([iconv], AC_HELP_STRING([--disable-iconv], [do not use iconv() at all]),
              ac_arg_use_iconv=$enableval, ac_arg_use_iconv=yes)
AC_ARG_ENABLE([iconvrepl], AC_HELP_STRING([--enable-iconvrepl], [compile in iconv() replacement fallbacks]),
              ac_arg_use_iconvrepl=$enableval, ac_arg_use_iconvrepl=no)
AC_ARG_ENABLE([remote], AC_HELP_STRING([--disable-remote], [disable remote control FIFO]),
              ac_arg_use_remote=$enableval, ac_arg_use_remote=auto)
AC_ARG_ENABLE([jabber], AC_HELP_STRING([--enable-jabber], [enable Jabber chat network (EXPERIMENTAL INCOMPLETE)]),
              ac_arg_use_jabber=$enableval, ac_arg_use_jabber=no)
AC_ARG_ENABLE([msn], AC_HELP_STRING([--enable-msn], [enable MSN chat network (EXPERIMENTAL INCOMPLETE)]),
              ac_arg_use_msn=$enableval, ac_arg_use_msn=auto)
AC_ARG_ENABLE([tcl], AC_HELP_STRING([--disable-tcl], [disable Tcl scripting]),
              ac_arg_use_tcl=$enableval, ac_arg_use_tcl=auto)
AC_ARG_ENABLE([ssl], AC_HELP_STRING([--disable-ssl], [disable SSL encryption]),
              ac_arg_use_ssl=$enableval, ac_arg_use_ssl=auto)
AC_ARG_ENABLE([ssl], AC_HELP_STRING([[[[--enable-ssl=gnutls,openssl]]]], [choose SSL library]))

AC_ARG_ENABLE([autopackage], AC_HELP_STRING([--enable-autopackage], [enable autopackage generation]),
              ac_arg_use_autopackage=$enableval, ac_arg_use_autopackage=no)

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
AC_CHECK_HEADERS([locale.h langinfo.h sys/select.h arpa/inet.h ctype.h errno.h fcntl.h limits.h netdb.h netinet/in.h sys/ioctl.h sys/socket.h sys/time.h sys/wait.h termios.h unistd.h sys/utsname.h sys/un.h wchar.h wctype.h pwd.h])

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
    if test "x$ac_use_ssl$ac_arg_use_autopackage" = xopensslyes; then
      AC_MSG_FAILURE([cannot use OpenSSL with autopackage])
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
  if test "x$ac_arg_use_autopackage" = xyes; then
    AC_CHECK_HEADERS([tcl8.4/tcl.h tcl.h], [ac_use_tcl=yes
                                            break])
    if test "x$ac_use_tcl" = "xyes"; then
      AC_SEARCH_LIBS([Tcl_CreateInterp], [tcl8.4], [ac_use_tcl=yes], [ac_use_tcl=no])
    fi
  else
    AC_CHECK_HEADERS([tcl8.4/tcl.h tcl8.3/tcl.h tcl.h], [ac_use_tcl=yes
                                                         break])
    if test "x$ac_use_tcl" = "xyes"; then
      AC_SEARCH_LIBS([Tcl_CreateInterp], [tcl8.4 tcl8.3], [ac_use_tcl=yes], [ac_use_tcl=no])
    fi
  fi
  if test "x$ac_usl_tcl$ac_arg_use_tcl" = "xnoyes"; then
    AC_MSG_FAILURE([cannot find a suitable Tcl interpreter library])
  fi
  ac_arg_use_tcl=$ac_use_tcl;
fi

if test "x$ac_arg_use_iconv" != "xno"; then
  AM_ICONV
fi

AC_CACHE_CHECK(for iconv() fallbacks to compile in, ac_cv_iconv_fb,
  [ac_cv_iconv_fb="ASCII UCS2BE WIN1251 KOI8 LATIN9 LATIN1 UTF8 WCHART TRANSLIT"
  if test "x$ac_arg_use_iconvrepl" != "xyes"; then
    if test "x$ac_arg_use_iconvrepl" != "xno"; then
      ac_cv_iconv_fb=$ac_arg_use_iconvrepl;
    fi
  fi
  if test "x$am_cv_func_iconv$ac_arg_use_iconvrepl" = "xyesno"; then
    old_LDFLAGS=$LDFLAGS
    LDFLAGS="$LDFLAGS $LIBICONV"
    AC_RUN_IFELSE([AC_LANG_PROGRAM([[#include <stdlib.h>
          #include <iconv.h>
          #include <stdio.h>]],
        [[iconv_t resa, resb;
          FILE *hf = fopen ("conftest.encodings", "w");
          resa = iconv_open ("UTF-8", "US-ASCII");
          resb = iconv_open ("US-ASCII", "UTF-8");
          if (resa == (iconv_t)-1 || !resa || resb == (iconv_t)-1 || !resb)
              fprintf (hf, "ASCII ");
          resa = iconv_open ("UTF-8", "UCS-2BE");
          resb = iconv_open ("UCS-2BE", "UTF-8");
          if (resa == (iconv_t)-1 || !resa || resb == (iconv_t)-1 || !resb)
          {
              resa = iconv_open ("UTF-8", "UNICODEBIG");
              resb = iconv_open ("UNICODEBIG", "UTF-8");
          }
          if (resa == (iconv_t)-1 || !resa || resb == (iconv_t)-1 || !resb)
              fprintf (hf, "UCS2BE ");
          resa = iconv_open ("UTF-8", "CP1251");
          resb = iconv_open ("CP1251", "UTF-8");
          if (resa == (iconv_t)-1 || !resa || resb == (iconv_t)-1 || !resb)
          {
              resa = iconv_open ("UTF-8", "WINDOWS-1251");
              resb = iconv_open ("WINDOWS-1251", "UTF-8");
          }
          if (resa == (iconv_t)-1 || !resa || resb == (iconv_t)-1 || !resb)
              fprintf (hf, "WIN1251 ");
          resa = iconv_open ("UTF-8", "KOI8-U");
          resb = iconv_open ("KOI8-U", "UTF-8");
          if (resa == (iconv_t)-1 || !resa || resb == (iconv_t)-1 || !resb)
              fprintf (hf, "KOI8 ");
          resa = iconv_open ("UTF-8", "ISO-8859-15");
          resb = iconv_open ("ISO-8859-15", "UTF-8");
          if (resa == (iconv_t)-1 || !resa || resb == (iconv_t)-1 || !resb)
          {
              resa = iconv_open ("UTF-8", "ISO8859-15");
              resb = iconv_open ("ISO8859-15", "UTF-8");
          }
          if (resa == (iconv_t)-1 || !resa || resb == (iconv_t)-1 || !resb)
              fprintf (hf, "LATIN9 ");
          resa = iconv_open ("UTF-8", "ISO-8859-1");
          resb = iconv_open ("ISO-8859-1", "UTF-8");
          if (resa == (iconv_t)-1 || !resa || resb == (iconv_t)-1 || !resb)
          {
              resa = iconv_open ("UTF-8", "ISO8859-1");
              resb = iconv_open ("ISO8859-1", "UTF-8");
          }
          if (resa == (iconv_t)-1 || !resa || resb == (iconv_t)-1 || !resb)
              fprintf (hf, "LATIN1 ");
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
          if (!resa)
              fprintf (hf, "UTF8 ");
          resa = iconv_open ("UTF-8", "WCHAR_T");
          resb = iconv_open ("WCHAR_T", "UTF-8");
          if (resa == (iconv_t)-1 || !resa || resb == (iconv_t)-1 || !resb)
              fprintf (hf, "WCHART ");
          resa = iconv_open ("US-ASCII//TRANSLIT", "UTF-8");
          if (resa == (iconv_t)-1 || !resa)
              fprintf (hf, "TRANSLIT ");
          fclose (hf);
          exit (0);]])],
      [ac_cv_iconv_fb="`cat conftest.encodings`"],
      [], [:])
    LDFLAGS=$old_LDFLAGS
    rm -f conftest.encodings
  fi
])

if false; then
 AC_DEFINE(ENABLE_FALLBACK_ASCII, 0, enable fallback function for ASCII)
 AC_DEFINE(ENABLE_FALLBACK_UCS2BE, 0, enable fallback function for UCS2BE)
 AC_DEFINE(ENABLE_FALLBACK_WIN1251, 0, enable fallback function for WIN1251)
 AC_DEFINE(ENABLE_FALLBACK_KOI8, 0, enable fallback function for KOI8)
 AC_DEFINE(ENABLE_FALLBACK_LATIN9, 0, enable fallback function for LATIN9)
 AC_DEFINE(ENABLE_FALLBACK_LATIN1, 0, enable fallback function for LATIN1)
 AC_DEFINE(ENABLE_FALLBACK_UTF8, 0, enable fallback function for UTF8)
 AC_DEFINE(ENABLE_FALLBACK_WCHART, 0, enable fallback function for WCHART)
 AC_DEFINE(ENABLE_FALLBACK_TRANSLIT, 0, transliteration is _not_ supported)
fi

for enc in DUMMY $ac_cv_iconv_fb; do
  AC_DEFINE_UNQUOTED(ENABLE_FALLBACK_$enc, 1, enable fallback function for $enc)
done

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

AC_LANG([C++])
AC_PROG_CXX
if test "x$ac_arg_use_msn" = "xyes"; then
  AC_SUBST(LIBMSN, "libmicqmsn.a", [Static lib to link if MSN is enabled])
  AC_SUBST(L_LIBMSN, "-L. -lmicqmsn -lmsn -lstdc++", [Static lib link option to link if MSN is enabled])
  AC_DEFINE(ENABLE_MSN, 1, [Enable MSN chat network using libmsn])
  AC_CHECK_HEADERS(sstream,,
    [AC_CHECK_HEADERS(strstream,,
        AC_MSG_ERROR([Your libstdc++ doesn't have the sstream or strstream classes]))])
else
  ac_arg_use_msn=no
fi
AC_LANG([C])

if test "x$ac_arg_use_jabber" != "xno"; then
  if test "x$ac_arg_use_ssl" != "xgnutls"; then
    AC_MSG_CHECKING([Jabber])
    AC_MSG_RESULT([no, GnuTLS missing])
    ac_use_jabber=no
  else
    PKG_CHECK_MODULES(Jabber, gloox, ac_use_jabber=yes, ac_use_jabber=no)
  fi

  if test "x$ac_use_jabber$ac_arg_use_jabber" = "xnoyes"; then
    AC_MSG_FAILURE([cannot find Jabber library libgloox])
  fi
  ac_arg_use_jabber=$ac_use_jabber;
  if test "x$ac_arg_use_jabber" = "xyes"; then
    AC_SUBST(LIBJABBER, "libmicqjabber.a", [Static lib to link if Jabber is enabled])
    AC_SUBST(L_LIBJABBER, "-L. -lmicqjabber", [Static lib link option to link if Jabber is enabled])
    AC_DEFINE(ENABLE_JABBER, 1, [Enable Jabber chat network using libgloox])
    LIBS="$LIBS $Jabber_LIBS"
  fi
fi

if test "x$ac_arg_use_dc" = "xyes"; then
  AC_DEFINE(ENABLE_PEER2PEER, 1, [Whether to enable peer to peer connections])
fi

if test "x$ac_arg_use_remote" = "xyes"; then
  AC_DEFINE(ENABLE_REMOTECONTROL, 1, [Whether to enable remote control FIFO])
fi

if test "x$ac_arg_use_ssl" = "xgnutls"; then
  AC_DEFINE(ENABLE_SSL, 1, [Whether to use the SSL extension])
  AC_DEFINE(ENABLE_GNUTLS, 1, [Whether to use GnuTLS for SSL/TLS support])
elif test "x$ac_arg_use_ssl" = "xopenssl"; then
  AC_DEFINE(ENABLE_SSL, 1, [Whether to enable SSL/TLS support])
fi

if test "x$ac_arg_use_tcl" = "xyes"; then
  AC_DEFINE(ENABLE_TCL, 1, [Whether to use the Tcl scripting extension])
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
AC_CHECK_FUNCS([mkdir atexit strcasecmp strchr memchr strdup strncasecmp strpbrk strrchr strstr strtol],,
               [AC_MSG_ERROR([Required function $ac_func missing. Sorry.])])
AC_CHECK_FUNCS([localtime_r timegm timelocal memcmp uname fcntl strchr strtok_r \
                symlink chmod tcgetattr gettimeofday setlocale select sigprocmask])
AC_CHECK_FUNCS([memset wcwidth iswalnum iswspace nl_langinfo atoll getpwuid getuid fsync])

if test "x$ac_cv_func_timegm" != xyes; then
    AC_LIBOBJ(timegm)
    AC_CHECK_FUNCS([setenv putenv unsetenv])
fi

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

AC_CHECK_TYPES([wchar_t, wint_t, socklen_t, BOOL, UBYTE, UWORD, UDWORD],,,[$ac_includes_default
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
AC_CACHE_CHECK(checking for variadic macro support, ac_cv_macro_variadic,
  [AC_LINK_IFELSE([AC_LANG_PROGRAM([[#define variadic(a,...) printf (a "bla" , ## __VA_ARGS__) ]],
      [[variadic ("bla\n");                      /* note space before comma ^ to avoid gcc-2.95 bug */
        variadic ("bla%sbla\n", "bla");]])],
    [ac_cv_macro_variadic=yes],
    [ac_cv_macro_variadic=no])])
if test "x$ac_cv_macro_variadic" = xyes; then
    AC_DEFINE(HAVE_VARIADIC_MACRO,1,[have support for advances variadic macros])
fi


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

if test "x$ac_arg_use_autopackage" = xyes; then
    enable_binreloc=auto
    rk_BINRELOC
    if test "x$br_cv_binreloc" = xyes; then
        AC_CHECK_PROGS(RELAYTOOL, relaytool, :)
        old_LIBS=$LIBS
        LIBS=
        AC_SEARCH_LIBS([dlopen], [dl])
        LIBS="$old_LIBS $LIBS"
        if test "x$RELAYTOOL" != x:; then
            LIBS='`'"$RELAYTOOL --relay 'tcl8.4 tcl8.3 gnutls gloox' $LIBS"'`'
            AC_DEFINE(ENABLE_AUTOPACKAGE,1,[Use autopackage to check libraries at run time])
        fi
    fi
else
    AC_SUBST(BINRELOC_LIBS,[ ])
fi

AC_MSG_CHECKING([whether to enable Jabber chat network (EXPERIMENTAL INCOMPLETE)])
AC_MSG_RESULT([$ac_arg_use_jabber])
AC_MSG_CHECKING([whether to enable MSN chat network (EXPERIMENTAL INCOMPLETE)])
AC_MSG_RESULT([$ac_arg_use_msn])
AC_MSG_CHECKING([whether to enable peer to peer connections])
AC_MSG_RESULT([$ac_arg_use_dc])
AC_MSG_CHECKING([whether to enable remote control FIFO])
AC_MSG_RESULT([$ac_arg_use_remote])
AC_MSG_CHECKING([whether to enable Tcl scripting])
AC_MSG_RESULT([$ac_arg_use_tcl])
AC_MSG_CHECKING([whether to enable SSL/TLS support])
AC_MSG_RESULT([$ac_arg_use_ssl])


AC_CONFIG_FILES([src/Makefile Makefile lang/Makefile doc/Makefile])
AC_OUTPUT
