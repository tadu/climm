# $Id$ 

bin_PROGRAMS = micq

AM_CFLAGS = -DPKGDATADIR='"$(pkgdatadir)"' -I$(top_srcdir)/include

micq_SOURCES = conv.c file_util.c icq_response.c micq.c \
       server.c util.c  util_ui.c mreadline.c mselect.c network.c \
       msg_queue.c tabs.c i18n.c util_table.c contactopts.c contact.c tcp.c packet.c \
       cmd_pkt_cmd_v5.c cmd_pkt_cmd_v5_util.c cmd_pkt_server.c cmd_user.c \
       preferences.c util_io.c util_str.c util_syntax.c util_rl.c connection.c peer_file.c \
       cmd_pkt_v8.c cmd_pkt_v8_flap.c cmd_pkt_v8_tlv.c cmd_pkt_v8_snac.c \
       remote.c os.c util_tcl.c util_ssl.c im_icq8.c buildmark.c

micq_LDADD = @LIBOBJS@
$(micq_SOURCES): @LIBOBJS@

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

clean:
	rm -f snprintf.c snprintf.h memmove.c memmove.h .cvsupdate

buildmark.o: .cvsupdate

.cvsupdate: force
	grep Id: $(top_srcdir)/src/*.c $(top_srcdir)/include/*.h \
	    | sed "s,.*\(20../../.. ..:..:..\).*,#define CVSUPDATE \"\1 UTC\"," \
	    | sort | tail -n 1 | tr / - > .cvsupdate
	date "+#define BUILDDATE \"%Y-%m-%d %T %Z\"" >> .cvsupdate

force:

lang:
	@rm -f *.c~ ../lang/*.i18n~
	@../lang/i18n-extract *.c
	@../lang/i18n-number *.c
	@if grep -q "i18n (#" *.c; then \
	   echo "Ooops, untranslated strings left."; \
	   grep "i18n (#" *.c; \
	   exit 1; fi; true
	@rm -f *.c~ ../lang/*.i18n~
	@../lang/i18n-extract *.c
	@rm -rf ../lang/*.i18n~

