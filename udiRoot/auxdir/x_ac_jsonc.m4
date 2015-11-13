##*****************************************************************************
## $Id$
##*****************************************************************************
#  AUTHOR:
#    Chris Dunlap <cdunlap@llnl.gov> (originally for OpenSSL)
#    Modified for json by Christopher Morrone <morrone2@llnl.gov>
#
#  SYNOPSIS:
#    X_AC_JSON()
#
#  DESCRIPTION:
#    Check the usual suspects for an json installation,
#    updating CPPFLAGS and LDFLAGS as necessary.
#
#  WARNINGS:
#    This macro must be placed after AC_PROG_CC and before AC_PROG_LIBTOOL.
##*****************************************************************************

AC_DEFUN([X_AC_JSON], [

  _x_ac_json_dirs="/usr /usr/local /opt/json"
  _x_ac_json_libs="lib64 lib"

  AC_ARG_WITH(
    [json-c],
    AS_HELP_STRING(--with-json-c=PATH,Specify path to json-c installation),
    [_x_ac_json_dirs="$withval $_x_ac_json_dirs"])

  AC_CACHE_CHECK(
    [for json installation],
    [x_ac_cv_json_dir],
    [
      for d in $_x_ac_json_dirs; do
        test -d "$d" || continue
        test -d "$d/include" || continue
        test -d "$d/include/json-c" || continue
        test -f "$d/include/json-c/json.h" || continue
	for bit in $_x_ac_json_libs; do
          test -d "$d/$bit" || continue

 	  _x_ac_json_libs_save="$LIBS"
          LIBS="-L$d/$bit -ljson-c $LIBS"
          AC_LINK_IFELSE(
            [AC_LANG_CALL([], json_encode)],
            AS_VAR_SET(x_ac_cv_json_dir, $d))
          LIBS="$_x_ac_json_libs_save"
          test -n "$x_ac_cv_json_dir" && break
	done
        test -n "$x_ac_cv_json_dir" && break
      done
    ])

  if test -z "$x_ac_cv_json_dir"; then
    AC_MSG_WARN([unable to locate json installation])
  else
    JSON_LIBS="-ljson-c"
    JSON_CPPFLAGS="-I$x_ac_cv_json_dir/include"
    JSON_DIR="$x_ac_cv_json_dir"
    if test "$ac_with_rpath" = "yes"; then
      JSON_LDFLAGS="-Wl,-rpath -Wl,$x_ac_cv_json_dir/$bit -L$x_ac_cv_json_dir/$bit"
    else
      JSON_LDFLAGS="-L$x_ac_cv_json_dir/$bit"
    fi
  fi

  AC_SUBST(JSON_LIBS)
  AC_SUBST(JSON_CPPFLAGS)
  AC_SUBST(JSON_LDFLAGS)
  AC_SUBST(JSON_DIR)

  AM_CONDITIONAL(WITH_JSON, test -n "$x_ac_cv_json_dir")
])
