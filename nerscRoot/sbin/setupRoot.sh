#!/bin/sh

##################
## Setup NERSC chroot for docker,chos images
##  Validates user selected chroot image
##  Bind mounts image to $nerscRootPath
##  Bind mounts needed filesystems into the image
##
## Authors:  Douglas Jacobsen, Shane Canon
## 2015/02/27
##################

export PATH="/usr/bin:/bin"
export LD_LIBRARY_PATH=""
export LD_PRELOAD=""

NERSC_ROOT_TYPE=$1
NERSC_ROOT_VALUE=$2

nerscMount=/var/nerscMount
dockerVFSPath=/scratch/docker/vfs/dir
chosPath=/scratch/chos
nerscRootPath=/global/syscom/sc/nsg/opt/nerscRoot
mapPath=$nerscRootPath/fsMap.conf
etcDir=$nerscRootPath/etc
kmodDir=$nerscRootPath/kmod/$( uname -r )
validTargets="chosVFS dockerVFS chosExt4Image dockerExt4Image"

targetType=""
target=""

globalFs="u1 u2 project syscom common" 
lustreFs="scratch"


die () {
    echo $1
    exit 1
}

setupNerscVFSRoot () {
    local fs
    local dir
    local item
    imageDir=$1

    ## create tmpfs/rootfs for our /
    ## this is to ensure there are writeable areas for manipulating the image
    mount -t rootfs none $nerscMount
    cd $nerscMount

    ## setup NERSC GPFS filesystems
    mkdir global
    for fs in $globalFs; do 
        mkdir global/$fs
        mount -o bind /global/$fs global/$fs
        mount -o remount,nodev,nosuid $nerscMount/global/$fs
    done
    cd global
    ln -s u1 homes
    cd $nerscMount

    ## setup lustre mounts
    for fs in $lustreFs; do
        mkdir $fs
        mount -o bind /$fs $fs
        #mount -o remount,nodev,nosuid $nerscMount/$fs ### XXX lustre is weird about remounts
    done

    ## make some aspects of the local environment available
    mkdir -p local/etc
    mount -o bind /dsl/etc local/etc
    mount -o remount,nodev,nosuid $nerscMount/local/etc
    mkdir -p .shared
    mount -o bind /dsl/.shared .shared

    ## reserve some directories in "/" that will be handled explicitly
    mkdir -p etc/nerscImage
    mkdir -p etc/site
    mkdir -p proc
    mkdir -p sys
    mkdir -p dev
    mkdir -p tmp
    
    # mount the image into the new mount
    for dir in `ls $imageDir`; do
        if [ -e $dir ]; then
            continue
        fi
        if [ -L $imageDir/$dir ]; then
            cp $imageDir/$dir $dir
            continue
        fi
        if [ -d $imageDir/$dir ]; then
            mkdir $dir
            mount --bind $imageDir/$dir $nerscMount/$dir
            mount -o remount,nodev,nosuid $nerscMount/$dir
        fi
    done

    ## merge image etc, site customizations, and local customizations into /etc
    if [ -e $imageDir/etc ]; then
        mount -o bind $imageDir/etc etc/nerscImage
        mount -o remount,nodev,nosuid $nerscMount/etc/nerscImage
        cd etc
        for item in `ls nerscImage`; do
            ln -s nerscImage/$item .
        done
        for item in `ls $etcDir`; do
            cp -p $etcDir/$item $nerscMount/etc/site
            if [ -e $item ]; then
                rm $item
            fi
            ln -s site/$item $item
        done
        ## take care of passwd
        if [ -e passwd ]; then
            rm passwd
        fi
        ln -s site/nersc_passwd passwd
        if [ -e group ]; then
            rm group
        fi
        ln -s site/nersc_group group
    fi

    ## mount up linux needs
    mount -t proc none $nerscMount/proc
    mount -o bind /dev $nerscMount/dev
    mount -o bind /sys $nerscMount/sys

}

setupLoopbackMount() {
    local imageFile
    local kmodPath
    imageFile=$1
    kmodPath=$2

    /sbin/insmod $kmodPath/drivers/block/loop.ko
    /sbin/insmod $kmodPath/fs/mbcache.ko
    /sbin/insmod $kmodPath/fs/jbd2/jbd2.ko
    /sbin/insmod $kmodPath/fs/ext4/ext4.ko

    loopMount=/var/loopNerscMount
    mkdir -p $loopMount
    mount -t ext4 -o loop,ro $imageFile $loopMount || die "Failed to mount image file $imageFile"
    setupNerscVFSRoot $loopMount
}

containsItem () {
    local tgt
    tgt=$1
    shift
    [[ $# -eq 0 ]] && return 1
    while true; do
        [[ $1 == $tgt ]] && return 0
        [[ $# -eq 0 ]] && break
        shift
    done
    return 1
}



if [ "x$CHOS" != "x" ]; then
    targetType="chos"
    target=$CHOS
fi

if [ "x$DOCKER" != "x" ]; then
    targetType="docker"
    target=$DOCKER
fi

if [ "x$NERSC_ROOT_TYPE" == "xCHOS" ]; then
    targetType="chos"
fi
if [ "x$NERSC_ROOT_TYPE" == "xDOCKER" ]; then
    targetType="docker"
fi
if [ "x$NERSC_ROOT_VALUE" != "x" ]; then
    target=$NERSC_ROOT_VALUE
fi

containsItem $targetType "chos" "docker" || die "Invalid image target type: $targetType"

## since $target is user provided, need to sanitize
target=${target//[^a-zA-Z0-9_:\.]/}

linePrefix="$targetType;$target;"
line=$( egrep "^$linePrefix" $mapPath ) || die "Cannot find $targetType image \"$target\". Failed."
image=$( echo $line | awk -F ';' '{print $3}' ) || die "Cannot identify path for $targetType image \"$target\". Failed."
imageType=$( echo $line | awk -F ';' '{print $4}' ) || die "Cannot identify imageType for $targetType image \"$target\". Failed."

containsItem $imageType "vfs" "ext4Image" || die "Invalid imageType for $targetType image \"$target\". Failed."

## get base path for this target type
pathVar="${targetType}Path"
eval basePath=\$$pathVar
[[ "x$basePath" != "x" ]] || die "Invalid base path for $targetType"
[[ "x$basePath" != "x/" ]] || die "Base path cannot be /"

## get final path to image
fullPath="${basePath}/${image}"
[[ -e $fullPath ]] || die "Path to $targetType image \"$target\" does not exist"

## start doing bind mounts
mkdir -p $nerscMount
if [ "x$imageType" == "xvfs" ]; then
    setupNerscVFSRoot $fullPath
elif [ "x$imageType" == "xext4Image" ]; then
    setupLoopbackMount $fullPath $kmodDir
else
    exit 1
fi
