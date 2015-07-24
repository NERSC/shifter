########
## udiRoot: Makefile
##
## Author:  Douglas Jacobsen <dmj@nersc.gov>
## Date  :  2015/03/23
##
########

include ../../config.mk
-include ../../config.inc
SED_UPDATE=sed 's|@@@SYSTEM@@@|$(SYSTEM)|g' |\
           sed 's|@@@PREFIX@@@|$(PREFIX)|g' |\
           sed 's|@@@CONFIG_FILE@@@|$(CONFIG_FILE)|g'


ALL=udiRoot-nhc-plugin
ifneq ($(IS_NATIVE_SLURM),1)
    ALL += udiRoot-epilogue udiRoot-prologue
endif

all: $(ALL)

udiRoot-epilogue: udiRoot-epilogue.in
	cat $< | $(SED_UPDATE) > $@

udiRoot-prologue: udiRoot-prologue.in
	cat $< | $(SED_UPDATE) > $@

udiRoot-nhc-plugin:udiRoot-nhc-plugin.in
	cat $< | $(SED_UPDATE) > $@


install: all
	mkdir -p $(DESTDIR)/libexec
	$(INSTALL) --owner=root --group=root --mode=555  udiRoot-nhc-plugin $(DESTDIR)/libexec/udiRoot-nhc-plugin
	ifneq ($(IS_NATIVE_SLURM),1)
		$(INSTALL) --owner=root --group=root --mode=555  udiRoot-prologue $(DESTDIR)/libexec/udiRoot-prologue
		$(INSTALL) --owner=root --group=root --mode=555  udiRoot-epilogue $(DESTDIR)/libexec/udiRoot-epilogue
	endif

clean:
	if [ -a udiRoot-prologue ]; then $(RM) udiRoot-prologue; fi
	if [ -a udiRoot-epilogue ]; then $(RM) udiRoot-epilogue; fi
	if [ -a udiRoot-nhc-plugin ]; then $(RM) udiRoot-nhc-plugin; fi
