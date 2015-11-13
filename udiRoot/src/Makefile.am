AUTOMAKE_OPTIONS=foreign

AM_CPPFLAGS = -DCONFIG_FILE=\"${sysconfdir}/udiRoot.conf\"

sbin_PROGRAMS = setupRoot unsetupRoot
bin_PROGRAMS = shifter

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

shifter_SOURCES = $(SHIFTER_SOURCES)
setupRoot_SOURCES = $(SETUPROOT_SOURCES)
unsetupRoot_SOURCES = $(UNSETUPROOT_SOURCES)


