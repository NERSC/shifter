########
## udiRoot: Makefile
##
## Author:  Douglas Jacobsen <dmj@nersc.gov>
## Date  :  2015/03/23
##
########

include ../config.mk
KVER := $(shell uname -r | sed 's|\(.*\)-cray_.*|\1|g' )

## configuration
CFLAGS=-O2 -Wall
LDFLAGS=

all: modules.tar

modules.tar:
	./build_kernel_modules.sh $(CRAY_NETTYPE) $(CRAY_NODETYPE)

install: all
	mkdir -p $(PREFIX)/kmod/$(KVER)-cray_$(CRAY_NETTYPE)_$(CRAY_NODETYPE)
	tar xvf modules.tar -C $(PREFIX)/kmod/$(KVER)-cray_$(CRAY_NETTYPE)_$(CRAY_NODETYPE)
	chmod -R a+rX $(PREFIX)/kmod

clean:
	if [ -a modules.tar ]; then $(RM) modules.tar; fi
