# $Id$ 

AUTOMAKE_OPTIONS = subdir-objects

bin_PROGRAMS = climm # icqsyn icqprx

AM_CFLAGS = -DPKGDATADIR='"$(pkgdatadir)"' -I$(top_srcdir)/include

climm_SOURCES = autopackage_prefix.c conv.c file_util.c im_response.c climm.c \
       im_request.c util.c util_opts.c util_ui.c mreadline.c \
       msg_queue.c tabs.c i18n.c util_table.c contact.c tcp.c packet.c \
       cmd_user.c preferences.c util_io.c util_str.c util_parse.c \
       util_syntax.c util_rl.c util_alias.c connection.c peer_file.c \
       oldicq_base.c oldicq_compat.c oldicq_client.c oldicq_server.c oldicq_util.c \
       oscar_base.c oscar_tlv.c oscar_snac.c \
       oscar_service.c oscar_location.c oscar_contact.c oscar_icbm.c \
       oscar_bos.c oscar_roster.c oscar_oldicq.c oscar_register.c  \
       remote.c os.c util_tcl.c util_ssl.c im_icq8.c buildmark.c util_otr.c

icqsyn_SOURCES = icqsyn.c conv.c packet.c util_str.c util_syntax.c
icqprx_SOURCES = icqprx.c

EXTRA_LIBRARIES = libclimmmsn.a libclimmxmpp.a
noinst_LIBRARIES = @LIBMSN@ @LIBXMPP@

libclimmmsn_a_SOURCES = msn_base.cpp

libclimmxmpp_a_SOURCES = jabber_base.cpp

climm_LDADD = @LIBOBJS@ @LIBICONV@ @L_LIBMSN@ @L_LIBXMPP@ @LIBOTR_LIBS@ @LIBGNUTLS_LIBS@ @LIBGCRYPT_LIBS@ @XMPP_LIBS@ @LIBTCL_LIBS@ @BINRELOC_LIBS@

$(climm_SOURCES): @LIBOBJS@ @LIBMSN@

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
	    | sed "s,.*\(20..[-/]..[-/].. ..:..:..\).*,#define CVSUPDATE \"\1 UTC\"," \
	    | sort | tail -$(POSIX)1 | tr / - > .cvsupdate
	@(date -u "+#define BUILDDATE \"%Y-%m-%d %T %Z\"" || \
	 date    "+#define BUILDDATE \"%Y-%m-%d %T %Z\"") >> .cvsupdate

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

