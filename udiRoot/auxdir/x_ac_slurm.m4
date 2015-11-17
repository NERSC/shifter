##*****************************************************************************
## $Id$
##*****************************************************************************
#  AUTHOR:
#    Douglas Jacobsen <dmjacobsen@lbl.gov>
#       Totally ripped off from x_ac_munge.m4, by:
#    Chris Dunlap <cdunlap@llnl.gov> (originally for OpenSSL)
#    Modified for munge by Christopher Morrone <morrone2@llnl.gov>
#
#  SYNOPSIS:
#    X_AC_SLURM()
#
#  DESCRIPTION:
#    Check the usual suspects for a slurm installation,
#    updating CPPFLAGS and LDFLAGS as necessary.
#
#  WARNINGS:
#    This macro must be placed after AC_PROG_CC and before AC_PROG_LIBTOOL.
##*****************************************************************************

AC_DEFUN([X_AC_SLURM], [

  _x_ac_slurm_dirs="/usr /usr/local /opt/slurm /opt/slurm/default"
  _x_ac_slurm_libs="lib64 lib"

  AC_ARG_WITH(
    [slurm],
    AS_HELP_STRING(--with-slurm=PATH,Specify path to slurm installation),
    [_x_ac_munge_dirs="$withval $_x_ac_slurm"])

  AC_CACHE_CHECK(
    [for slurm installation],
    [x_ac_cv_slurm_dir],
    [
      for d in $_x_ac_slurm_dirs; do
        test -d "$d" || continue
        test -d "$d/include" || continue
        test -f "$d/include/slurm.h" || continue
	for bit in $_x_ac_slurm_libs; do
          test -d "$d/$bit" || continue

 	  _x_ac_slurm_libs_save="$LIBS"
          LIBS="-L$d/$bit -lslurm $LIBS"
          AC_LINK_IFELSE(
            [AC_LANG_CALL([], slurm_ping)],
            AS_VAR_SET(x_ac_cv_slurm_dir, $d))
          LIBS="$_x_ac_slurm_libs_save"
          test -n "$x_ac_cv_slurm_dir" && break
	done
        test -n "$x_ac_cv_slurm_dir" && break
      done
    ])

  if test -z "$x_ac_cv_slurm_dir"; then
    AC_MSG_WARN([unable to locate slurm installation])
  else
    SLURM_LIBS="-lslurm"
    SLURM_CPPFLAGS="-I$x_ac_cv_slurm_dir/include"
    SLURM_DIR="$x_ac_cv_slurm_dir"
    if test "$ac_with_rpath" = "yes"; then
      SLURM_LDFLAGS="-Wl,-rpath -Wl,$x_ac_cv_slurm_dir/$bit -L$x_ac_cv_slurm_dir/$bit"
    else
      SLURM_LDFLAGS="-L$x_ac_cv_slurm_dir/$bit"
    fi
  fi

  AC_SUBST(SLURM_LIBS)
  AC_SUBST(SLURM_CPPFLAGS)
  AC_SUBST(SLURM_LDFLAGS)
  AC_SUBST(SLURM_DIR)

  AM_CONDITIONAL(WITH_SLURM, test -n "$x_ac_cv_slurm_dir")
])
