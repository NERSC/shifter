AUTOMAKE_OPTIONS=foreign

AM_CPPFLAGS = -DCONFIG_FILE=\"${sysconfdir}/udiRoot.conf\" -DLIBEXECDIR=\"${libexecdir}/shifter\" -I$(top_srcdir)/src -Wall

pkglibexec_PROGRAMS = shifter_slurm_dws_support

SHIFTER_SLURM_DWS_SUPPORT_SOURCES = \
	shifter_slurm_dws_support.c \
	$(top_srcdir)/src/UdiRootConfig.c \
	$(top_srcdir)/src/utility.c \
	$(top_srcdir)/src/VolumeMap.c \
	$(top_srcdir)/src/MountList.c \
	$(top_srcdir)/src/shifter_core.c


shifter_slurm_dws_support_SOURCES = $(SHIFTER_SLURM_DWS_SUPPORT_SOURCES)

EXTRA_DIST = cle6 systemd
