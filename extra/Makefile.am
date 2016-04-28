AUTOMAKE_OPTIONS=foreign

AM_CPPFLAGS = -DCONFIG_FILE=\"${sysconfdir}/udiRoot.conf\" -DLIBEXECDIR=\"${libexecdir}/shifter\" -I../src -Wall

pkglibexec_PROGRAMS = shifter_slurm_dws_support

SHIFTER_SLURM_DWS_SUPPORT_SOURCES = \
	shifter_slurm_dws_support.c \
	../src/UdiRootConfig.c \
	../src/utility.c \
	../src/VolumeMap.c \
	../src/MountList.c \
	../src/shifter_core.c


shifter_slurm_dws_support_SOURCES = $(SHIFTER_SLURM_DWS_SUPPORT_SOURCES)
