#!/usr/bin/python

# Shifter, Copyright (c) 2015, The Regents of the University of California,
# through Lawrence Berkeley National Laboratory (subject to receipt of any
# required approvals from the U.S. Dept. of Energy).  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#  1. Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright notice,
#     this list of conditions and the following disclaimer in the documentation
#     and/or other materials provided with the distribution.
#  3. Neither the name of the University of California, Lawrence Berkeley
#     National Laboratory, U.S. Dept. of Energy nor the names of its
#     contributors may be used to endorse or promote products derived from this
#     software without specific prior written permission.`
#
# See LICENSE for full text.

"""
Convert module that handles converting an unpacked image into a compatibable
format for shifter.
"""

import os
import subprocess
import shutil
import tempfile
from shifter_imagegw.util import program_exists


def generate_ext4_image(expand_path, image_path):
    """
    Creates an ext4 based image
    """
    message = 'ext4 support is note implemented yet %s %s' % \
              (expand_path, image_path)
    raise NotImplementedError(message)


def generate_cramfs_image(expand_path, image_path):
    """
    Creates a CramFS based image
    """
    program_exists('mkfs.cramfs')
    cmd = ["mkfs.cramfs", expand_path, image_path]
    ret = subprocess.call(cmd)
    if ret != 0:
        # error handling
        pass
    try:
        shutil.rmtree(expand_path)
    except:
        # error handling
        pass

    return True


def generate_squashfs_image(expand_path, image_path):
    """
    Creates a SquashFS based image
    """
    # This will raise an exception if mksquashfs tool is not found
    # it should be handled by the calling function
    program_exists('mksquashfs')

    args = ["mksquashfs", expand_path, image_path, "-all-root"]
    if 'DISABLE_NOXATTRS' not in os.environ:
        args.append('-no-xattrs')
    ret = subprocess.call(args)
    if ret != 0:
        # error handling
        pass
    try:
        shutil.rmtree(expand_path)
    except:
        pass

    return True


def convert(fmt, expand_path, image_path):
    """ do the conversion """
    if os.path.exists(image_path):
        return True

    (dirname, fname) = os.path.split(image_path)
    (temp_fd, temp_path) = tempfile.mkstemp('.partial', fname, dirname)
    os.close(temp_fd)
    os.unlink(temp_path)

    try:
        success = False
        if fmt == 'squashfs':
            success = generate_squashfs_image(expand_path, temp_path)
        elif fmt == 'cramfs':
            success = generate_cramfs_image(expand_path, temp_path)
        elif fmt == 'ext4':
            success = generate_ext4_image(expand_path, temp_path)
        elif fmt == 'mock':
            with open(temp_path, 'w') as f:
                f.write('bogus')
            success = True
        else:
            raise NotImplementedError("%s not a supported format" % fmt)
    except:
        if os.path.exists(temp_path):
            os.unlink(temp_path)
        raise

    if not success:
        return False
    try:
        os.rename(temp_path, image_path)
    except:
        return False

    # Some error must have occurred
    return True


def writemeta(fmt, meta, metafile):
    """ write the metadata file """
    with open(metafile, 'w') as meta_fd:
        # write out ENV, ENTRYPOINT, WORKDIR and format
        private = False
        if 'private' in meta:
            private = meta['private']
        # Disable saving private image info for backwards support
        # This can be deprecated in the future.
        if 'DISABLE_ACL_METADATA' in os.environ:
            private = False
        meta_fd.write("FORMAT: %s\n" % (fmt))
        if 'entrypoint' in meta and meta['entrypoint'] is not None:
            meta_fd.write("ENTRY: %s\n" % (meta['entrypoint']))
        if 'workdir' in meta and meta['workdir'] is not None:
            meta_fd.write("WORKDIR: %s\n" % (meta['workdir']))
        if private and 'userACL' in meta and meta['userACL'] is not None:
            acls = ','.join(map(lambda x: str(x), meta['userACL']))
            meta_fd.write("USERACL: %s\n" % (acls))
        if private and 'groupACL' in meta and meta['groupACL'] is not None:
            acls = ','.join(map(lambda x: str(x), meta['groupACL']))
            meta_fd.write("GROUPACL: %s\n" % (acls))
        if 'env' in meta and meta['env'] is not None:
            for keyval in meta['env']:
                meta_fd.write("ENV: %s\n" % (keyval))
        if 'user' in meta:
            meta_fd.write("USER: %s\n" % meta['user'])
        meta_fd.close()
    # Some error must have occurred
    return True
