AUTOMAKE_OPTIONS=foreign

AM_CPPFLAGS = -DCONFIG_FILE=\"${sysconfdir}/udiRoot.conf\"
CPPUTEST_CFLAGS = -I../dep/cpputest/include
CPPUTEST_LDFLAGS = ../dep/cpputest/lib/libCppUTest.a
TEST_CFLAGS = -fprofile-arcs -ftest-coverage -O0 -ggdb $(AM_CPPFLAGS) $(CPPUTEST_CFLAGS) -DNO_ROOT_OWN_CHECK=1
TEST_LDFLAGS = $(CPPUTEST_LDFLAGS)

sbin_PROGRAMS = setupRoot unsetupRoot
bin_PROGRAMS = shifter shifterimg
noinst_PROGRAMS = test_utility test_VolumeMap test_UdiRootConfig test_MountList test_shifter_core test_ImageData test_shifter

SHIFTER_SOURCES = \
    shifter.c \
    UdiRootConfig.c \
    utility.c \
    config.h \
    shifter_core.c \
    ImageData.c \
    VolumeMap.c \
    MountList.c

SETUPROOT_SOURCES = \
    setupRoot.c \
    UdiRootConfig.c \
    utility.c \
    config.h \
    shifter_core.c \
    ImageData.c \
    VolumeMap.c \
    MountList.c

UNSETUPROOT_SOURCES = \
    unsetupRoot.c \
    UdiRootConfig.c \
    utility.c \
    config.h \
    shifter_core.c \
    ImageData.c \
    VolumeMap.c \
    MountList.c

SHIFTERIMG_SOURCES = \
    shifterimg.c \
    ImageData.c \
    utility.c \
    UdiRootConfig.c

test_utility_SOURCES = test_utility.cpp utility.c
test_utility_CXXFLAGS = $(TEST_CFLAGS)
test_utility_CFLAGS = $(TEST_CFLAGS)
test_utility_LDFLAGS = $(TEST_LDFLAGS)

test_VolumeMap_SOURCES = \
    test_VolumeMap.cpp \
    VolumeMap.c \
    utility.c \
    shifter_core.c \
    MountList.c
test_VolumeMap_CXXFLAGS = $(TEST_CFLAGS)
test_VolumeMap_CFLAGS = $(TEST_CFLAGS)
test_VolumeMap_LDFLAGS = $(TEST_LDFLAGS)

test_UdiRootConfig_SOURCES = \
    test_UdiRootConfig.cpp \
    UdiRootConfig.c \
    utility.c
test_UdiRootConfig_CXXFLAGS = $(TEST_CFLAGS)
test_UdiRootConfig_CFLAGS = $(TEST_CFLAGS)
test_UdiRootConfig_LDFLAGS = $(TEST_LDFLAGS)

test_shifter_SOURCES = \
    test_shifter.cpp \
    shifter.c \
    shifter_core.c \
    UdiRootConfig.c \
    utility.c \
    ImageData.c \
    VolumeMap.c \
    MountList.c
test_shifter_CXXFLAGS = $(TEST_CFLAGS) -D_TESTHARNESS_SHIFTER
test_shifter_CFLAGS = $(TEST_CFLAGS) -D_TESTHARNESS_SHIFTER
test_shifter_LDFLAGS = $(TEST_LDFLAGS)

test_ImageData_SOURCES = \
    test_ImageData.cpp \
    ImageData.c \
    utility.c
test_ImageData_CXXFLAGS = $(TEST_CFLAGS)
test_ImageData_CFLAGS = $(TEST_CFLAGS)
test_ImageData_LDFLAGS = $(TEST_LDFLAGS)

test_shifter_core_SOURCES = \
    test_shifter_core.cpp \
    shifter_core.c \
    utility.c \
    UdiRootConfig.c \
    VolumeMap.c \
    MountList.c \
    ImageData.c
test_shifter_core_CXXFLAGS = $(TEST_CFLAGS)
test_shifter_core_CFLAGS = $(TEST_CFLAGS)
test_shifter_core_LDFLAGS = $(TEST_LDFLAGS)

test_MountList_SOURCES = \
    test_MountList.cpp \
    MountList.c \
    utility.c
test_MountList_CXXFLAGS = $(TEST_CFLAGS)
test_MountList_CFLAGS = $(TEST_CFLAGS)
test_MountList_LDFLAGS = $(TEST_LDFLAGS)


shifter_SOURCES = $(SHIFTER_SOURCES)
setupRoot_SOURCES = $(SETUPROOT_SOURCES)
unsetupRoot_SOURCES = $(UNSETUPROOT_SOURCES)
shifterimg_SOURCES = $(SHIFTERIMG_SOURCES)
shifterimg_LDFLAGS = $(MUNGE_LDFLAGS) $(JSON_LDFLAGS) $(LIBCURL) $(MUNGE_LDFLAGS)
shifterimg_CPPFLAGS = $(AM_CPPFLAGS) $(MUNGE_CPPFLAGS) $(JSON_CPPFLAGS) $(LIBCURL_CPPFLAGS) $(MUNGE_CPPFLAGS)

install-data-hook:
	/bin/chmod 4755 $(DESTDIR)/$(bindir)/shifter
