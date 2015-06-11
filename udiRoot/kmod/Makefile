########
## udiRoot: Makefile
##
## Author:  Douglas Jacobsen <dmj@nersc.gov>
## Date  :  2015/03/23
##
########

include ../config.mk
-include ../config.inc
KVER := $(shell uname -r | sed 's|\(.*\)-cray_.*|\1|g' )

## configuration
CFLAGS=-O2 -Wall
LDFLAGS=

all: modules_c.tar modules_s.tar

modules_c.tar:
	./build_kernel_modules.sh $(CRAY_NETTYPE) c
	mv modules.tar modules_c.tar
modules_s.tar:
	./build_kernel_modules.sh $(CRAY_NETTYPE) s
	mv modules.tar modules_s.tar

install: all
	mkdir -p $(DESTDIR)/kmod/$(KVER)-cray_$(CRAY_NETTYPE)_$(CRAY_NODETYPE)
	tar xvf modules_c.tar -C $(DESTDIR)/kmod/$(KVER)-cray_$(CRAY_NETTYPE)_c
	tar xvf modules_s.tar -C $(DESTDIR)/kmod/$(KVER)-cray_$(CRAY_NETTYPE)_s
	chmod -R a+rX $(DESTDIR)/kmod

clean:
	if [ -a modules_c.tar ]; then $(RM) modules_c.tar; fi
	if [ -a modules_s.tar ]; then $(RM) modules_s.tar; fi
