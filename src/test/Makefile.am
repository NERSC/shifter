AUTOMAKE_OPTIONS=foreign

AM_CPPFLAGS = -DCONFIG_FILE=\"test_udiRoot.conf\" -DLIBEXECDIR=\"${libexecdir}/shifter\"
CPPUTEST_CFLAGS = -I$(top_builddir)/dep/cpputest/include
CPPUTEST_LDFLAGS = $(top_builddir)/dep/cpputest/lib*/libCppUTest.a
TEST_CFLAGS = -fprofile-arcs -ftest-coverage -O0 -ggdb -I$(top_srcdir)/src $(AM_CPPFLAGS) $(CPPUTEST_CFLAGS) -DNO_ROOT_OWN_CHECK=1 -DROOTFS_TYPE="\"$(ROOTFS_TYPE)\""
TEST_LDFLAGS = $(CPPUTEST_LDFLAGS)

dist_noinst_DATA = test_udiRoot.conf.in etc chroot1 chroot2 chroot3 etc_small data_config1.conf data_config2.conf data_config3.conf data_config4.conf setup_test_chroot.sh
noinst_DATA = test_udiRoot.conf chroot1/nss chroot2/nss chroot3/nss
check_PROGRAMS = test_utility test_VolumeMap test_UdiRootConfig test_MountList test_shifter_core test_shifter_core_AsRoot test_shifter_core_AsRootDangerous test_ImageData test_shifter
TESTS = test_utility test_VolumeMap test_UdiRootConfig test_MountList test_shifter_core test_ImageData test_shifter

test_udiRoot.conf: test_udiRoot.conf.in
	cat $(srcdir)/test_udiRoot.conf.in | sed "s|@@@PREFIX@@@|./|g" | sed "s|@@@CONFIG_DIR@@@|./|g" | sed "s|@@@ROOTFSTYPE@@@|$(ROOTFS_TYPE)|g"  > test_udiRoot.conf
	chmod 644 test_udiRoot.conf

chroot1/nss:
	./setup_test_chroot.sh chroot1
	touch chroot1/nss

chroot2/nss:
	./setup_test_chroot.sh chroot2
	touch chroot2/nss

chroot3/nss:
	./setup_test_chroot.sh chroot3
	touch chroot3/nss


test_utility_SOURCES = \
    test_utility.cpp \
    $(top_srcdir)/src/utility.c
test_utility_CXXFLAGS = $(TEST_CFLAGS)
test_utility_CFLAGS = $(TEST_CFLAGS)
test_utility_LDFLAGS = $(TEST_LDFLAGS)

test_VolumeMap_SOURCES = \
    test_VolumeMap.cpp \
    $(top_srcdir)/src/VolumeMap.c \
    $(top_srcdir)/src/utility.c
test_VolumeMap_CXXFLAGS = $(TEST_CFLAGS)
test_VolumeMap_CFLAGS = $(TEST_CFLAGS)
test_VolumeMap_LDFLAGS = $(TEST_LDFLAGS)

test_UdiRootConfig_SOURCES = \
    test_UdiRootConfig.cpp \
    $(top_srcdir)/src/UdiRootConfig.c \
    $(top_srcdir)/src/utility.c \
	$(top_srcdir)/src/VolumeMap.c
test_UdiRootConfig_CXXFLAGS = $(TEST_CFLAGS)
test_UdiRootConfig_CFLAGS = $(TEST_CFLAGS)
test_UdiRootConfig_LDFLAGS = $(TEST_LDFLAGS)

test_shifter_SOURCES = \
    test_shifter.cpp \
    $(top_srcdir)/src/shifter.c \
    $(top_srcdir)/src/shifter_core.c \
    $(top_srcdir)/src/UdiRootConfig.c \
    $(top_srcdir)/src/utility.c \
    $(top_srcdir)/src/ImageData.c \
    $(top_srcdir)/src/VolumeMap.c \
    $(top_srcdir)/src/MountList.c
test_shifter_CXXFLAGS = $(TEST_CFLAGS) -D_TESTHARNESS_SHIFTER
test_shifter_CFLAGS = $(TEST_CFLAGS) -D_TESTHARNESS_SHIFTER
test_shifter_LDFLAGS = $(TEST_LDFLAGS)

test_ImageData_SOURCES = \
    test_ImageData.cpp \
    $(top_srcdir)/src/ImageData.c \
    $(top_srcdir)/src/utility.c
test_ImageData_CXXFLAGS = $(TEST_CFLAGS)
test_ImageData_CFLAGS = $(TEST_CFLAGS)
test_ImageData_LDFLAGS = $(TEST_LDFLAGS)

test_shifter_core_SOURCES = \
    test_shifter_core.cpp \
    $(top_srcdir)/src/shifter_core.c \
    $(top_srcdir)/src/utility.c \
    $(top_srcdir)/src/UdiRootConfig.c \
    $(top_srcdir)/src/VolumeMap.c \
    $(top_srcdir)/src/MountList.c \
    $(top_srcdir)/src/ImageData.c
test_shifter_core_CXXFLAGS = $(TEST_CFLAGS) -DNOTROOT
test_shifter_core_CFLAGS = $(TEST_CFLAGS)
test_shifter_core_LDFLAGS = $(TEST_LDFLAGS)

test_shifter_core_AsRoot_SOURCES = $(test_shifter_core_SOURCES)
test_shifter_core_AsRoot_CXXFLAGS = $(TEST_CFLAGS)
test_shifter_core_AsRoot_CFLAGS = $(TEST_CFLAGS)
test_shifter_core_AsRoot_LDFLAGS = $(TEST_LDFLAGS)

test_shifter_core_AsRootDangerous_SOURCES = $(test_shifter_core_SOURCES)
test_shifter_core_AsRootDangerous_CXXFLAGS = $(TEST_CFLAGS) -DDANGEROUSTESTS=1
test_shifter_core_AsRootDangerous_CFLAGS = $(TEST_CFLAGS)
test_shifter_core_AsRootDangerous_LDFLAGS = $(TEST_LDFLAGS)

test_MountList_SOURCES = \
    test_MountList.cpp \
    $(top_srcdir)/src/MountList.c \
    $(top_srcdir)/src/utility.c
test_MountList_CXXFLAGS = $(TEST_CFLAGS)
test_MountList_CFLAGS = $(TEST_CFLAGS)
test_MountList_LDFLAGS = $(TEST_LDFLAGS)

.PHONY: clean-local-check

clean-local: clean-local-check
clean-local-check:
	-rm -rf *.gcda
	-rm -rf *.gcno
	-rm -f test_udiRoot.conf 
