# $Id$
#
# Toplevel Directory Makefile for mICQ

DIR = src

all:	
	@for i in $(DIR); do \
	cd $$i && make all && cd .. ; \
	done

install:
	@for i in $(DIR); do \
	cd $$i && make install && cd .. ; \
	done
	install -g root -o root -m 755 micq.1 /usr/local/man/man1

clean:
	@for i in $(DIR); do \
	cd $$i && make clean && cd .. ; \
	done
