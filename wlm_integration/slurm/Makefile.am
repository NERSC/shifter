AUTOMAKE_OPTIONS=foreign

AM_CPPFLAGS = -DCONFIG_FILE=\"${sysconfdir}/udiRoot.conf\" -DLIBEXECDIR=\"${libexecdir}/shifter\" $(SLURM_CPPFLAGS) -I../../src/ -DIS_NATIVE_SLURM=$(SLURM_NATIVE_SLURM)

pkglib_LTLIBRARIES = shifter_slurm.la

PLUGIN_FLAGS = -module -avoid-version --export-dynamic

SHIFTER_SO_SOURCES = \
    shifterSpank.c \
    wrapper.c \
    ../../src/UdiRootConfig.c \
    ../../src/utility.c \
    ../../src/shifter_core.c \
    ../../src/ImageData.c \
    ../../src/VolumeMap.c

shifter_slurm_la_SOURCES = $(SHIFTER_SO_SOURCES)
shifter_slurm_la_LDFLAGS = $(SO_LDFLAGS) $(PLUGIN_FLAGS) $(SLURM_LDFLAGS)
