########
## dockerRoot: Makefile
##
## Author:  Douglas Jacobsen <dmj@nersc.gov>
## Date  :  2015/03/23
##
########

PREFIX=/global/syscom/sc/nsg/opt/dockerRoot
CONFIG_FILE=/global/syscom/sc/nsg/etc/dockerRoot.conf

CC      = /usr/bin/gcc
RM      = /bin/rm
CMP     = /usr/bin/cmp
INSTALL = /usr/bin/install

## configuration
CFLAGS=-O2 -Wall
LDFLAGS=

all: dockerRootClient setupRoot.sh unsetupRoot.sh getDockerImage.pl

setupRoot.sh: setupRoot.sh.in
	cat $< | sed 's|@@@CONFIG_FILE@@@|$(CONFIG_FILE)|g' | sed 's|@@@PREFIX@@@|$(PREFIX)|g' > $@

unsetupRoot.sh: unsetupRoot.sh.in
	cat $< | sed 's|@@@CONFIG_FILE@@@|$(CONFIG_FILE)|g' | sed 's|@@@PREFIX@@@|$(PREFIX)|g' > $@

dockerRootClient: dockerRootClient.c
	$(CC) $(LDFLAGS) $(CFLAGS) -DPREFIX=\"$(PREFIX)\" -DCONFIG_FILE=\"$(CONFIG_FILE)\" $< -o $@

getDockerImage.pl: getDockerImage.pl.in
	cat $< | sed 's|@@@CONFIG_FILE@@@|$(CONFIG_FILE)|g' | sed 's|@@@PREFIX@@@|$(PREFIX)|g' > $@
	
install: all
	if ! $(CMP) -s setupRoot.sh $(PREFIX)/sbin/setupRoot.sh; then \
		$(INSTALL) --owner=root --group=root --mode=555  setupRoot.sh $(PREFIX)/sbin/setupRoot.sh; \
	fi
	if ! $(CMP) -s unsetupRoot.sh $(PREFIX)/sbin/unsetupRoot.sh; then \
		$(INSTALL) --owner=root --group=root --mode=555  setupRoot.sh $(PREFIX)/sbin/unsetupRoot.sh; \
	fi
	if ! $(CMP) -s getDockerImage.pl $(PREFIX)/bin/getDockerImage.pl; then \
		$(INSTALL) --owner=root --group=root --mode=555  getDockerImage.pl $(PREFIX)/bin/getDockerImage.pl; \
	fi
	if ! $(CMP) -s dockerRootClient $(PREFIX)/sbin/dockerRootClient; then \
		$(INSTALL) --owner=root --group=root --mode=4555  dockerRootClient $(PREFIX)/sbin/dockerRootClient; \
	fi

clean:
	if [ -a setupRoot.sh ]; then $(RM) setupRoot.sh; fi
	if [ -a unsetupRoot.sh ]; then $(RM) unsetupRoot.sh; fi
	if [ -a getDockerImage.pl ]; then $(RM) getDockerImage.pl; fi
	if [ -a dockerRootClient ]; then $(RM) dockerRootClient; fi

