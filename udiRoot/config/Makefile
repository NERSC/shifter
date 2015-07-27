########
## udiRoot: Makefile
##
## Author:  Douglas Jacobsen <dmj@nersc.gov>
## Date  :  2015/03/23
##
########

include ../config.mk
-include ../config.inc

all: udiRoot.conf premount.sh postmount.sh

udiRoot.conf: udiRoot.conf.$(SYSTEM).in
	cat $< | sed 's|@@@SYSTEM@@@|$(SYSTEM)|g' | sed 's|@@@PREFIX@@@|$(PREFIX)|g' | sed 's|@@@INCLUDE_FILE@@@|$(INCLUDE_FILE)|g' | sed 's|@@@CONFIG_DIR@@@|$(CONFIG_DIR)|g' > $@

premount.sh: premount.sh.$(SYSTEM).in
	cat $< | sed 's|@@@SYSTEM@@@|$(SYSTEM)|g' | sed 's|@@@PREFIX@@@|$(PREFIX)|g' | sed 's|@@@INCLUDE_FILE@@@|$(INCLUDE_FILE)|g' | sed 's|@@@CONFIG_DIR@@@|$(CONFIG_DIR)|g' > $@

postmount.sh: postmount.sh.$(SYSTEM).in
	cat $< | sed 's|@@@SYSTEM@@@|$(SYSTEM)|g' | sed 's|@@@PREFIX@@@|$(PREFIX)|g' | sed 's|@@@INCLUDE_FILE@@@|$(INCLUDE_FILE)|g' | sed 's|@@@CONFIG_DIR@@@|$(CONFIG_DIR)|g' > $@


install: all
	mkdir -p $(DESTDIR)/etc
	$(INSTALL) --owner=root --group=root --mode=444  udiRoot.conf $(CONFIGINST_DIR)/udiRoot.conf
	$(INSTALL) --owner=root --group=root --mode=400  premount.sh $(CONFIGINST_DIR)/premount.sh; \
	$(INSTALL) --owner=root --group=root --mode=400  postmount.sh $(CONFIGINST_DIR)/postmount.sh; \

clean:
	if [ -a udiRoot.conf ]; then $(RM) udiRoot.conf; fi
	if [ -a premount.sh ]; then $(RM) premount.sh; fi
	if [ -a postmount.sh ]; then $(RM) postmount.sh; fi
