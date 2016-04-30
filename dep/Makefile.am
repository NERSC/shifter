AUTOMAKE_OPTIONS=foreign

if BUILD_STATICSSHD
OPT = udiRoot_dep.tar
else
OPT =
endif

pkglibexec_SCRIPTS = mount
check_PROGRAMS = cpputest/include/CppUTest/CppUTestConfig.h
noinst_DATA = $(OPT)

mount:
	+./build_mount.sh > build_mount.log

cpputest/include:
	+./cpputest.sh > build_cpputest.log

udiRoot_dep.tar:
	+./build_ssh.sh > build_ssh.log

.PHONY: clean-local-check

install-data-hook:
	mkdir -p $(DESTDIR)/$(libexecdir)/shifter/opt
	tar xf udiRoot_dep.tar -C $(DESTDIR)$(libexecdir)/shifter/opt --strip-components=2 -p --no-same-owner

clean-local: clean-local-check
clean-local-check:
	-rm -rf build
	-rm -rf build_mount
	-rm -rf cpputest
	-rm udiRoot_dep.tar
	-rm mount

