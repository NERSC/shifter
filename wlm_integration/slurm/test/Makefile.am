AUTOMAKE_OPTIONS=foreign

AM_CPPFLAGS = -DCONFIG_FILE=\"test_udiRoot.conf\" -DLIBEXECDIR=\"${libexecdir}/shifter\"
CPPUTEST_CFLAGS = -I$(top_builddir)/dep/cpputest/include
CPPUTEST_LDFLAGS = $(top_builddir)/dep/cpputest/lib*/libCppUTest.a
TEST_CFLAGS = -fprofile-arcs -ftest-coverage -O0 -ggdb \
    -I$(top_srcdir)/wlm_integration/slurm -I$(top_srcdir)/src \
    $(AM_CPPFLAGS) $(CPPUTEST_CFLAGS) -DNO_ROOT_OWN_CHECK=1 \
    -DROOTFS_TYPE="\"$(ROOTFS_TYPE)\""
TEST_LDFLAGS = $(CPPUTEST_LDFLAGS)

dist_noinst_DATA = test_udiRoot.conf.in
noinst_DATA     = test_udiRoot.conf
check_PROGRAMS = test_shifterSpank_config test_shifterSpank_util test_shifterSpank_prolog test_shifterSpank_cgroup
TESTS = test_shifterSpank_config test_shifterSpank_util test_shifterSpank_prolog test_shifterSpank_cgroup

test_udiRoot.conf: test_udiRoot.conf.in
	cat $(srcdir)/test_udiRoot.conf.in | sed "s|@@@PREFIX@@@|./|g" | sed "s|@@@CONFIG_DIR@@@|./|g" | sed "s|@@@ROOTFSTYPE@@@|$(ROOTFS_TYPE)|g"  > test_udiRoot.conf
	chmod 644 test_udiRoot.conf


test_shifterSpank_config_SOURCES = \
    test_shifterSpank_config.cpp \
    $(top_srcdir)/wlm_integration/slurm/shifterSpank.c \
    mockwrapper.c \
    $(top_srcdir)/src/UdiRootConfig.c \
    $(top_srcdir)/src/utility.c \
    $(top_srcdir)/src/VolumeMap.c \
    $(top_srcdir)/src/ImageData.c \
    $(top_srcdir)/src/shifter_core.c \
    $(top_srcdir)/src/MountList.c
test_shifterSpank_config_CXXFLAGS = $(TEST_CFLAGS)
test_shifterSpank_config_CFLAGS = $(TEST_CFLAGS)
test_shifterSpank_config_LDFLAGS = $(TEST_LDFLAGS)

test_shifterSpank_util_SOURCES = \
    test_shifterSpank_util.cpp \
    $(top_srcdir)/wlm_integration/slurm/shifterSpank.c \
    mockwrapper.c \
    $(top_srcdir)/src/UdiRootConfig.c \
    $(top_srcdir)/src/utility.c \
    $(top_srcdir)/src/VolumeMap.c \
    $(top_srcdir)/src/ImageData.c \
    $(top_srcdir)/src/shifter_core.c \
    $(top_srcdir)/src/MountList.c
test_shifterSpank_util_CXXFLAGS = $(TEST_CFLAGS)
test_shifterSpank_util_CFLAGS = $(TEST_CFLAGS)
test_shifterSpank_util_LDFLAGS = $(TEST_LDFLAGS)

test_shifterSpank_prolog_SOURCES = \
    test_shifterSpank_prolog.cpp \
    $(top_srcdir)/wlm_integration/slurm/shifterSpank.c \
    mockwrapper.c \
    $(top_srcdir)/src/UdiRootConfig.c \
    $(top_srcdir)/src/utility.c \
    $(top_srcdir)/src/VolumeMap.c \
    $(top_srcdir)/src/ImageData.c \
    $(top_srcdir)/src/shifter_core.c \
    $(top_srcdir)/src/MountList.c
test_shifterSpank_prolog_CXXFLAGS = $(TEST_CFLAGS)
test_shifterSpank_prolog_CFLAGS = $(TEST_CFLAGS)
test_shifterSpank_prolog_LDFLAGS = $(TEST_LDFLAGS)

test_shifterSpank_cgroup_SOURCES = \
    test_shifterSpank_cgroup.cpp \
    $(top_srcdir)/wlm_integration/slurm/shifterSpank.c \
    mockwrapper.c \
    $(top_srcdir)/src/UdiRootConfig.c \
    $(top_srcdir)/src/utility.c \
    $(top_srcdir)/src/VolumeMap.c \
    $(top_srcdir)/src/ImageData.c \
    $(top_srcdir)/src/shifter_core.c \
    $(top_srcdir)/src/MountList.c
test_shifterSpank_cgroup_CXXFLAGS = $(TEST_CFLAGS)
test_shifterSpank_cgroup_CFLAGS = $(TEST_CFLAGS)
test_shifterSpank_cgroup_LDFLAGS = $(TEST_LDFLAGS)

.PHONY: clean-local-check

clean-local: clean-local-check
clean-local-check:
	-rm -rf *.gcda
	-rm -rf *.gcno
	-rm -f test_udiRoot.conf 
