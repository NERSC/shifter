DIRS := config src kmod ssh

include config.mk
-include config.inc

ifeq ($(WLM_INT),alps)
    DIRS += wlm_integration/alps
endif

config.inc:
	echo PREFIX=$(PREFIX) > config.inc; \
    echo DESTDIR=$(DESTDIR) >> config.inc

recurse:
	echo $(PREFIX)
#for dir in $(DIRS); do (cd $$dir; make all || exit 1) || exit 1; done
all: config.inc recurse

test:
	for dir in $(DIRS); do (cd $$dir; make test || exit 1) || exit 1; done

install:
	mkdir -p $(PREFIX)
	mkdir -p $(PREFIX)/sbin
	mkdir -p $(PREFIX)/bin
	mkdir -p $(PREFIX)/share/udiRoot
	PREFIX=$(PREFIX) ./setup_udiRoot_etc_files.sh
	$(INSTALL) --owner=root --group=root --mode=500  setup_udiRoot_etc_files.sh $(PREFIX)/share/udiRoot
	for dir in $(DIRS); do (cd $$dir; make install || exit 1) || exit 1; done

clean:
	if [[ -e config.inc ]]; then rm config.inc; fi; \
	for dir in $(DIRS); do (cd $$dir; make clean || exit 1) || exit 1; done
