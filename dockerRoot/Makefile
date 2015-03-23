DIRS := src kmod

PREFIX=/global/syscom/sc/nsg/opt/dockerRoot
CONFIG_DIR=/global/syscom/sc/nsg/etc
CONFIG_FILE=$(CONFIG_DIR)/dockerRoot.conf
CRAY_NETTYPE=gem
CRAY_NODETYPE=c


all: 
	for dir in $(DIRS); do (cd $$dir; make all || exit 1) || exit 1; done

test:
	for dir in $(DIRS); do (cd $$dir; make test || exit 1) || exit 1; done

install:
	mkdir -p $(PREFIX)
	mkdir -p $(PREFIX)/sbin
	mkdir -p $(PREFIX)/bin
	./setup_dockerRoot_etc_files.sh
	for dir in $(DIRS); do (cd $$dir; make install || exit 1) || exit 1; done

clean:
	for dir in $(DIRS); do (cd $$dir; make clean || exit 1) || exit 1; done
