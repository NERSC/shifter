########
## udiRoot: Makefile
##
## Author:  Douglas Jacobsen <dmj@nersc.gov>
## Date  :  2015/03/23
##
########
include ../config.mk
-include ../config.inc

## configuration
CFLAGS=-O2 -Wall
LDFLAGS=
SED_MODIFY=sed 's|@@@CONFIG_FILE@@@|$(CONFIG_FILE)|g' |\
           sed 's|@@@INCLUDE_FILE@@@|$(INCLUDE_FILE)|g' |\
           sed 's|@@@PREFIX@@@|$(PREFIX)|g' |\
           sed  -e '/@@@INCLUDE shifterLibFancyShell.sh@@@/{r shifterLibFancyShell.sh' -e 'd}' |\
           sed  -e '/@@@INCLUDE shifterLibCore.sh@@@/{r shifterLibCore.sh' -e 'd}'
define CLEANFILES
    for file in $(1); do echo $$file; done
endef

all: udiRootEnter setupRoot setupRoot.sh unsetupRoot.sh shiftsh shiftrun getDockerImage.pl

setupRoot.sh: setupRoot.sh.in
	cat $< | $(SED_MODIFY) > $@
unsetupRoot.sh: unsetupRoot.sh.in
	cat $< | $(SED_MODIFY) > $@
getDockerImage.pl: getDockerImage.pl.in
	cat $< | $(SED_MODIFY) > $@
shiftsh: shiftsh.in
	cat $< | $(SED_MODIFY) > $@
shiftrun: shiftrun.in
	cat $< | $(SED_MODIFY) > $@

udiRootEnter: udiRootEnter.c
	$(CC) $(LDFLAGS) $(CFLAGS) -DPREFIX=\"$(PREFIX)\" -DCONFIG_FILE=\"$(CONFIG_FILE)\" -DVERSION=\"$(UDI_VERSION)\" $< -o $@
distributeKeys: distributeKeys.c
	$(CC) $(LDFLAGS) $(CFLAGS) -DPREFIX=\"$(PREFIX)\" -DCONFIG_FILE=\"$(CONFIG_FILE)\" -DVERSION=\"$(UDI_VERSION)\" -DKEYLEN=$(KEYLEN) $< -o $@
setupRoot: setupRoot.c UdiRootConfig.c utility.c ImageData.c
	$(CC) $(LDFLAGS) $(CFLAGS) -DPREFIX=\"$(PREFIX)\" -DCONFIG_FILE=\"$(CONFIG_FILE)\" -DVERSION=\"$(UDI_VERSION)\" -DKEYLEN=$(KEYLEN) -DNO_ROOT_OWN_CHECK $^ -o $@


	
install: all
	mkdir -m 755 -p $(DESTDIR)/sbin
	mkdir -m 755 -p $(DESTDIR)/bin
	ln -s getDockerImage.pl docker
	$(INSTALL) --owner=root --group=root --mode=555  setupRoot.sh $(DESTDIR)/sbin/setupRoot.sh
	$(INSTALL) --owner=root --group=root --mode=555  unsetupRoot.sh $(DESTDIR)/sbin/unsetupRoot.sh
	$(INSTALL) --owner=root --group=root --mode=555  getDockerImage.pl $(DESTDIR)/bin/getDockerImage.pl
	$(INSTALL) --owner=root --group=root --mode=4555  udiRootEnter $(DESTDIR)/sbin/udiRootEnter
	$(INSTALL) --owner=root --group=root --mode=555  shiftsh $(DESTDIR)/bin/shiftsh
	$(INSTALL) --owner=root --group=root --mode=555  shiftrun $(DESTDIR)/bin/shiftrun
	$(INSTALL) --owner=root --group=root docker $(DESTDIR)/bin/docker

clean: setupRoot.sh unsetupRoot.sh getDockerImage.pl udiRootEnter shiftsh shiftrun
	for file in $^; do if [[ -e "$$file" ]]; then rm "$$file"; fi; done
