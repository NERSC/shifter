#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.62])
AC_INIT([shifterUdiRoot], [2015.09], [shifter@nersc.gov])
AC_CONFIG_SRCDIR([src/shifter_core.h])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([auxdir])
AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

## get path for slurm
#AC_ARG_WITH([slurm], AS_HELP_STRING([--with-slurm], [Path to slurm]))
#AS_IF([test "x$with_slurm" != "xno"],

X_AC_MUNGE

X_AC_SLURM

X_AC_JSON

LIBCURL_CHECK_CONFIG

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h stddef.h stdint.h stdlib.h string.h sys/mount.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_UID_T
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_CHOWN
AC_FUNC_FORK
AC_FUNC_GETGROUPS
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([getcwd memmove memset mkdir putenv realpath rmdir setenv strchr strdup strstr strtoul uname])

AC_CONFIG_FILES([Makefile
                 src/Makefile])
#                 config/Makefile
#                 dep/Makefile
#                 kmod/Makefile
#                 src/Makefile
#                 src/test/Makefile
#                 wlm_integration/cray/Makefile
#                 wlm_integration/slurm/Makefile
#                 wlm_integration/torque/Makefile])
AC_OUTPUT
