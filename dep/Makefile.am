AUTOMAKE_OPTIONS=foreign

pkglibexec_SCRIPTS = mount
noinst_PROGRAMS = cpputest/include

mount:
	+./build.sh > build_dep.log

cpputest/include:
	+./cpputest.sh > build_cpputest.log

.PHONY: clean-local-check

install-data-hook:
	mkdir -p $(DESTDIR)/$(libexecdir)/shifter/opt
	tar xf udiRoot_dep.tar -C $(DESTDIR)$(libexecdir)/shifter/opt --strip-components=2 -p --no-same-owner

clean-local: clean-local-check
clean-local-check:
	-rm -rf build
	-rm -rf cpputest
	-rm udiRoot_dep.tar
	-rm mount

