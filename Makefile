# $Id$
#
# Toplevel Directory Makefile for mICQ

DIR = src lang
DESTDIR =
prefix = /usr/local

all:	
	@for i in $(DIR); do \
	cd $$i && $(MAKE) all && cd .. ; \
	done

install:
	@for i in $(DIR); do \
	cd $$i && $(MAKE) install && cd .. ; \
	done
	install -d -g root -o root -m 755 $(DESTDIR)/$(prefix)/man/man1
	install -g root -o root -m 644 micq.1 $(DESTDIR)/$(prefix)/man/man1

clean:
	@for i in $(DIR); do \
	cd $$i && $(MAKE) clean && cd .. ; \
	done

indent:
	@for i in $(DIR); do \
	cd $$i && $(MAKE) indent && cd ..; \
	done
