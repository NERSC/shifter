AUTOMAKE_OPTIONS=foreign

sbin_PROGRAMS = mount mkfs.xfs
noinst_PROGRAMS = cpputest

mount:
	./build.sh

cpputest:
	./cpputest.sh

.PHONY: clean-local-check mkfs.phony
mkfs.xfs: mkfs.phony

install-data-hook:
	mkdir -p $(DESTDIR)$(exec_prefix)/opt
	tar xf udiRoot_dep.tar -C $(DESTDIR)$(exec_prefix)/opt --strip-components=2 -p --no-same-owner

clean-local: clean-local-check
clean-local-check:
	-rm -rf build
	-rm -rf cpputest
	-rm udiRoot_dep.tar
	-rm mount

