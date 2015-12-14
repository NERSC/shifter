AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I auxdir
SUBDIRS    = src dep

if WITH_SLURM
    SUBDIRS += wlm_integration/slurm
endif WITH_SLURM
