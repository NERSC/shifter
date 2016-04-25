AUTOMAKE_OPTIONS=foreign

pkglibexec_SCRIPTS = mount
noinst_PROGRAMS = cpputest/include

mount:
	./build.sh

cpputest/include:
	./cpputest.sh

.PHONY: clean-local-check

install-data-hook:
	mkdir -p $(DESTDIR)/$(datarootdir)/shifterudiroot/opt
	tar xf udiRoot_dep.tar -C $(DESTDIR)$(datarootdir)/shifterudiroot/opt --strip-components=2 -p --no-same-owner

clean-local: clean-local-check
clean-local-check:
	-rm -rf build
	-rm -rf cpputest
	-rm udiRoot_dep.tar
	-rm mount

