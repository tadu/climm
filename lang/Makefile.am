# $Id$

langdir = $(datadir)/micq
lang_DATA = C.i18n \
         de.i18n de_CH.i18n de@fun.i18n en.i18n en@fun.i18n \
         es.i18n fr.i18n ru.i18n ru@fun.i18n ja.UTF-8.i18n ro.i18n \
         bg.i18n pt.i18n fi.i18n hr.i18n id.i18n it.i18n nl.i18n \
         pl.i18n se.i18n uk.i18n yu.i18n zh_CN.i18n sr.UTF-8.i18n \
         sr.i18n ja.i18n

maintainer: sr.i18n ja.i18n

sr.i18n: sr.UTF-8.i18n
	sed < $< 's/«/\{/g;s/»/\}/g' | grep -v ^1007 \
	  | iconv  -f utf-8 -t iso-8859-5 | sed s/^1000:.*/1000:iso-8859-5/ > $@

ja.i18n: ja.UTF-8.i18n
	sed < $< 's/«/\{/g;s/»/\}/g' | grep -v ^1007 | grep -v ^# | grep -v ^1849 | grep -v ^2084 \
	  | iconv -f utf-8 -t eucjp | sed s/^1000:.*/1000:eucjp/ > $@

EXTRA_DIST = i18n-extract i18n-number i18n-template $(lang_DATA)

