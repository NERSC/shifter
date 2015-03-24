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

dockerRoot.conf: dockerRoot.conf.$(SYSTEM).in
	cat $< | sed 's|@@@SYSTEM@@@|$(SYSTEM)|g' | sed 's|@@@PREFIX@@@|$(PREFIX)|g' | sed 's|@@@INCLUDE_FILE@@@|$(INCLUDE_FILE)|g' > $@

dockerRoot.include: dockerRoot.include.$(SYSTEM).in
	cat $< | sed 's|@@@SYSTEM@@@|$(SYSTEM)|g' | sed 's|@@@PREFIX@@@|$(PREFIX)|g' | sed 's|@@@INCLUDE_FILE@@@|$(INCLUDE_FILE)|g' > $@

fsMap.conf: fsMap.conf.$(SYSTEM).in
	cat $< | sed 's|@@@SYSTEM@@@|$(SYSTEM)|g' | sed 's|@@@PREFIX@@@|$(PREFIX)|g' | sed 's|@@@INCLUDE_FILE@@@|$(INCLUDE_FILE)|g' > $@

all: dockerRoot.conf dockerRoot.include fsMap.conf

install: all
	mkdir -p $(CONFIG_DIR)
	if ! $(CMP) -s dockerRoot.conf $(CONFIG_FILE); then \
	    $(INSTALL) --owner=root --group=root --mode=444  dockerRoot.conf $(CONFIG_FILE); \
	fi
	if ! $(CMP) -s dockerRoot.include $(INCLUDE_FILE); then \
	    $(INSTALL) --owner=root --group=root --mode=444  dockerRoot.include $(INCLUDE_FILE); \
	fi
	if ! $(CMP) -s fsMap.conf $(PREFIX)/fsMap.conf; then \
	    $(INSTALL) --owner=root --group=root --mode=400  dockerRoot.include $(PREFIX)/fsMap.conf; \
	fi

clean:
	if [ -a dockerRoot.conf ]; then $(RM) dockerRoot.conf; fi
	if [ -a dockerRoot.include ]; then $(RM) dockerRoot.include; fi
	if [ -a fsMap.conf ]; then $(RM) fsMap.conf; fi
