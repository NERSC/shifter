AUTOMAKE_OPTIONS=foreign

SUBDIRS = test

AM_CPPFLAGS = -DCONFIG_FILE=\"${sysconfdir}/udiRoot.conf\" -DLIBEXECDIR=\"${libexecdir}/shifter\" -Wall

if ALLOW_STATICSSHD
SSH_CPPFLAGS=-DALLOW_STATICSSHD=1
else
SSH_CPPFLAGS=
endif

sbin_PROGRAMS = setupRoot unsetupRoot
bin_PROGRAMS = shifter shifterimg

SHIFTER_SOURCES = \
    shifter.c \
    UdiRootConfig.h \
    UdiRootConfig.c \
    utility.h \
    utility.c \
    shifter_core.h \
    shifter_core.c \
    ImageData.h \
    ImageData.c \
    VolumeMap.h \
    VolumeMap.c \
    PathList.h \
    PathList.c \
    MountList.h \
    MountList.c

SETUPROOT_SOURCES = \
    setupRoot.c \
    UdiRootConfig.h \
    UdiRootConfig.c \
    utility.h \
    utility.c \
    shifter_core.h \
    shifter_core.c \
    ImageData.h \
    ImageData.c \
    VolumeMap.h \
    VolumeMap.c \
    PathList.h \
    PathList.c \
    MountList.h \
    MountList.c

UNSETUPROOT_SOURCES = \
    unsetupRoot.c \
    UdiRootConfig.h \
    UdiRootConfig.c \
    utility.h \
    utility.c \
    shifter_core.h \
    shifter_core.c \
    ImageData.h \
    ImageData.c \
    VolumeMap.h \
    VolumeMap.c \
    PathList.h \
    PathList.c \
    MountList.h \
    MountList.c

SHIFTERIMG_SOURCES = \
    shifterimg.c \
    shifterimg.h \
    ImageData.h \
    ImageData.c \
    utility.h \
    utility.c \
    UdiRootConfig.h \
    UdiRootConfig.c \
    VolumeMap.h \
    VolumeMap.c \
    shifter_core.c \
    MountList.c \
    PathList.c


shifter_SOURCES = $(SHIFTER_SOURCES)
setupRoot_SOURCES = $(SETUPROOT_SOURCES)
setupRoot_CPPFLAGS = $(AM_CPPFLAGS) $(SSH_CPPFLAGS)
unsetupRoot_SOURCES = $(UNSETUPROOT_SOURCES)
shifterimg_SOURCES = $(SHIFTERIMG_SOURCES)
shifterimg_LDFLAGS = $(MUNGE_LDFLAGS) $(JSON_LDFLAGS) $(LIBCURL) $(MUNGE_LDFLAGS)
shifterimg_CPPFLAGS = $(AM_CPPFLAGS) $(MUNGE_CPPFLAGS) $(JSON_CPPFLAGS) $(LIBCURL_CPPFLAGS) $(MUNGE_CPPFLAGS)

install-exec-hook:
	/bin/chmod 4755 $(DESTDIR)$(bindir)/shifter
