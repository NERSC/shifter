AUTOMAKE_OPTIONS=foreign

AM_CPPFLAGS = -DCONFIG_FILE=\"${sysconfdir}/udiRoot.conf\"

sbin_PROGRAMS = setupRoot unsetupRoot imageGWConnect
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

IMAGEGWCONNECT_SOURCES = \
    ImageGWConnect.c \
    ImageData.c \
    utility.c \
    UdiRootConfig.c

shifter_SOURCES = $(SHIFTER_SOURCES)
setupRoot_SOURCES = $(SETUPROOT_SOURCES)
unsetupRoot_SOURCES = $(UNSETUPROOT_SOURCES)
imageGWConnect_SOURCES = $(IMAGEGWCONNECT_SOURCES)
imageGWConnect_LDFLAGS = $(MUNGE_LDFLAGS) $(JSON_LDFLAGS) $(LIBCURL) $(MUNGE_LDFLAGS)
imageGWConnect_CPPFLAGS = $(AM_CPPFLAGS) $(MUNGE_CPPFLAGS) $(JSON_CPPFLAGS) $(LIBCURL_CPPFLAGS) $(MUNGE_CPPFLAGS)

install-data-hook:
	/bin/chmod 4755 $(DESTDIR)/$(bindir)/shifter
