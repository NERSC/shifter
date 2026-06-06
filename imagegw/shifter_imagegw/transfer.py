#!/usr/bin/env python
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
Install, remove, and manipulate files on the systems

Will use local shell/copy commands to perform needed actions if the system has
filesystems locally available.
"""

import os
import shutil
import logging
from shifter_imagegw.fasthash import fast_hash


def remove_file(filename, system):
    """
    Remove the specified file from the system
    """
    basepath = system.imageDir
    image_fn = os.path.basename(filename)
    target_fn = os.path.join(basepath, image_fn)
    if os.path.exists(target_fn):
        os.unlink(target_fn)
    else:
        logging.warning(f"Removing unknown file: {target_fn}")


def check_file(filename, system, import_image=False):
    """
    check the validatity of a file on the system
    """
    basepath = system.imageDir
    image_fn = os.path.basename(filename)
    target_fn = os.path.join(basepath, image_fn)
    if import_image:
        return os.path.exists(filename)
    else:
        return os.path.exists(target_fn)


def hash_file(filename, system):
    """
    Calculate a hash of the image file.
    """
    return fast_hash(filename)


def transfer(system, image_path, metadata_path=None,
             import_image=False, dest_path=None):
    """
    transfer an image and its metadata to the system
    """

    for fn in [image_path, metadata_path]:
        if not fn:
            continue
        logging.debug(f'copy file {fn} to {dest_path}')
        dstfn = os.path.basename(fn)
        if import_image and fn == image_path:
            dstfn = dest_path
        dst = os.path.join(system.imageDir, dstfn)
        shutil.copyfile(fn, dst)


def remove(system, image_path, metadata_path=None):
    """
    remove an image and its metadata from the system
    """
    logging.info(f"remove: {system.imageDir} {image_path} {metadata_path}")
    if metadata_path:
        remove_file(metadata_path, system)
    remove_file(image_path, system)
    return True


def imagevalid(system, image_path, metadata_path=None):
    """
    check if image exists on the system
    """
    metadata_ok = True
    if metadata_path is not None:
        metadata_ok = check_file(metadata_path, system)
    image_ok = check_file(image_path, system)

    return metadata_ok and image_ok
