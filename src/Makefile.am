# $Id$ 

bin_PROGRAMS = micq

AM_CFLAGS = -DPKGDATADIR=\"$(pkgdatadir)\"

# If BeOS, add 'beos.c'
micq_SOURCES = conv.c conv_rus.c conv_jp.c file_util.c icq_response.c micq.c \
       server.c util.c  util_ui.c mreadline.c mselect.c network.c \
       msg_queue.c tabs.c i18n.c util_table.c contact.c tcp.c packet.c \
       cmd_pkt_cmd_v5.c cmd_pkt_cmd_v5_util.c cmd_pkt_server.c cmd_user.c \
       preferences.c util_io.c session.c peer_file.c \
       cmd_pkt_v8.c cmd_pkt_v8_flap.c cmd_pkt_v8_tlv.c cmd_pkt_v8_snac.c \
       buildmark.c

micq_LDADD = @LIBOBJS@
$(micq_SOURCES): @LIBOBJS@

lang:
	@rm -f *.c~ ../lang/*.i18n~
	@../lang/i18n-extract
	@../lang/i18n-number
	@if grep -q "i18n (#" *.c; then \
	   echo "Ooops, untranslated strings left."; \
	   grep "i18n (#" *.c; \
	   exit 1; fi; true
	@rm -f *.c~ ../lang/*.i18n~
	@../lang/i18n-extract
	@rm -rf ../lang/*.i18n~

snprintf.o: snprintf.c snprintf.h

snprintf.c:
	rm -f snprintf.c
	ln -s ../replace/snprintf.c . || cp ../replace/snprintf.c .

snprintf.h:
	rm -f snprintf.h
	ln -s ../replace/snprintf.h . || cp ../replace/snprintf.h .

clean:
	rm -f snprintf.c snprintf.h

buildmark.o: force

force:
