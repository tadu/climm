# Anyone who knows how to make Makefiles please help out.
# I have no clue but this appears to work fairly well
# be sure to use gmake
#
# Most options that were here and soem that weren't have been moved to config.h
# edit that file to set up options.
#
CC = gcc
CFLAGS = -O4 -Wall -DUNIX 

# uncomment the line below for debuging info
#CFLAGS += -g

CLIBS = 
#uncomment the line below if your using Solaris
#CLIBS= -lnsl -lsocket
#uncomment the line below if your using MiNT
#CLIBS= -lsocket
#uncomment the line below if your using NeXT
#CFLAGS += -posix
OBJS =file_util.o icq_response.o micq.o msg_queue.o rus_conv.o sendmsg.o \
      server.o  ui.o util.o  util_ui.o mreadline.o mselect.o network.o \
      char_conv.o jp_conv.o
      
HEADERS = config.h datatype.h micq.h ui.h msg_queue.h mreadline.h mselect.h util_ui.h

INCLUDE = -I. -I../include -I../lang

#uncomment next two lines if you are using BeOS
#OBJS += beos.o
#HEADERS +=beos.h

all : micq

re : clean all

micq : ${OBJS}    
	${CC} ${CFLAGS} ${INCLUDE} -o micq ${OBJS} ${CLIBS}

.c.o:     
	${CC} ${CFLAGS} ${INCLUDE} -c $< -o $@

# a dependency of the objects of the headers is senseless imho.
# ${OBJS} :

clean :
	rm -f ${OBJS} micq

install:
	@install -g root -o root -m 755 micq /usr/local/bin
