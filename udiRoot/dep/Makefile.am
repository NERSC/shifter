AUTOMAKE_OPTIONS=foreign

sbin_PROGRAMS = mount

mount:
	./build.sh

.PHONY: clean-local-check 

install-data-hook:
	tar xf udiRoot_dep.tar -C $(DESTDIR)$(exec_prefix)

clean-local: clean-local-check
clean-local-check:
	-rm -rf build
	-rm udiRoot_dep.tar
	-rm mount

