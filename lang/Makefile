# $Id$
#

all:	
	
clean:	
	
indent:	
	

install:
	@if [ $$UID = 0 ]; then \
	 mkdir $(DESTDIR)/$(prefix)/share/micq ; \
	 cp *.i18n $(DESTDIR)/$(prefix)/share/micq ;\
	else \
	 mkdir ${HOME}/.micq/ ; \
	 cp *.i18n ${HOME}/.micq/ ; \
	fi
