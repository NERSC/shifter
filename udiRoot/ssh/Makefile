########
## udiRoot: Makefile
##
## Author:  Douglas Jacobsen <dmj@nersc.gov>
## Date  :  2015/03/23
##
########

include ../config.mk

## configuration
CFLAGS=-O2 -Wall
LDFLAGS=

all: udiRoot_sshd.tar

udiRoot_sshd.tar:
	./build.sh

install: all
	mkdir -p $(PREFIX)/sshd
	tar xf udiRoot_sshd.tar -C $(PREFIX)/sshd --strip-components=2

clean:
	if [ -a udiRoot_sshd.tar ]; then $(RM) udiRoot_sshd.tar; fi
