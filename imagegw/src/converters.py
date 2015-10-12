#!/usr/bin/python

import os
import subprocess
import shutil


def generateExt4Image(expandedPath, imagePath):
    """
    Creates an ext4 based image
    """

    ## create sparsefile for the image
    # TODO: Fix
    #ret = subprocess.call(["dd", "of=%s" % imagePath, "bs=1", "count=0", "seek=%d" % (get_size(layersPath) * 2)], stdout=fdnull, stderr=fdnull)
    if ret != 0:
        # error handling
        pass
    ret = subprocess.call(["mke2fs", "-F", imagePath], stdout=fdnull, stderr=fdnull)
    if ret != 0:
        # error handling
        pass

    ## prepare a mount point and mount the image file
    mntPoint = tempfile.mkdtemp()
    ret = subprocess.call(["mount", "-o", "loop", imagePath, mntPoint], stdout=fdnull, stderr=fdnull)
    if ret != 0:
        return None
    # TODO: copy contents

    loopDevice = None
    fd = open("/proc/mounts", "r")
    for line in fd:
        (device, t_mntPoint, stuff) = line.split(' ', 2)
        if t_mntPoint == mntPoint and re.match('/dev/loop\d+', device) is not None:
            loopDevice = device
            break
    fd.close()
    ret = subprocess.call(["umount", mntPoint], stdout=fdnull, stderr=fdnull)
    if loopDevice is not None:
        subprocess.call(['losetup', '-d', loopDevice])
    return True

def generateCramFSImage(expandedPath, imagePath):
    """
    Creates a CramFS based image
    """
    ret = subprocess.call(["mkfs.cramfs", expandedPath, imagePath], stdout=fdnull, stderr=fdnull)
    if ret != 0:
        # error handling
        pass
    try:
        shutil.rmtree(expandedPath)
    except:
        # error handling
        pass

    return True

def generateSquashFSImage(expandedPath, imagePath):
    """
    Creates a SquashFS based image
    """

    ret = subprocess.call(["mksquashfs", expandedPath, imagePath, "-all-root"])
    if ret != 0:
        # error handling
        pass
    try:
        shutil.rmtree(expandedPath)
    except:
        # error handling
        pass

    return True

def convert(format,expandedPath,imagePath):
    if os.path.exists(imagePath):
        print "file already exist"
        return True
        
    imageTempPath=imagePath+'.partial'

    success=False
    if format=='squashfs':
        success=generateSquashFSImage(expandedPath,imageTempPath)
    elif format=='cramfs':
        success=generateCramFSImage(expandedPath,imageTempPath)
    elif format=='ext4':
        success=generateExt4Image(expandedPath,imageTempPath)
    else:
        raise NotImplementedError("%s not a supported format"%format)
    if not success:
        return False
    try:
        os.rename(imageTempPath, imagePath)
    except:
        return False

    # Some error must have occurred
    return True
