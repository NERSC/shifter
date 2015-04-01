########
## udiRoot: Makefile
##
## Author:  Douglas Jacobsen <dmj@nersc.gov>
## Date  :  2015/03/23
##
########

include ../config.mk

udiRoot.conf: udiRoot.conf.$(SYSTEM).in
	cat $< | sed 's|@@@SYSTEM@@@|$(SYSTEM)|g' | sed 's|@@@PREFIX@@@|$(PREFIX)|g' | sed 's|@@@INCLUDE_FILE@@@|$(INCLUDE_FILE)|g' > $@

udiRoot.include: udiRoot.include.$(SYSTEM).in
	cat $< | sed 's|@@@SYSTEM@@@|$(SYSTEM)|g' | sed 's|@@@PREFIX@@@|$(PREFIX)|g' | sed 's|@@@INCLUDE_FILE@@@|$(INCLUDE_FILE)|g' > $@

fsMap.conf: fsMap.conf.$(SYSTEM).in
	cat $< | sed 's|@@@SYSTEM@@@|$(SYSTEM)|g' | sed 's|@@@PREFIX@@@|$(PREFIX)|g' | sed 's|@@@INCLUDE_FILE@@@|$(INCLUDE_FILE)|g' > $@

all: udiRoot.conf udiRoot.include fsMap.conf

install: all
	mkdir -p $(CONFIG_DIR)
	if ! $(CMP) -s udiRoot.conf $(CONFIG_FILE); then \
	    $(INSTALL) --owner=root --group=root --mode=444  udiRoot.conf $(CONFIG_FILE); \
	fi
	if ! $(CMP) -s udiRoot.include $(INCLUDE_FILE); then \
	    $(INSTALL) --owner=root --group=root --mode=400  udiRoot.include $(INCLUDE_FILE); \
	fi
	if ! $(CMP) -s fsMap.conf $(PREFIX)/fsMap.conf; then \
	    $(INSTALL) --owner=root --group=root --mode=400  fsMap.conf $(PREFIX)/fsMap.conf; \
	fi

clean:
	if [ -a udiRoot.conf ]; then $(RM) udiRoot.conf; fi
	if [ -a udiRoot.include ]; then $(RM) udiRoot.include; fi
	if [ -a fsMap.conf ]; then $(RM) fsMap.conf; fi
