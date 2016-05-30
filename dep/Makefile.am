AUTOMAKE_OPTIONS=foreign

if BUILD_STATICSSHD
OPT = udiRoot_dep.tar
else
OPT =
endif

pkglibexec_SCRIPTS = mount
dist_noinst_SCRIPTS = build_mount.sh build_ssh.sh cpputest.sh
check_DATA = cpputest/include/CppUTest/CppUTestConfig.h
noinst_DATA = $(OPT)

mount:
	+$(srcdir)/build_mount.sh > build_mount.log

cpputest/include/CppUTest/CppUTestConfig.h:
	+$(srcdir)/cpputest.sh > build_cpputest.log

udiRoot_dep.tar:
	+$(srcdir)/build_ssh.sh > build_ssh.log

.PHONY: clean-local-check

install-data-hook:
	mkdir -p $(DESTDIR)/$(libexecdir)/shifter/opt
	if [[ -e udiRoot_dep.tar ]]; then tar xf udiRoot_dep.tar -C $(DESTDIR)$(libexecdir)/shifter/opt --strip-components=2 -p --no-same-owner; fi

clean-local: clean-local-check
clean-local-check:
	-rm -rf build build_ssh.log
	-rm -rf build_mount build_mount.log
	-rm -rf cpputest build_cpputest.log
	-rm udiRoot_dep.tar
	-rm mount

