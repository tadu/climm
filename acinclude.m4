
dnl @synopsis AC_FUNC_SNPRINTF
dnl
dnl Checks for a fully C99 compliant snprintf, in particular checks
dnl whether it does bounds checking and returns the correct string length;
dnl does the same check for vsnprintf. If no working snprintf or vsnprintf
dnl is found, request a replacement and warn the user about it.
dnl Note: the mentioned replacement is freely available and may be used in
dnl any project regardless of it's licence (just like the autoconf special
dnl exemption).
dnl
dnl @version $Id$
dnl @author Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
dnl @copyright GPL-2

AC_DEFUN([AC_FUNC_SNPRINTF],
[AC_CHECK_FUNCS(snprintf vsnprintf)
AC_MSG_CHECKING(for working snprintf)
AC_CACHE_VAL(ac_cv_have_working_snprintf,
[AC_TRY_RUN(
[#include <stdio.h>

int main(void)
{
    char bufs[5] = { 'x', 'x', 'x', '\0', '\0' };
    char bufd[5] = { 'x', 'x', 'x', '\0', '\0' };
    int i;
    i = snprintf (bufs, 2, "%s", "111");
    if (strcmp (bufs, "1")) exit (1);
    if (i != 3) exit (1);
    i = snprintf (bufd, 2, "%d", 111);
    if (strcmp (bufd, "1")) exit (1);
    if (i != 3) exit (1);
    exit(0);
}], ac_cv_have_working_snprintf=yes, ac_cv_have_working_snprintf=no, ac_cv_have_working_snprintf=cross)])
AC_MSG_RESULT([$ac_cv_have_working_snprintf])
AC_MSG_CHECKING(for working vsnprintf)
AC_CACHE_VAL(ac_cv_have_working_vsnprintf,
[AC_TRY_RUN(
[#include <stdio.h>
#include <stdarg.h>

int my_vsnprintf (char *buf, const char *tmpl, ...)
{
    int i;
    va_list args;
    va_start (args, tmpl);
    i = vsnprintf (buf, 2, tmpl, args);
    va_end (args);
    return i;
}

int main(void)
{
    char bufs[5] = { 'x', 'x', 'x', '\0', '\0' };
    char bufd[5] = { 'x', 'x', 'x', '\0', '\0' };
    int i;
    i = my_vsnprintf (bufs, "%s", "111");
    if (strcmp (bufs, "1")) exit (1);
    if (i != 3) exit (1);
    i = my_vsnprintf (bufd, "%d", 111);
    if (strcmp (bufd, "1")) exit (1);
    if (i != 3) exit (1);
    exit(0);
}], ac_cv_have_working_vsnprintf=yes, ac_cv_have_working_vsnprintf=no, ac_cv_have_working_vsnprintf=cross)])
AC_MSG_RESULT([$ac_cv_have_working_vsnprintf])
if test x$ac_cv_have_working_snprintf$ac_cv_have_working_vsnprintf != "xyesyes"; then
  AC_LIBOBJ(snprintf)
  AC_MSG_WARN([Replacing missing/broken (v)snprintf() with version from http://www.ijs.si/software/snprintf/.])
  AC_DEFINE(PREFER_PORTABLE_SNPRINTF, 1, "enable replacement (v)snprintf if system (v)snprintf is broken")
fi])

dnl @synopsis AC_FUNC_MEMMOVE
dnl
dnl Checks for a memmove that can handle overlaps correctly. If no working
dnl memmove is found, request a replacement and warn the user about it.
dnl
dnl @version $Id$
dnl @author Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
dnl @copyright GPL-2

AC_DEFUN([AC_FUNC_MEMMOVE],
[AC_CHECK_FUNCS(memmove)
AC_MSG_CHECKING(for working memmove)
AC_CACHE_VAL(ac_cv_have_working_memmove,
[AC_TRY_RUN(
[#include <stdio.h>

int main(void)
{
    char buf[10];
    strcpy (buf, "01234567");
    memmove (buf, buf + 2, 3);
    if (strcmp (buf, "23434567"))
        exit (1);
    strcpy (buf, "01234567");
    memmove (buf + 2, buf, 3);
    if (strcmp (buf, "01012567"))
        exit (1);
    exit (0);
}], ac_cv_have_working_memmove=yes, ac_cv_have_working_memmove=no, ac_cv_have_working_memmove=cross)])
AC_MSG_RESULT([$ac_cv_have_working_memmove])
if test x$ac_cv_have_working_memmove != "xyes"; then
  AC_LIBOBJ(memmove)
  AC_MSG_WARN([Replacing missing/broken memmove.])
  AC_DEFINE(PREFER_PORTABLE_MEMMOVE, 1, "enable replacement memmove if system memmove is broken or missing")
fi])
