#!/bin/bash

BUILDDIR=`pwd`
KVER=$( uname -r | sed 's|\(.*\)-cray.*|\1|g')
NETWORK=${1:-gem}
NODETYPE=${2:-c}
ARCH=${3:-x86}

KDIR=/usr/src/linux-$KVER
if [ ! -e "$KDIR" ]; then
    echo "Cannot find kernel sources: $KDIR" 1>&2
    exit 1
fi
CONFIG_FILE=$KDIR/arch/$ARCH/configs/cray_${NETWORK}_${NODETYPE}_defconfig
if [ ! -e "$CONFIG_FILE" ]; then
    echo "Cannot find a configuration file for the specified setup: $CONFIG_FILE" 1>&2
    exit 1
fi

BDIR=$( mktemp -d )
FSTYPE=$( df -TP $BDIR | tail -n +2 | awk '{print $2}' )

if [ "x$FSTYPE" == "xgpfs" -o "x$FSTYPE" == "xdvs" -o "x$FSTYPE" == "x" ]; then
    echo "Will not build kernel on fs type $FSTYPE" 1>&2
    rmdir $BDIR
    exit 1
fi

TGT_CONFIG_FILE=$BDIR/linux-$KVER/.config
cp -rp $KDIR $BDIR
cp $CONFIG_FILE $TGT_CONFIG_FILE
echo "CONFIG_BLK_DEV_LOOP=m" >> $TGT_CONFIG_FILE
echo "CONFIG_EXT4_FS=m" >> $TGT_CONFIG_FILE
echo "CONFIG_CRAMFS=m" >> $TGT_CONFIG_FILE
echo "CONFIG_SQUASHFS=m" >> $TGT_CONFIG_FILE
echo "CONFIG_JBD2=m" >> $TGT_CONFIG_FILE
echo "CONFIG_FS_MBCACHE=m" >> $TGT_CONFIG_FILE
echo "CONFIG_XFS_FS=m" >> $TGT_CONFIG_FILE
echo "CONFIG_XFS_QUOTA=y" >> $TGT_CONFIG_FILE
echo "CONFIG_XFS_DMAPI=m" >> $TGT_CONFIG_FILE
echo "CONFIG_XFS_POSIX_ACL=y" >> $TGT_CONFIG_FILE
echo "CONFIG_XFS_RT=y" >> $TGT_CONFIG_FILE

cd $BDIR/linux-$KVER
yes "" | make oldconfig ## reconfigure kernel accepting defaults
make modules

find . -name \*.ko -type f -print0 | tar -cf $BUILDDIR/modules.tar --null -T -
cd $BUILDDIR

rm -rf $BDIR/linux-$KVER
rmdir $BDIR
