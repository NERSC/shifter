########
## udiRoot: Makefile
##
## Author:  Douglas Jacobsen <dmj@nersc.gov>
## Date  :  2015/03/23
##
########
include ../../config.mk
-include ../../config.inc

## configuration
CFLAGS=-O2 -Wall -I$(SLURM_ROOT)/include -I../../src
LDFLAGS=-L$(SLURM_ROOT)/lib64 -lslurm -Wl,-rpath=$(SLURM_ROOT)/lib64

ALL=shifter.so
ifeq ($(IS_NATIVE_SLURM),1)
    ALL += udiRoot-epilogue udiRoot-prologue
endif

all: $(ALL)

SED_UPDATE=sed 's|@@@SYSTEM@@@|$(SYSTEM)|g' |\
           sed 's|@@@PREFIX@@@|$(PREFIX)|g' |\
           sed 's|@@@CONFIG_FILE@@@|$(CONFIG_FILE)|g'

udiRoot-epilogue: udiRoot-epilogue.in
	cat $< | $(SED_UPDATE) > $@

udiRoot-prologue: udiRoot-prologue.in
	cat $< | $(SED_UPDATE) > $@

shifter.so: spank_shifter.c ../../src/UdiRootConfig.c ../../src/utility.c
	$(CC) $(LDFLAGS) $(CFLAGS) -DPREFIX=\"$(PREFIX)\" \
        -DCONFIG_FILE=\"$(CONFIG_FILE)\" -DVERSION=\"$(UDI_VERSION)\" \
        -DIS_NATIVE_SLURM=$(IS_NATIVE_SLURM) -shared -fPIC $(LDFLAGS) $^ -o $@
	
install: all
	mkdir -m 755 -p $(DESTDIR)/share/slurm
	mkdir -m 755 -p $(DESTDIR)/libexec
	$(INSTALL) --owner=root --group=root --mode=555  shifter.so $(DESTDIR)/share/slurm/shifter.so
	if [ -a udiRoot-epilogue ]; then $(INSTALL) --owner=root --group=root --mode=555 udiRoot-epilogue $(DESTDIR)/libexec/udiRoot-epilogue; fi
	if [ -a udiRoot-prologue ]; then $(INSTALL) --owner=root --group=root --mode=555 udiRoot-prologue $(DESTDIR)/libexec/udiRoot-prologue; fi

clean: shifter.so
	for file in $^; do if [[ -e "$$file" ]]; then rm "$$file"; fi; done
