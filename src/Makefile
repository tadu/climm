# $Id$
# originally by Matt D. Smith
# rewritten by  Rico Gloeckner <mc@verdinet.de>

CC = gcc
CFLAGS = -O2 -Wall -DUNIX

# uncomment the line below for debuging info
CFLAGS += -g

CLIBS = 
#uncomment the line below if your using Solaris
#CLIBS= -lnsl -lsocket
#uncomment the line below if your using MiNT
#CLIBS= -lsocket
#uncomment the line below if your using NeXT
#CFLAGS += -posix
OBJS = conv.o conv_rus.o conv_jp.o file_util.o icq_response.o micq.o \
       server.o  ui.o util.o  util_ui.o mreadline.o mselect.o network.o \
       msg_queue.o sendmsg.o tabs.o i18n.o buildmark.o util_table.o
      
HEADERS = config.h datatype.h micq.h ui.h msg_queue.h mreadline.h \
          mselect.h util_ui.h tabs.h i18n.h buildmark.h util_table.h \
          conv.h

INCLUDE = -I../include -I../lang

#uncomment next two lines if you are using BeOS
#OBJS += beos.o
#HEADERS +=beos.h

all : micq

re : clean all

micq : ${OBJS}    
	${CC} ${CFLAGS} ${INCLUDE} -o micq ${OBJS} ${CLIBS}

.c.o:     
	${CC} ${CFLAGS} ${INCLUDE} -c $< -o $@

buildmark.o: force

force:

clean :
	rm -f ${OBJS} micq *~

install:
	install -g root -o root -m 755 micq /usr/local/bin

indent:
	@for i in *.c; do \
	    indent $$i;\
	done
