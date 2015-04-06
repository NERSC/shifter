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

all: udiRootEnter setupRoot.sh unsetupRoot.sh shiftsh shiftrun getDockerImage.pl

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

	
install: all
	mkdir -p $(PREFIX)/sbin
	mkdir -p $(PREFIX)/bin
	$(INSTALL) --owner=root --group=root --mode=555  setupRoot.sh $(PREFIX)/sbin/setupRoot.sh
	$(INSTALL) --owner=root --group=root --mode=555  unsetupRoot.sh $(PREFIX)/sbin/unsetupRoot.sh
	$(INSTALL) --owner=root --group=root --mode=555  getDockerImage.pl $(PREFIX)/bin/getDockerImage.pl
	$(INSTALL) --owner=root --group=root --mode=4555  udiRootEnter $(PREFIX)/sbin/udiRootEnter
	$(INSTALL) --owner=root --group=root --mode=555  shiftsh $(PREFIX)/bin/shiftsh
	$(INSTALL) --owner=root --group=root --mode=555  shiftrun $(PREFIX)/bin/shiftrun

clean: setupRoot.sh unsetupRoot.sh getDockerImage.pl udiRootEnter shiftsh shiftrun
	for file in $^; do if [[ -e "$$file" ]]; then rm "$$file"; fi; done
