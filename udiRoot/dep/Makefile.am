AUTOMAKE_OPTIONS=foreign

sbin_PROGRAMS = mount

mount:
	./build.sh

.PHONY: clean-local-check 

install-data-hook:
	mkdir -p $(DESTDIR)$(exec_prefix)/opt
	tar xf udiRoot_dep.tar -C $(DESTDIR)$(exec_prefix)/opt --strip-components=2 -p --no-same-owner

clean-local: clean-local-check
clean-local-check:
	-rm -rf build
	-rm udiRoot_dep.tar
	-rm mount

