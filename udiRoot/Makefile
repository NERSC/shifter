DIRS := config src dep

include config.mk
-include config.inc

ifeq ($(CRAY_SUPPORT),1)
    DIRS += wlm_integration/cray
    DIRS += kmod
endif
ifeq ($(WLM_INT),torque)
    DIRS += wlm_integration/torque
endif
ifeq ($(WLM_INT),slurm)
    DIRS += wlm_integration/slurm
endif

config.inc:
	echo PREFIX=$(PREFIX) > config.inc; \
    echo DESTDIR=$(DESTDIR) >> config.inc

recurse:
	echo $(PREFIX); \
	for dir in $(DIRS); do (cd $$dir; make all || exit 1) || exit 1; done
all: config.inc recurse

test: cpputest
	for dir in $(DIRS); do (cd $$dir; make test || exit 1) || exit 1; done

cpputest:
	./cpputest.sh
install:
	mkdir -m 755 -p $(DESTDIR)
	mkdir -m 755 -p $(DESTDIR)/sbin
	mkdir -m 755 -p $(DESTDIR)/bin
	mkdir -p $(DESTDIR)/share/udiRoot
	PREFIX=$(DESTDIR) ./setup_udiRoot_etc_files.sh
	$(INSTALL) --owner=root --group=root --mode=500  setup_udiRoot_etc_files.sh $(DESTDIR)/share/udiRoot
	for dir in $(DIRS); do (cd $$dir; make install || exit 1) || exit 1; done

clean:
	if [[ -e config.inc ]]; then rm config.inc; fi; \
	for dir in $(DIRS); do (cd $$dir; make clean || exit 1) || exit 1; done
