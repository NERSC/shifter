#!/bin/sh

export PATH="/usr/bin:/bin"
export LD_LIBRARY_PATH=""
export LD_PRELOAD=""

nerscMount=/var/nerscMount
loopMount=/var/loopNerscMount
ret=0

for i in `cat /proc/mounts | grep $nerscMount | sort -k2 -r | awk '{print $2}'`; do
    umount $i || ret=1
done

grep $loopMount /proc/mounts >/dev/null 2>&1 || exit $ret 

## must be a loop mount to clean up
umount $loopMount || ret=1
/sbin/rmmod ext4
/sbin/rmmod loop
/sbin/rmmod jbd2
/sbin/rmmod mbcache
exit $ret
