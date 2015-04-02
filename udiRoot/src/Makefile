########
## udiRoot: Makefile
##
## Author:  Douglas Jacobsen <dmj@nersc.gov>
## Date  :  2015/03/23
##
########
include ../config.mk

## configuration
CFLAGS=-O2 -Wall
LDFLAGS=

all: udiRootEnter setupRoot.sh unsetupRoot.sh getDockerImage.pl

setupRoot.sh: setupRoot.sh.in
	cat $< | sed 's|@@@CONFIG_FILE@@@|$(CONFIG_FILE)|g' | sed 's|@@@INCLUDE_FILE@@@|$(INCLUDE_FILE)|g' | sed 's|@@@PREFIX@@@|$(PREFIX)|g' > $@

unsetupRoot.sh: unsetupRoot.sh.in
	cat $< | sed 's|@@@CONFIG_FILE@@@|$(CONFIG_FILE)|g' | sed 's|@@@INCLUDE_FILE@@@|$(INCLUDE_FILE)|g' | sed 's|@@@PREFIX@@@|$(PREFIX)|g' > $@

udiRootEnter: udiRootEnter.c
	$(CC) $(LDFLAGS) $(CFLAGS) -DPREFIX=\"$(PREFIX)\" -DCONFIG_FILE=\"$(CONFIG_FILE)\" -DVERSION=\"$(UDI_VERSION)\" $< -o $@

getDockerImage.pl: getDockerImage.pl.in
	cat $< | sed 's|@@@CONFIG_FILE@@@|$(CONFIG_FILE)|g' | sed 's|@@@PREFIX@@@|$(PREFIX)|g' > $@
	
install: all
	mkdir -p $(PREFIX)/sbin
	mkdir -p $(PREFIX)/bin
	if ! $(CMP) -s setupRoot.sh $(PREFIX)/sbin/setupRoot.sh; then \
		$(INSTALL) --owner=root --group=root --mode=555  setupRoot.sh $(PREFIX)/sbin/setupRoot.sh; \
	fi
	if ! $(CMP) -s unsetupRoot.sh $(PREFIX)/sbin/unsetupRoot.sh; then \
		$(INSTALL) --owner=root --group=root --mode=555  unsetupRoot.sh $(PREFIX)/sbin/unsetupRoot.sh; \
	fi
	if ! $(CMP) -s getDockerImage.pl $(PREFIX)/bin/getDockerImage.pl; then \
		$(INSTALL) --owner=root --group=root --mode=555  getDockerImage.pl $(PREFIX)/bin/getDockerImage.pl; \
	fi
	if ! $(CMP) -s udiRootEnter $(PREFIX)/sbin/udiRootEnter; then \
		$(INSTALL) --owner=root --group=root --mode=4555  udiRootEnter $(PREFIX)/sbin/udiRootEnter; \
	fi
	if ! $(CMP) -s shiftsh $(PREFIX)/bin/shiftsh; then \
		$(INSTALL) --owner=root --group=root --mode=555  shiftsh $(PREFIX)/bin/shiftsh; \
	fi

clean:
	if [ -a setupRoot.sh ]; then $(RM) setupRoot.sh; fi
	if [ -a unsetupRoot.sh ]; then $(RM) unsetupRoot.sh; fi
	if [ -a getDockerImage.pl ]; then $(RM) getDockerImage.pl; fi
	if [ -a udiRootEnter ]; then $(RM) udiRootEnter; fi

