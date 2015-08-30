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
CFLAGS=-O0 -ggdb -W -Wall -Wundef -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -ansi
LDFLAGS=

SED_MODIFY=sed 's|@@@CONFIG_FILE@@@|$(CONFIG_FILE)|g' |\
           sed 's|@@@CONFIG_DIR@@@|$(CONFIG_DIR)|g' |\
           sed 's|@@@INCLUDE_FILE@@@|$(INCLUDE_FILE)|g' |\
           sed 's|@@@PREFIX@@@|$(PREFIX)|g' |\
           sed  -e '/@@@INCLUDE shifterLibFancyShell.sh@@@/{r shifterLibFancyShell.sh' -e 'd}' |\
           sed  -e '/@@@INCLUDE shifterLibCore.sh@@@/{r shifterLibCore.sh' -e 'd}'
define CLEANFILES
    for file in $(1); do echo $$file; done
endef

all: setupRoot unsetupRoot shifter getDockerImage.pl docker

test:
	cd test; make test || exit 1

getDockerImage.pl: getDockerImage.pl.in
	cat $< | $(SED_MODIFY) > $@
docker: getDockerImage.pl
	cp getDockerImage.pl docker
config.h: config.h.in
	cat $< | $(SED_MODIFY) > $@

setupRoot: setupRoot.c UdiRootConfig.c utility.c ImageData.c config.h shifter_core.c VolumeMap.c MountList.c
	$(CC) $(LDFLAGS) $(CFLAGS) -DPREFIX=\"$(PREFIX)\" -DCONFIG_FILE=\"$(CONFIG_FILE)\" -DVERSION=\"$(UDI_VERSION)\" $^ -o $@
unsetupRoot: unsetupRoot.c UdiRootConfig.c utility.c config.h shifter_core.c VolumeMap.c MountList.c
	$(CC) $(LDFLAGS) $(CFLAGS) -DPREFIX=\"$(PREFIX)\" -DCONFIG_FILE=\"$(CONFIG_FILE)\" -DVERSION=\"$(UDI_VERSION)\" $^ -o $@
shifter: shifter.c UdiRootConfig.c utility.c config.h shifter_core.c ImageData.c VolumeMap.c MountList.c
	$(CC) $(LDFLAGS) $(CFLAGS) -DPREFIX=\"$(PREFIX)\" -DCONFIG_FILE=\"$(CONFIG_FILE)\" -DVERSION=\"$(UDI_VERSION)\" $^ -o $@

install: all
	mkdir -m 755 -p $(DESTDIR)/sbin
	mkdir -m 755 -p $(DESTDIR)/bin
	$(INSTALL) --owner=root --group=root --mode=555  setupRoot $(DESTDIR)/sbin/setupRoot
	$(INSTALL) --owner=root --group=root --mode=555  unsetupRoot $(DESTDIR)/sbin/unsetupRoot
	$(INSTALL) --owner=root --group=root --mode=4555  shifter $(DESTDIR)/bin/shifter
	$(INSTALL) --owner=root --group=root --mode=555  getDockerImage.pl $(DESTDIR)/bin/getDockerImage.pl
	$(INSTALL) --owner=root --group=root docker $(DESTDIR)/bin/docker

clean: setupRoot unsetupRoot shifter docker getDockerImage.pl config.h
	set -x; for file in $^; do if [[ -e "$$file" ]]; then rm "$$file"; fi; done
