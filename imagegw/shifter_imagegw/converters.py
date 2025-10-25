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
import logging
from shifter_imagegw.util import program_exists, rmtree


def _generate_squashfs_image(expand_path, image_path, options):
    """
    Creates a SquashFS based image
    """
    # This will raise an exception if mksquashfs tool is not found
    # it should be handled by the calling function
    program_exists('mksquashfs')

    cmd = ["mksquashfs", expand_path, image_path, "-all-root"]

    if options:
        cmd.extend(options)
    else:
        cmd.append('-no-xattrs')
    logging.debug(' '.join(cmd))
    ret = subprocess.call(cmd)
    if ret != 0:
        # error handling
        raise OSError("mksquashfs failed")
    try:
        rmtree(expand_path)
    except Exception:
        pass


def convert(fmt, expand_path, image_path, options=None):
    """ do the conversion """
    if fmt != 'squashfs':
        raise NotImplementedError(f"Format {fmt} is not a supported format")

    if os.path.exists(image_path):
        raise FileExistsError

    (dirname, fname) = os.path.split(image_path)
    (temp_fd, temp_path) = tempfile.mkstemp('.partial', fname, dirname)
    os.close(temp_fd)
    os.unlink(temp_path)
    opts = None
    if options:
        if isinstance(options, str):
            opts = [options]
        elif isinstance(options, list):
            opts = options
        else:
            raise ValueError("options should be a string or list")

    try:
        _generate_squashfs_image(expand_path, temp_path, opts)
    except Exception as e:
        if os.path.exists(temp_path):
            os.unlink(temp_path)
        raise e

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
        private = meta.get('private', False)
        meta_fd.write(f"FORMAT: {fmt}\n")
        if meta.get('entrypoint'):
            meta_fd.write(f"ENTRY: {meta['entrypoint']}\n")
        for item in ['cmd', 'workdir', 'user']:
            if item in meta:
                meta_fd.write(f"{item.upper()}: {meta[item]}\n")
        if private:
            for item in ['userACL', 'groupACL']:
                if item in meta and meta[item]:
                    acls = ','.join(map(lambda x: str(x), meta[item]))
                    meta_fd.write(f"{item.upper()}: {acls}\n")

        if 'env' in meta and meta['env']:
            for keyval in meta['env']:
                meta_fd.write(f"ENV: {keyval}\n")
        meta_fd.close()
    # Some error must have occurred
    return True
