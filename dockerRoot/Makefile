DIRS := config src kmod

include config.mk

ifeq ($(WLM_INT),alps)
    DIRS += wlm_integration/alps
endif


all: 
	for dir in $(DIRS); do (cd $$dir; make all || exit 1) || exit 1; done

test:
	for dir in $(DIRS); do (cd $$dir; make test || exit 1) || exit 1; done

install:
	mkdir -p $(PREFIX)
	mkdir -p $(PREFIX)/sbin
	mkdir -p $(PREFIX)/bin
	mkdir -p $(PREFIX)/share/dockerRoot
	./setup_dockerRoot_etc_files.sh
	$(INSTALL) --owner=root --group=root --mode=500  setup_dockerRoot_etc_files.sh $(PREFIX)/share/dockerRoot
	for dir in $(DIRS); do (cd $$dir; make install || exit 1) || exit 1; done

clean:
	for dir in $(DIRS); do (cd $$dir; make clean || exit 1) || exit 1; done
