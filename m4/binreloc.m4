# Check for binary relocation support
# Hongli Lai
# http://autopackage.org/

dnl Modified by R. Kuhlmann for different defaults.

AC_DEFUN([rk_BINRELOC],
[
	AC_ARG_ENABLE(binreloc,
		[  --enable-binreloc       compile with binary relocation support
                          (default=enable when available)],
		enable_binreloc=$enableval,enable_binreloc=auto)

	AC_ARG_ENABLE(binreloc-threads,
		[  --enable-binreloc-threads      compile binary relocation with threads support
	                         (default=no)],
		enable_binreloc_threads=$enableval,enable_binreloc_threads=no)

	BINRELOC_CFLAGS=
	BINRELOC_LIBS=
	if test "x$enable_binreloc" = "xauto"; then
		AC_CHECK_FILE([/proc/self/maps])
		AC_CACHE_CHECK([whether everything is installed to the same prefix],
			       [br_cv_valid_prefixes], [
			        if test "$bindir" != '${exec_prefix}/bin' && test "$bindir" != "$exec_prefix/bin"; then
			        	br_cv_valid_prefixes="no (bindir)"
			        elif test "$sbindir" != '${exec_prefix}/sbin' && test "$sbindir" != "$exec_prefix/sbin"; then
			        	br_cv_valid_prefixes="no (sbindir)"
			        elif test "$libdir" != '${exec_prefix}/lib' && test "$libdir" != "$exec_prefix/lib"; then
			        	br_cv_valid_prefixes="no (libdir)"
			        elif test "$libexecdir" != '${exec_prefix}/libexec' && test "$libexecdir" != "$exec_prefix/libexec"; then
			        	br_cv_valid_prefixes="no (libexecdir)"
			        elif test "$sysconfdir" != '${prefix}/etc' && test "$sysconfdir" != "$prefix/etc"; then
			        	br_cv_valid_prefixes="no (sysconfdir)"
			        elif test "$datadir" = '${prefix}/share' || test "$datadir" = "$prefix/share"; then
			        	br_cv_valid_prefixes=yes
			        elif test "$datadir" != '${datarootdir}' && test "$datadir" != "$datarootdir"; then
			        	br_cv_valid_prefixes="no (datadir $datadir datarootdir $datarootdir)"
			        elif test "$datarootdir" != '${prefix}/share' && test "$datarootdir" != "$prefix/share"; then
			        	br_cv_valid_prefixes="no (datadir $datadir datarootdir $datarootdir)"
			        else
					br_cv_valid_prefixes=yes
				fi
				])
	fi
	AC_CACHE_CHECK([whether binary relocation support should be enabled],
		       [br_cv_binreloc],
		       [if test "x$enable_binreloc" = "xyes"; then
		       	       br_cv_binreloc=yes
		       elif test "x$enable_binreloc" = "xauto"; then
			       if test "x$br_cv_valid_prefixes" = "xyes" -a \
			       	       "x$ac_cv_file__proc_self_maps" = "xyes"; then
				       br_cv_binreloc=yes
			       else
				       br_cv_binreloc=no
			       fi
		       else
			       br_cv_binreloc=no
		       fi])

	if test "x$br_cv_binreloc" = "xyes"; then
		BINRELOC_CFLAGS="-DENABLE_BINRELOC"
		AC_DEFINE(ENABLE_BINRELOC,1,[Use binary relocation?])
		if test "x$enable_binreloc_threads" = "xyes"; then
			AC_CHECK_LIB([pthread], [pthread_getspecific])
		fi

		AC_CACHE_CHECK([whether binary relocation should use threads],
			       [br_cv_binreloc_threads],
			       [if test "x$enable_binreloc_threads" = "xyes"; then
					if test "x$ac_cv_lib_pthread_pthread_getspecific" = "xyes"; then
						br_cv_binreloc_threads=yes
					else
						br_cv_binreloc_threads=no
					fi
			        else
					br_cv_binreloc_threads=no
				fi])

		if test "x$br_cv_binreloc_threads" = "xyes"; then
			BINRELOC_LIBS="-lpthread"
			AC_DEFINE(BR_PTHREADS,1,[Include pthread support for binary relocation?])
		else
			BINRELOC_CFLAGS="$BINRELOC_CFLAGS -DBR_PTHREADS=0"
			AC_DEFINE(BR_PTHREADS,0,[Include pthread support for binary relocation?])
		fi
	fi
	AC_SUBST(BINRELOC_CFLAGS)
	AC_SUBST(BINRELOC_LIBS)
])
