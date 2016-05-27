#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.62])
AC_INIT([shifter], [16.04.0pre1], [shifter-hpc@googlegroups.com])
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
X_AC_MUNGE
X_AC_SLURM
X_AC_JSON

AC_ARG_VAR([DD_PATH], [Path to dd executable])
AC_PATH_PROG([DD_PATH], [dd], [NotFound])

AC_ARG_VAR([CP_PATH], [Path to cp executable])
AC_PATH_PROG([CP_PATH], [cp], [NotFound])

AC_ARG_VAR([MV_PATH], [Path to mv executable])
AC_PATH_PROG([MV_PATH], [mv], [NotFound])

AC_ARG_VAR([CHMOD_PATH], [Path to chmod executable])
AC_PATH_PROG([CHMOD_PATH], [chmod], [NotFound])

AC_ARG_VAR([MKFSXFS_PATH], [Path to mkfs.xfs executable])
AC_PATH_PROG([MKFSXFS_PATH], [mkfs.xfs], [NotFound], [$PATH$PATH_SEPARATOR/sbin$PATH_SEPARATOR/usr/sbin])

AC_ARG_VAR([INSMOD_PATH], [Path to insmod executable])
AC_PATH_PROG([INSMOD_PATH], [insmod], [NotFound], [$PATH$PATH_SEPARATOR/sbin$PATH_SEPARATOR/usr/sbin])

AC_ARG_VAR([MODPROBE_PATH], [Path to modprobe executable])
AC_PATH_PROG([MODPROBE_PATH], [modprobe], [NotFound], [$PATH$PATH_SEPARATOR/sbin$PATH_SEPARATOR/usr/sbin])

AM_PATH_PYTHON

AC_ARG_ENABLE([cray], AS_HELP_STRING([--enable-cray], [Turn on cray support and testing options]))
ROOTFS_TYPE=tmpfs
USE_GETPWUID=1
AS_IF([test "x$enable_cray" == "xyes"], [
        ROOTFS_TYPE=ramfs
        USE_GETPWUID=0
])
AC_SUBST(ROOTFS_TYPE)
AC_SUBST(USE_GETPWUID)

AC_ARG_ENABLE([staticsshd], AS_HELP_STRING([--disable-staticsshd], [Turn off support for starting a staticly-linked sshd to within the container]))
AM_CONDITIONAL([BUILD_STATICSSHD], [test "x$enable_staticsshd" != "xno"])
AM_CONDITIONAL([ALLOW_STATICSSHD], [test "x$enable_staticsshd" != "xno"])

AS_VAR_SET(shifter_etc_files, $sysconfdir/shifter_etc_files)
AC_SUBST(shifter_etc_files)

AC_DEFINE_DIR(SHIFTER_PREFIX, prefix, [fully expanded prefix])
AC_DEFINE_DIR(SHIFTER_LIBEXECDIR, libexecdir, [fully expanded libexecdir])
AC_DEFINE_DIR(SHIFTER_SYSCONFDIR, sysconfdir, [fully expanded sysconfdir])
AC_DEFINE_DIR(SHIFTER_ETC_FILESDIR, shifter_etc_files, [fully expanded shifter_etc_files])

AC_PROG_LIBTOOL

## get path for slurm
#AC_ARG_WITH([slurm], AS_HELP_STRING([--with-slurm], [Path to slurm]))
#AS_IF([test "x$with_slurm" != "xno"],


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
                 src/Makefile
                 src/config.h
                 etc_files/Makefile
                 src/test/Makefile
                 dep/Makefile
                 extra/Makefile
                 wlm_integration/slurm/Makefile
                 wlm_integration/slurm/test/Makefile
                 imagegw/Makefile
                 imagegw/shifter_imagegw/Makefile
                 imagegw/shifter_imagegw/api/Makefile
                 imagegw/shifter_imagegw/__init__.py
                 udiRoot.conf.example
               ])
#                 config/Makefile
#                 dep/Makefile
#                 kmod/Makefile
#                 src/Makefile
#                 src/test/Makefile
#                 wlm_integration/cray/Makefile
#                 wlm_integration/torque/Makefile])
AC_OUTPUT
