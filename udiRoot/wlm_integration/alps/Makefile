########
## udiRoot: Makefile
##
## Author:  Douglas Jacobsen <dmj@nersc.gov>
## Date  :  2015/03/23
##
########

include ../../config.mk

all: udiRoot-epilogue udiRoot-prologue

udiRoot-epilogue: udiRoot-epilogue.in
	cat $< | sed 's|@@@SYSTEM@@@|$(SYSTEM)|g' | sed 's|@@@PREFIX@@@|$(PREFIX)|g' > $@

udiRoot-prologue: udiRoot-prologue.in
	cat $< | sed 's|@@@SYSTEM@@@|$(SYSTEM)|g' | sed 's|@@@PREFIX@@@|$(PREFIX)|g' > $@


install: all
	mkdir -p $(PREFIX)/libexec
	if ! $(CMP) -s udiRoot-prologue $(PREFIX)/libexec/udiRoot-prologue; then \
	    $(INSTALL) --owner=root --group=root --mode=555  udiRoot-prologue $(PREFIX)/libexec/udiRoot-prologue; \
	fi
	if ! $(CMP) -s udiRoot-epilogue $(PREFIX)/libexec/udiRoot-epilogue; then \
	    $(INSTALL) --owner=root --group=root --mode=555  udiRoot-epilogue $(PREFIX)/libexec/udiRoot-epilogue; \
	fi

clean:
	if [ -a udiRoot-prologue ]; then $(RM) udiRoot-prologue; fi
	if [ -a udiRoot-epilogue ]; then $(RM) udiRoot-epilogue; fi
