# $Id$ 

bin_PROGRAMS = micq

AM_CFLAGS = -DPKGDATADIR=\"$(pkgdatadir)\"

# If BeOS, add 'beos.c'
micq_SOURCES = conv.c file_util.c icq_response.c micq.c \
       server.c util.c  util_ui.c mreadline.c mselect.c network.c \
       msg_queue.c tabs.c i18n.c util_table.c contact.c tcp.c packet.c \
       cmd_pkt_cmd_v5.c cmd_pkt_cmd_v5_util.c cmd_pkt_server.c cmd_user.c \
       preferences.c util_io.c util_str.c util_syntax.c session.c peer_file.c \
       cmd_pkt_v8.c cmd_pkt_v8_flap.c cmd_pkt_v8_tlv.c cmd_pkt_v8_snac.c \
       remote.c buildmark.c

micq_LDADD = @LIBOBJS@
$(micq_SOURCES): @LIBOBJS@

snprintf.o: snprintf.c snprintf.h
snprintf.c snprintf.h:
	rm -f snprintf.c snprintf.h
	ln -s ../replace/snprintf.c . || cp ../replace/snprintf.c .
	ln -s ../replace/snprintf.h . || cp ../replace/snprintf.h .
memmove.o: memmove.c memmove.h
memmove.c memmove.h:
	rm -rf memmove.c memmove.h
	ln -s ../replace/memmove.c . || cp ../replace/memmove.c .
	ln -s ../replace/memmove.h . || cp ../replace/memmove.h .

clean:
	rm -f snprintf.c snprintf.h memmove.c memmove.h .cvsupdate

buildmark.o: .cvsupdate

.cvsupdate: force
	grep Id: *.c ../include/*.h | sed "s,.*\(20../../.. ..:..:..\).*,#define CVSUPDATE \"\1 UTC\"," \
	    | sort | tail -1 | tr / - > .cvsupdate
	date "+#define BUILDDATE \"%Y-%d-%m %T %Z\"" >> .cvsupdate

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

