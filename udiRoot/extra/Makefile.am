AUTOMAKE_OPTIONS=foreign

AM_CPPFLAGS = -DCONFIG_FILE=\"${sysconfdir}/udiRoot.conf\" -I../src -Wall

sbin_PROGRAMS = slurm_bb_support

SLURM_BB_SUPPORT_SOURCES = \
	slurm_bb_support.c \
	../src/UdiRootConfig.c \
	../src/utility.c \
	../src/VolumeMap.c \
	../src/MountList.c \
	../src/shifter_core.c


slurm_bb_support_SOURCES = $(SLURM_BB_SUPPORT_SOURCES)
