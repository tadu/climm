# $Id$ 

AUTOMAKE_OPTIONS = subdir-objects

bin_PROGRAMS = climm # icqsyn icqprx
EXTRA_PROGRAMS = icqprx icqsyn
EXTRA_LIBRARIES = libiksemel.a

AM_CFLAGS = -DPKGDATADIR='"$(pkgdatadir)"' -I$(top_srcdir)/include @IKS_INC@
AM_CXXFLAGS = -DPKGDATADIR='"$(pkgdatadir)"' -I$(top_srcdir)/include @IKS_INC@

climm_SOURCES = autopackage_prefix.c conv.c file_util.c im_response.c climm.c \
       im_request.c util.c util_opts.c util_ui.c mreadline.c \
       msg_queue.c util_tabs.c i18n.c util_table.c contact.c oscar_dc.c packet.c \
       cmd_user.c preferences.c util_io.c util_str.c util_parse.c \
       util_syntax.c util_rl.c util_alias.c connection.c oscar_dc_file.c \
       oscar_base.c oscar_tlv.c oscar_snac.c \
       io/io_tcp.c io/io_socks5.c io/io_gnutls.c io/io_openssl.c \
       io/io_fifo.c io/io_dns.c \
       oscar_service.c oscar_location.c oscar_contact.c oscar_icbm.c \
       oscar_bos.c oscar_roster.c oscar_oldicq.c oscar_register.c  \
       remote.c os.c util_tcl.c util_md5.c im_icq8.c buildmark.c util_otr.c xmpp_base.c

icqsyn_SOURCES = icqsyn.c conv.c packet.c util_str.c util_syntax.c
icqprx_SOURCES = icqprx.c

climm_LDADD = @LIBOBJS@ @LIBICONV@ @LIBOTR_LIBS@ @LIBGNUTLS_LIBS@ @LIBGCRYPT_LIBS@ @LIBTCL_LIBS@ @BINRELOC_LIBS@ @IKS_LIBS@

noinst_LIBRARIES = @IKS_LIBS@

libiksemel_a_SOURCES = ../iksemel-1.4/src/base64.c ../iksemel-1.4/src/dom.c \
	../iksemel-1.4/src/filter.c ../iksemel-1.4/src/iks.c \
	../iksemel-1.4/src/ikstack.c \
	../iksemel-1.4/src/jabber.c ../iksemel-1.4/src/md5.c \
	../iksemel-1.4/src/sax.c ../iksemel-1.4/src/sha.c \
	../iksemel-1.4/src/stream.c ../iksemel-1.4/src/utility.c

$(climm_SOURCES): @LIBOBJS@

snprintf.o: snprintf.c
snprintf.c: snprintf.h
snprintf.h:
	rm -f snprintf.c snprintf.h
	ln -s $(top_srcdir)/replace/snprintf.c . || cp $(top_srcdir)/replace/snprintf.c .
	ln -s $(top_srcdir)/replace/snprintf.h . || cp $(top_srcdir)/replace/snprintf.h .
memmove.o: memmove.c
memmove.c: memmove.h
memmove.h:
	rm -rf memmove.c memmove.h
	ln -s $(top_srcdir)/replace/memmove.c . || cp $(top_srcdir)/replace/memmove.c .
	ln -s $(top_srcdir)/replace/memmove.h . || cp $(top_srcdir)/replace/memmove.h .

timegm.o: timegm.c
timegm.c:
	rm -rf timegm.c
	ln -s $(top_srcdir)/replace/timegm.c . || cp $(top_srcdir)/replace/timegm.c .

clean-local:
	rm -f snprintf.c snprintf.h memmove.c memmove.h timegm.c .cvsupdate *.stub.c

buildmark.o: .cvsupdate

POSIX=`echo n\  | tail -n 1 2>/dev/null`

.cvsupdate: force
	@grep Id: $(top_srcdir)/src/*.c $(top_srcdir)/include/*.h \
	    | sed "s,.*\(20..[-/]..[-/].. ..:..:..\).*,#define SVNUPDATE \"\1 UTC\"," \
	    | sort | tail -$(POSIX)1 | tr / - | tee .cvsupdate
	@(date -u "+#define BUILDDATE \"%Y-%m-%d %T %Z\"" || \
	 date    "+#define BUILDDATE \"%Y-%m-%d %T %Z\"") >> .cvsupdate
	@grep "^  [$$] " $(top_srcdir)/config.log | head -$(POSIX)1 | tr -d '"' | sed "s,.*,#define CONFIGURELINE \" & \"," >> .cvsupdate

force:

lang:
	@rm -f *.c~ *.cpp~ ../lang/*.i18n~
	@../lang/i18n-extract *.c *.cpp
	@../lang/i18n-number *.c *.cpp
	@if grep -q "i18n (#" *.c *.cpp; then \
	   echo "Ooops, untranslated strings left."; \
	   grep "i18n (#" *.c; \
	   exit 1; fi; true
	@rm -f *.c~ ../lang/*.i18n~
	@../lang/i18n-extract *.c *.cpp
	@rm -rf ../lang/*.i18n~

