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
from subprocess import Popen, PIPE


def _sh_cmd(system, *args):
    """
    Helper function to build a local shell command
    """
    if len(args) == 0:
        return None
    return args


def _cp_cmd(system, localfile, targetfile):
    """
    Helper function to build a local copy command
    """
    return ['cp', localfile, targetfile]


def _exec_and_log(cmd, logger):
    """
    Execute a command and log the results to logger
    """
    if logger is not None:
        logger.info(f"about to exec: {' '.join(cmd)}")
    proc = Popen(cmd, stdout=PIPE, stderr=PIPE)
    if proc is None:
        if logger is not None:
            logger.error(f"Could not execute {' '.join(cmd)}")
        return
    bstdout, bstderr = proc.communicate()
    stdout = bstdout.decode("utf-8")
    stderr = bstderr.decode("utf-8")
    if logger is not None:
        if stdout is not None and len(stdout) > 0:
            logger.debug(f"{cmd[0]} stdout: {stdout.strip()}")
        if stderr is not None and len(stderr) > 0:
            logger.error(f"{cmd[0]} stderr: {stderr.strip()}")
    return proc.returncode


def _get_stdout_and_log(cmd, logger=None):
    """
    Execute a command and return stdout and stderr. Log the results to logger
    """
    if logger is not None:
        logger.info(f"about to exec: {' '.join(cmd)}")
    rerror = ''
    proc = Popen(cmd, stdout=PIPE, stderr=PIPE)
    if proc is None:
        if logger is not None:
            rerror = f"Could not execute {' '.join(cmd)}"
            logger.error(rerror)
        return rerror, ''
    bstdout, bstderr = proc.communicate()
    stdout = bstdout.decode("utf-8")
    stderr = bstderr.decode("utf-8")
    if logger is not None:
        if stdout is not None and len(stdout) > 0:
            logger.debug(f"{cmd[0]} stdout: {stdout.strip()}")
        if stderr is not None and len(stderr) > 0:
            logger.error(f"{cmd[0]} stdout: {stderr.strip()}")
            # push this error back to calling function so
            # it can be reported sensibly
            rerror = "%s %s" % (cmd[0], stderr.strip())
    return rerror, stdout


def pre_create_tempfile(basepath, filename, sh_cmd, system, logger=None):
    """
    Generate a tempfile for filename on the system
    """

    # TODO: Add command to setup the file with the right striping
    partial_fn = '%s.partial.XXXXXX' % filename
    temp_fn = os.path.join(basepath, partial_fn)

    cmd = sh_cmd(system, 'mktemp', temp_fn)
    if logger is not None:
        logger.info(f"about to exec: {' '.join(cmd)}")
    proc = Popen(cmd, stdout=PIPE, stderr=PIPE)
    temp_fn = None
    if proc is not None:
        bstdout, bstderr = proc.communicate()
        stdout = bstdout.decode("utf-8")
        stderr = bstderr.decode("utf-8")
        if proc.returncode == 0:
            temp_fn = stdout.strip()
        else:
            memo = 'Failed to precreate transfer file, ' \
                   f'{stderr} {str(proc.returncode)}'
            raise OSError(memo)
        if len(stderr) > 0 and logger is not None:
            logger.error(str(stderr.strip()))
    chmod_cmd = sh_cmd(system, 'chmod', '0600', temp_fn)
    ret = _exec_and_log(chmod_cmd, logger)
    if ret != 0:
        raise OSError('Failed to chmod precreated xfer file')
    return temp_fn


def copy_file(filename, system, logger=None):
    """
    Copy a file to the specified system
    """
    sh_cmd = None
    cp_cmd = None
    basepath = None
    sh_cmd = _sh_cmd
    cp_cmd = _cp_cmd
    basepath = system.imageDir

    image_fn = os.path.split(filename)[1]
    target_fn = os.path.join(basepath, image_fn)

    # pre-create the file with a temporary name
    temp_fn = pre_create_tempfile(basepath, image_fn, sh_cmd, system, logger)

    if temp_fn is None:
        raise OSError('Got no valid response back from tempfile precreation')

    if not temp_fn.startswith(basepath):
        memo = 'Got unexpected response back from tempfile precreation: ' \
               f'{temp_fn}'
        raise OSError(memo)

    copyret = None
    mvret = None
    try:
        copy = cp_cmd(system, filename, temp_fn)
        copyret = _exec_and_log(copy, logger)
    except Exception:
        rm_cmd = sh_cmd(system, 'rm', temp_fn)
        _exec_and_log(rm_cmd, logger)
        raise

    if copyret == 0:
        try:
            chmod_cmd = sh_cmd(system, 'chmod', '0600', temp_fn)
            ret = _exec_and_log(chmod_cmd, logger)
            if ret != 0:
                raise OSError('failed chmod command')

            mv_cmd = sh_cmd(system, 'mv', temp_fn, target_fn)
            mvret = _exec_and_log(mv_cmd, logger)
            if mvret != 0:
                raise OSError('failed mv command')
        except Exception:
            # TODO we might also need to remove target_fn in this case
            rm_cmd = sh_cmd(system, 'rm', temp_fn)
            _exec_and_log(rm_cmd, logger)
            raise

    if mvret == 0:
        try:
            chmod_cmd = sh_cmd(system, 'chmod', '0600', target_fn)
            ret = _exec_and_log(chmod_cmd, logger)
            if ret != 0:
                raise OSError('failed chmod command')
            return ret == 0
        except Exception:
            rm_cmd = sh_cmd(system, "rm", target_fn)
            _exec_and_log(rm_cmd, logger)
            raise

    return False


def import_copy_file(filename, destfilename, system, logger=None):
    """
    Copy a file to the specified system
    """
    sh_cmd = None
    cp_cmd = None
    basepath = None
    sh_cmd = _sh_cmd
    cp_cmd = _cp_cmd
    basepath = system.imageDir

    image_fn = os.path.split(destfilename)[1]
    target_fn = os.path.join(basepath, image_fn)

    # pre-create the file with a temporary name
    temp_fn = pre_create_tempfile(basepath, image_fn, sh_cmd, system, logger)

    if temp_fn is None:
        raise OSError('Got no valid response back from tempfile precreation')

    if not temp_fn.startswith(basepath):
        memo = 'Got unexpected response back from tempfile precreation: ' \
               f'{temp_fn}'
        raise OSError(memo)

    copyret = None
    try:
        copy = cp_cmd(system, filename, temp_fn)
        copyret = _exec_and_log(copy, logger)
    except Exception:
        rm_cmd = sh_cmd(system, 'rm', temp_fn)
        _exec_and_log(rm_cmd, logger)
        raise

    if copyret == 0:
        try:
            mv_cmd = sh_cmd(system, 'mv', temp_fn, target_fn)
            ret = _exec_and_log(mv_cmd, logger)
            return ret == 0
        except Exception:
            # TODO we might also need to remove target_fn in this case
            rm_cmd = sh_cmd(system, 'rm', temp_fn)
            _exec_and_log(rm_cmd, logger)
            raise
    return False


def remove_file(filename, system, logger=None):
    """
    Remove the specified file from the system
    """
    sh_cmd = None
    basepath = None
    sh_cmd = _sh_cmd
    basepath = system.imageDir
    image_fn = os.path.split(filename)[1]
    target_fn = os.path.join(basepath, image_fn)
    rm_cmd = sh_cmd(system, 'rm', '-f', target_fn)
    _exec_and_log(rm_cmd, logger)
    return True


def check_file(filename, system, logger=None, import_image=False):
    """
    check the validatity of a file on the system
    """
    sh_cmd = None
    basepath = None
    sh_cmd = _sh_cmd
    basepath = system.imageDir
    image_fn = os.path.split(filename)[1]
    target_fn = os.path.join(basepath, image_fn)
    if import_image:
        ls_cmd = sh_cmd(system, 'ls', filename)
    else:
        ls_cmd = sh_cmd(system, 'ls', target_fn)
    ret = _exec_and_log(ls_cmd, logger)

    if ret == 0:
        return True
    return False


def hash_file(filename, system, logger=None):
    """
    Calculate a hash of the image file.
    """
    sh_cmd = _sh_cmd
    hash_cmd = sh_cmd(system, 'fasthash', filename)
    ret = _get_stdout_and_log(hash_cmd, logger)
    if len(ret[0]) != 0:
        raise OSError("Error calculating hash: {ret[0]}")
    if logger is not None:
        logger.info("fasthash returning: {ret[1]}")
    # return hash, strip off new line at end
    return ret[1].strip()


def transfer(system, image_path, metadata_path=None, logger=None,
             import_image=False, dest_path=None):
    """
    transfer an image and its metadata to the system
    """
    # TODO: Catch copy_file fail here
    if metadata_path is not None:
        copy_file(metadata_path, system, logger)
    # If image path is None then we are just transferring the metafile
    if import_image:
        if dest_path is None:
            logger.error("Chose image import, but didn't specify dest_path")
        else:
            if image_path is None or import_copy_file(image_path, dest_path,
                                                      system, logger):
                return True
    else:
        if image_path is None or copy_file(image_path, system, logger):
            return True
    if logger is not None:
        logger.error(f"Transfer of {image_path} failed")
    return False


def remove(system, image_path, metadata_path=None, logger=None):
    """
    remove an image and its metadata from the system
    """
    if metadata_path is not None:
        remove_file(metadata_path, system, logger)
    if remove_file(image_path, system, logger):
        return True
    if logger is not None:
        logger.error("Remove of %s failed" % image_path)
    return False


def imagevalid(system, image_path, metadata_path=None, logger=None):
    """
    check if image exists on the system
    """
    metadata_ok = True
    if metadata_path is not None:
        metadata_ok = check_file(metadata_path, system, logger)
    image_ok = check_file(image_path, system, logger)

    return metadata_ok and image_ok
