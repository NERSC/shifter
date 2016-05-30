#!/bin/bash
#
# Wrapper for tar to fix permissions on a directory prior to
# extracting.  Otherwise the untar will fail as non-root
if [ -e /bin/tar ] ; then
  TAR=/bin/tar
elif [ -e /usr/bin/tar ] ; then
  TAR=/usr/bin/tar
else
  echo "tar not found" 1>&2
  exit 1
fi

# Just catch the cases where imagegw is likely running it
if [ "$1" == 'xf' ] && [ "$3" == "-C" ] ; then
  file=$2
  d=$4
  # Scan the tar file looking for non-writeable directories
  for f in $(tar tvf $file|grep ^d|sed 's/[^:]*:.. //') ; do
    if [ -d "$d/$f" ] ; then
      chmod +w "$d/$f"
      #echo $d/$f
    fi
  done
fi
exec tar "$@"
