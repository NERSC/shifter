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
CFLAGS=-O2 -Wall -I$(SLURM_ROOT)/include
LDFLAGS=
define CLEANFILES
    for file in $(1); do echo $$file; done
endef

all: shifter.so

shifter.so: spank_shifter.c
	$(CC) $(LDFLAGS) $(CFLAGS) -DPREFIX=\"$(PREFIX)\" \
        -DCONFIG_FILE=\"$(CONFIG_FILE)\" -DVERSION=\"$(UDI_VERSION)\" \
        -DIS_NATIVE_SLURM=$(IS_NATIVE_SLURM) -shared -fPIC $< -o $@
	
install: all
	mkdir -m 755 -p $(DESTDIR)/shared/slurm
	$(INSTALL) --owner=root --group=root --mode=555  shifter.so $(DESTDIR)/shared/slurm/shifter.so

clean: shifter.so
	for file in $^; do if [[ -e "$$file" ]]; then rm "$$file"; fi; done
