AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I auxdir
SUBDIRS    = dep src

if WITH_SLURM
    SUBDIRS += wlm_integration/slurm
endif WITH_SLURM
