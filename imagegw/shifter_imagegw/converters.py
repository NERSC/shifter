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
import tempfile
from shifter_imagegw.util import program_exists, rmtree


def generate_ext4_image(expand_path, image_path, options):
    """
    Creates an ext4 based image
    """
    message = f"ext4 support is not supported {expand_path} {image_path}"
    raise NotImplementedError(message)


def generate_cramfs_image(expand_path, image_path, options):
    """
    Creates a CramFS based image
    """
    program_exists('mkfs.cramfs')
    cmd = ["mkfs.cramfs", expand_path, image_path]
    if options is not None:
        cmd.extend(options)
    ret = subprocess.call(cmd)
    if ret != 0:
        # error handling
        pass
    try:
        rmtree(expand_path)
    except Exception:
        # error handling
        pass

    return True


def generate_squashfs_image(expand_path, image_path, options):
    """
    Creates a SquashFS based image
    """
    # This will raise an exception if mksquashfs tool is not found
    # it should be handled by the calling function
    program_exists('mksquashfs')

    cmd = ["mksquashfs", expand_path, image_path, "-all-root"]

    if options is not None:
        cmd.extend(options)
    else:
        cmd.append('-no-xattrs')
    ret = subprocess.call(cmd)
    if ret != 0:
        # error handling
        pass
    try:
        rmtree(expand_path)
    except Exception:
        pass

    return True


def convert(fmt, expand_path, image_path, options=None):
    """ do the conversion """
    if os.path.exists(image_path):
        return True

    (dirname, fname) = os.path.split(image_path)
    (temp_fd, temp_path) = tempfile.mkstemp('.partial', fname, dirname)
    os.close(temp_fd)
    os.unlink(temp_path)
    opts = None
    if options is not None and fmt in options:
        if isinstance(options[fmt], str):
            opts = [options[fmt]]
        elif isinstance(options[fmt], list):
            opts = options[fmt]
        else:
            raise ValueError("options for format should be a string or list")

    try:
        success = False
        if fmt == 'squashfs':
            success = generate_squashfs_image(expand_path, temp_path, opts)
        elif fmt == 'cramfs':
            success = generate_cramfs_image(expand_path, temp_path, opts)
        elif fmt == 'ext4':
            success = generate_ext4_image(expand_path, temp_path, opts)
        elif fmt == 'mock':
            with open(temp_path, 'w') as f:
                line = 'bogus'
                if options is not None:
                    line += ' '.join(opts)
                f.write(line)
            success = True
        else:
            raise NotImplementedError(f"{fmt} not a supported format")
    except Exception:
        if os.path.exists(temp_path):
            os.unlink(temp_path)
        raise

    if not success:
        return False
    try:
        os.rename(temp_path, image_path)
    except Exception:
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
        meta_fd.write(f"FORMAT: {fmt}\n")
        if meta.get('entrypoint'):
            meta_fd.write(f"ENTRY: {meta['entrypoint']}\n")
        if meta.get('cmd'):
            meta_fd.write(f"CMD: {meta['cmd']}\n")
        if meta.get('workdir'):
            meta_fd.write(f"WORKDIR: {meta['workdir']}\n")
        if private and 'userACL' in meta and meta['userACL'] is not None:
            acls = ','.join(map(lambda x: str(x), meta['userACL']))
            meta_fd.write(f"USERACL: {acls}\n")
        if private and 'groupACL' in meta and meta['groupACL'] is not None:
            acls = ','.join(map(lambda x: str(x), meta['groupACL']))
            meta_fd.write(f"GROUPACL: {acls}\n")
        if 'env' in meta and meta['env'] is not None:
            for keyval in meta['env']:
                meta_fd.write(f"ENV: {keyval}\n")
        if 'user' in meta:
            meta_fd.write("USER: {meta['user']}\n")
        meta_fd.close()
    # Some error must have occurred
    return True
