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

all: udiRoot_dep.tar

udiRoot_dep.tar:
	./build.sh

install: all
	mkdir -p $(DESTDIR)/deps
	tar xf udiRoot_dep.tar -C $(DESTDIR)/deps --strip-components=2
	chmod -R u+rX,go-rwx $(DESTDIR)/deps

clean:
	if [ -a udiRoot_dep.tar ]; then $(RM) udiRoot_dep.tar; fi
	if [ -d build ]; then $(RM) -r build; fi
