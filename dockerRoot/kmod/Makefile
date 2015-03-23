########
## dockerRoot: Makefile
##
## Author:  Douglas Jacobsen <dmj@nersc.gov>
## Date  :  2015/03/23
##
########

PREFIX=/global/syscom/sc/nsg/opt/dockerRoot
CONFIG_FILE=/global/syscom/sc/nsg/etc/dockerRoot.conf
CRAY_NETTYPE=gem
CRAY_NODETYPE=c

CC      = /usr/bin/gcc
RM      = /bin/rm
CMP     = /usr/bin/cmp
INSTALL = /usr/bin/install

## configuration
CFLAGS=-O2 -Wall
LDFLAGS=

all: modules.tar

modules.tar:
	./build_kernel_modules.sh $(CRAY_NETTYPE) $(CRAY_NODETYPE)

install: all
	KVER := $(shell uname -r)
	mkdir -p $(PREFIX)/kmod/$(KVER)-cray_$(CRAY_NETTYPE)_$(CRAY_NODETYPE)
	tar xvf modules.tar -C $(PREFIX)/kmod/$(KVER)-cray_$(CRAY_NETTYPE)_$(CRAY_NODETYPE)
	chmod -R a+rX $(PREFIX)/kmod

clean:
	if [ -a modules.tar ]; then $(RM) modules.tar; fi
