########
## qsetenv: Makefile
##
## Author:  Douglas Jacobsen <dmj@nersc.gov>
## Date  :  2014/09/10
##
########
include ../../config.mk
-include ../../config.inc

## configuration
CFLAGS=-O2 -Wall
TORQUE_DIR=$(shell qsub --about 2>&1 | awk '{ i=0; while (i++ < NF) { if ($$i == "InstallDir:") { print $$(i+1) } } }' )
TORQUE_VERSION=$(shell qsub --about 2>&1 | awk '/Version:/{ print $$2}')
CFLAGS+= -DTORQUE_VERSION="\"$(TORQUE_VERSION)\""
CPPFLAGS=-I$(TORQUE_DIR)/include
LDFLAGS=-L$(TORQUE_DIR)/lib -Wl,-rpath,$(TORQUE_DIR)/lib

all: qsetenv qgetenv qstatenv qsetenv.8

qsetenv: qsetenv.c
	$(CC) $(LDFLAGS) $(CPPFLAGS) $(CFLAGS) $< -ltorque -o $@

qstatenv: qstatenv.c
	$(CC) $(LDFLAGS) $(CPPFLAGS) $(CFLAGS) $< -ltorque -o $@

qgetenv: qgetenv.c
	$(CC) $(LDFLAGS) $(CPPFLAGS) $(CFLAGS)  $< -ltorque -o $@

qsetenv.8: qsetenv.man8
	groff $(@:.8=.man8) > $@

install: all
	mkdir -p -m 755 $(DESTDIR)/libexec
	mkdir -p -m 755 $(DESTDIR)/share/man/man8
	$(INSTALL) --owner=root --group=root --mode=555 qstatenv $(DESTDIR)/libexec/qstatenv
	$(INSTALL) --owner=root --group=root --mode=555 qsetenv $(DESTDIR)/libexec/qsetenv
	$(INSTALL) --owner=root --group=root --mode=555 qgetenv $(DESTDIR)/libexec/qgetenv
	$(INSTALL) --owner=root --group=root --mode=444 qsetenv.8 $(DESTDIR)/share/man/man8/qsetenv.8


clean: qsetenv qgetenv qstatenv qsetenv.8
	for file in $^; do if [[ -e "$$file" ]]; then rm "$$file"; fi; done
