AUTOMAKE_OPTIONS=foreign

AM_CPPFLAGS = -DCONFIG_FILE=\"test_udiRoot.conf\" -DLIBEXECDIR=\"${libexecdir}/shifter\"
CPPUTEST_CFLAGS = -I../../../dep/cpputest/include
CPPUTEST_LDFLAGS = ../../../dep/cpputest/lib*/libCppUTest.a
TEST_CFLAGS = -fprofile-arcs -ftest-coverage -O0 -ggdb -I.. -I../../../src/ $(AM_CPPFLAGS) $(CPPUTEST_CFLAGS) -DNO_ROOT_OWN_CHECK=1 -DROOTFS_TYPE="\"$(ROOTFS_TYPE)\""
TEST_LDFLAGS = $(CPPUTEST_LDFLAGS)

noinst_DATA     = test_udiRoot.conf
check_PROGRAMS = test_shifterSpank_config test_shifterSpank_util test_shifterSpank_prolog
TESTS = test_shifterSpank_config test_shifterSpank_util test_shifterSpank_prolog

test_udiRoot.conf: test_udiRoot.conf.in
	cat test_udiRoot.conf.in | sed "s|@@@PREFIX@@@|./|g" | sed "s|@@@CONFIG_DIR@@@|./|g" | sed "s|@@@ROOTFSTYPE@@@|$(ROOTFS_TYPE)|g"  > test_udiRoot.conf
	chmod 644 test_udiRoot.conf


test_shifterSpank_config_SOURCES = \
    test_shifterSpank_config.cpp \
    ../shifterSpank.c \
    mockwrapper.c \
    ../../../src/UdiRootConfig.c \
    ../../../src/utility.c \
    ../../../src/VolumeMap.c \
    ../../../src/ImageData.c \
    ../../../src/shifter_core.c \
    ../../../src/MountList.c
test_shifterSpank_config_CXXFLAGS = $(TEST_CFLAGS)
test_shifterSpank_config_CFLAGS = $(TEST_CFLAGS)
test_shifterSpank_config_LDFLAGS = $(TEST_LDFLAGS)

test_shifterSpank_util_SOURCES = \
    test_shifterSpank_util.cpp \
    ../shifterSpank.c \
    mockwrapper.c \
    ../../../src/UdiRootConfig.c \
    ../../../src/utility.c \
    ../../../src/VolumeMap.c \
    ../../../src/ImageData.c \
    ../../../src/shifter_core.c \
    ../../../src/MountList.c
test_shifterSpank_util_CXXFLAGS = $(TEST_CFLAGS)
test_shifterSpank_util_CFLAGS = $(TEST_CFLAGS)
test_shifterSpank_util_LDFLAGS = $(TEST_LDFLAGS)

test_shifterSpank_prolog_SOURCES = \
    test_shifterSpank_prolog.cpp \
    ../shifterSpank.c \
    mockwrapper.c \
    ../../../src/UdiRootConfig.c \
    ../../../src/utility.c \
    ../../../src/VolumeMap.c \
    ../../../src/ImageData.c \
    ../../../src/shifter_core.c \
    ../../../src/MountList.c
test_shifterSpank_prolog_CXXFLAGS = $(TEST_CFLAGS)
test_shifterSpank_prolog_CFLAGS = $(TEST_CFLAGS)
test_shifterSpank_prolog_LDFLAGS = $(TEST_LDFLAGS)

.PHONY: clean-local-check

clean-local: clean-local-check
clean-local-check:
	-rm -rf *.gcda
	-rm -rf *.gcno
	-rm -f test_udiRoot.conf 
