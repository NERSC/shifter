AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I auxdir
SUBDIRS    = dep src extra imagegw etc_files
DISTCHECK_CONFIGURE_FLAGS = --disable-staticsshd

if WITH_SLURM
    SUBDIRS += wlm_integration/slurm
endif WITH_SLURM

sysconf_DATA=udiRoot.conf.example
EXTRA_DIST = AUTHORS Dockerfile LICENSE NEWS README.md autogen.sh config.h shifter.spec
