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
CFLAGS=-O2 -W -Wall -Wundef -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -ansi
LDFLAGS=
ROOTFS_TYPE="\"tmpfs\""
ifeq ($(CRAY_SUPPORT),1)
	ROOTFS_TYPE="\"rootfs\""
endif

SED_MODIFY=sed 's|@@@CONFIG_FILE@@@|$(CONFIG_FILE)|g' |\
           sed 's|@@@CONFIG_DIR@@@|$(CONFIG_DIR)|g' |\
           sed 's|@@@INCLUDE_FILE@@@|$(INCLUDE_FILE)|g' |\
           sed 's|@@@PREFIX@@@|$(PREFIX)|g' |\
           sed  -e '/@@@INCLUDE shifterLibFancyShell.sh@@@/{r shifterLibFancyShell.sh' -e 'd}' |\
           sed  -e '/@@@INCLUDE shifterLibCore.sh@@@/{r shifterLibCore.sh' -e 'd}'
define CLEANFILES
    for file in $(1); do echo $$file; done
endef

all: setupRoot unsetupRoot shifter shiftsh shiftrun getDockerImage.pl shifterShim

test:
	cd test; make test || exit 1

getDockerImage.pl: getDockerImage.pl.in
	cat $< | $(SED_MODIFY) > $@
docker: getDockerImage.pl
	cp getDockerImage.pl docker
shiftsh: shiftsh.in
	cat $< | $(SED_MODIFY) > $@
shiftrun: shiftrun.in
	cat $< | $(SED_MODIFY) > $@
config.h: config.h.in
	cat $< | $(SED_MODIFY) > $@

setupRoot: setupRoot.c UdiRootConfig.c utility.c ImageData.c config.h shifter_core.c
	$(CC) $(LDFLAGS) $(CFLAGS) -DPREFIX=\"$(PREFIX)\" -DCONFIG_FILE=\"$(CONFIG_FILE)\" -DVERSION=\"$(UDI_VERSION)\" -DKEYLEN=$(KEYLEN) -DROOTFS_TYPE=$(ROOTFS_TYPE) -DNO_ROOT_OWN_CHECK $^ -o $@
unsetupRoot: unsetupRoot.c UdiRootConfig.c utility.c config.h shifter_core.c
	$(CC) $(LDFLAGS) $(CFLAGS) -DPREFIX=\"$(PREFIX)\" -DCONFIG_FILE=\"$(CONFIG_FILE)\" -DVERSION=\"$(UDI_VERSION)\" -DKEYLEN=$(KEYLEN) -DROOTFS_TYPE=$(ROOTFS_TYPE) -DNO_ROOT_OWN_CHECK $^ -o $@
shifter: shifter.c UdiRootConfig.c utility.c config.h shifter_core.c ImageData.c
	$(CC) $(LDFLAGS) $(CFLAGS) -DPREFIX=\"$(PREFIX)\" -DCONFIG_FILE=\"$(CONFIG_FILE)\" -DVERSION=\"$(UDI_VERSION)\" -DKEYLEN=$(KEYLEN) -DROOTFS_TYPE=$(ROOTFS_TYPE) -DNO_ROOT_OWN_CHECK $^ -o $@
shifterShim: shifterShim.c
	$(CC) $(LDFLAGS) $(CFLAGS) -DPREFIX=\"$(PREFIX)\" -DCONFIG_FILE=\"$(CONFIG_FILE)\" -DVERSION=\"$(UDI_VERSION)\" -DKEYLEN=$(KEYLEN) -DROOTFS_TYPE=$(ROOTFS_TYPE) -DNO_ROOT_OWN_CHECK $^ -o $@

install: all
	mkdir -m 755 -p $(DESTDIR)/sbin
	mkdir -m 755 -p $(DESTDIR)/bin
	$(INSTALL) --owner=root --group=root --mode=555  setupRoot $(DESTDIR)/sbin/setupRoot
	$(INSTALL) --owner=root --group=root --mode=555  unsetupRoot $(DESTDIR)/sbin/unsetupRoot
	$(INSTALL) --owner=root --group=root --mode=4555  shifter $(DESTDIR)/bin/shifter
	$(INSTALL) --owner=root --group=root --mode=555  getDockerImage.pl $(DESTDIR)/bin/getDockerImage.pl
	$(INSTALL) --owner=root --group=root --mode=555  shiftsh $(DESTDIR)/bin/shiftsh
	$(INSTALL) --owner=root --group=root --mode=555  shiftrun $(DESTDIR)/bin/shiftrun
	$(INSTALL) --owner=root --group=root --mode=555  shifterShim $(DESTDIR)/bin/shifterShim
	$(INSTALL) --owner=root --group=root docker $(DESTDIR)/bin/docker

clean: setupRoot unsetupRoot shifter docker getDockerImage.pl shiftsh shiftrun config.h shifterShim
	set -x; for file in $^; do if [[ -e "$$file" ]]; then rm "$$file"; fi; done
