#!/bin/sh

export PATH="/usr/bin:/bin"
export LD_LIBRARY_PATH=""
export LD_PRELOAD=""

CONFIG_FILE=/global/syscom/sc/nsg/etc/nerscRoot.conf
if [ -e $CONFIG_FILE ]; then
    . $CONFIG_FILE
fi

if [ -z $nerscMount ]; then
    nerscMount=/var/nerscMount
fi
if [ -z $loopMount ]; then
    loopMount=/var/loopNerscMount
fi
ret=0

for i in `cat /proc/mounts | grep "$nerscMount" | sort -k2 -r | awk '{print $2}'`; do
    umount "$i" || ret=1
done

grep "$loopMount" /proc/mounts >/dev/null 2>&1 || ret=1

## must be a loop mount to clean up
umount "$loopMount" || ret=1

## remove any kernel modules loaded (in reverse order)
if [ -n $kmodCache -a -e "$kmodCache" ]; then
    for kmod in `cat "$kmodCache" | awk '{ L[n++] = $0 } END{ while(n--) { print L[n] } }'`; do
        /sbin/rmmod "$kmod"
    done
    rm "$kmodCache"
fi
exit $ret
