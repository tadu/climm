# $Id$ 

bin_PROGRAMS = micq

CFLAGS += -DPREFIX=\"$(prefix)\" -Wall

# If BeOS, add 'beos.c'
micq_SOURCES = conv.c conv_rus.c conv_jp.c file_util.c icq_response.c micq.c \
       server.c util.c  util_ui.c mreadline.c mselect.c network.c \
       msg_queue.c sendmsg.c tabs.c i18n.c buildmark.c util_table.c \
       cmd_user.c

lang:
	@rm -f *.c~ ../lang/*.i18n~
	@../lang/i18n-extract
	@../lang/i18n-number
	@if grep -q "###" *.c; then \
	   echo "Ooops, untranslated strings left."; \
	   grep "###" *.c; \
	   exit 1; fi; true
	@rm -f *.c~ ../lang/*.i18n~
	@../lang/i18n-extract
	@rm -rf ../lang/*.i18n~
	@../lang/i18n-template

buildmark.o: force

force:
