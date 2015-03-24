########
## dockerRoot: Makefile
##
## Author:  Douglas Jacobsen <dmj@nersc.gov>
## Date  :  2015/03/23
##
########

include ../config.mk

CC      = /usr/bin/gcc
RM      = /bin/rm
CMP     = /usr/bin/cmp
INSTALL = /usr/bin/install

dockerRoot-epilogue: dockerRoot-epilogue.in
	cat $< | sed 's|@@@SYSTEM@@@|$(SYSTEM)|g' | sed 's|@@@PREFIX@@@|$(PREFIX)|g' > $@

dockerRoot-prologue: dockerRoot-prologue.in
	cat $< | sed 's|@@@SYSTEM@@@|$(SYSTEM)|g' | sed 's|@@@PREFIX@@@|$(PREFIX)|g' > $@

all: dockerRoot-epilogue dockerRoot-prologue

install: all
	mkdir -p $(PREFIX)/libexec
	if ! $(CMP) -s dockerRoot-prologue $(PREFIX)/libexec/dockerRoot-prologue; then \
	    $(INSTALL) --owner=root --group=root --mode=555  dockerRoot-prologue $(PREFIX)/libexec/dockerRoot-prologue; \
	fi
	if ! $(CMP) -s dockerRoot-epilogue $(PREFIX)/libexec/dockerRoot-epilogue; then \
	    $(INSTALL) --owner=root --group=root --mode=555  dockerRoot-epilogue $(PREFIX)/libexec/dockerRoot-epilogue; \
	fi

clean:
	if [ -a dockerRoot-prologue ]; then $(RM) dockerRoot-prologue; fi
	if [ -a dockerRoot-epilogue ]; then $(RM) dockerRoot-epilogue; fi
