#!/usr/bin/python

import os
import subprocess
import shutil
from shifter_imagegw.util import program_exists

"""
Shifter, Copyright (c) 2015, The Regents of the University of California,
through Lawrence Berkeley National Laboratory (subject to receipt of any
required approvals from the U.S. Dept. of Energy).  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
 3. Neither the name of the University of California, Lawrence Berkeley
    National Laboratory, U.S. Dept. of Energy nor the names of its
    contributors may be used to endorse or promote products derived from this
    software without specific prior written permission.`

See LICENSE for full text.
"""

def generateExt4Image(expandedPath, imagePath):
    """
    Creates an ext4 based image
    """
    raise NotImplementedError('ext4 support is note implemented yet')

    ## create sparsefile for the image
    # TODO: compute size
    size=100*1024*1024*1024
    ret = subprocess.call(["dd", "of=%s" % imagePath, "bs=1", "count=0", "seek=%d" % (size)], stdout=fdnull, stderr=fdnull)
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
    program_exists ('mkfs.cramfs')
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
    # This will raise an exception if mksquashfs tool is not found
    # it should be handled by the calling function
    program_exists ('mksquashfs')

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

def writemeta(format,meta,metafile):

    with open(metafile, 'w') as mf:
        # write out ENV, ENTRYPOINT, WORKDIR and format
        mf.write("FORMAT: %s\n"%(format))
        if 'entrypoint' in meta and meta['entrypoint'] is not None:
            mf.write("ENTRY: %s\n"%(meta['entrypoint']))
        if 'workdir' in meta and meta['workdir'] is not None:
            mf.write("WORKDIR: %s\n"%(meta['workdir']))
        if 'env' in meta and meta['env'] is not None:
            for kv in meta['env']:
                mf.write("ENV: %s\n"%(kv))
        mf.close()
    # Some error must have occurred
    return True

