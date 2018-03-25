AUTOMAKE_OPTIONS=foreign

SUBDIRS=test

AM_CPPFLAGS = -DCONFIG_FILE=\"${sysconfdir}/udiRoot.conf\" -DLIBEXECDIR=\"${libexecdir}/shifter\" $(SLURM_CPPFLAGS) -I$(top_srcdir)/wlm_integration/slurm -I$(top_srcdir)/src -DIS_NATIVE_SLURM=$(SLURM_NATIVE_SLURM) -Wall

pkglib_LTLIBRARIES = shifter_slurm.la

PLUGIN_FLAGS = -module -avoid-version --export-dynamic

SHIFTER_SO_SOURCES = \
    shifterSpank.c \
    shifterSpank.h \
    wrapper.c \
    wrapper.h \
    $(top_srcdir)/src/UdiRootConfig.c \
    $(top_srcdir)/src/utility.c \
    $(top_srcdir)/src/shifter_core.c \
    $(top_srcdir)/src/shifter_mem.c \
    $(top_srcdir)/src/PathList.c \
    $(top_srcdir)/src/ImageData.c \
    $(top_srcdir)/src/VolumeMap.c

shifter_slurm_la_SOURCES = $(SHIFTER_SO_SOURCES)
shifter_slurm_la_LDFLAGS = $(SO_LDFLAGS) $(PLUGIN_FLAGS)
