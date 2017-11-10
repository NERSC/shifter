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
Install, remove, and manipulate files on the systems either local or remote

Will use local shell/copy commands to perform needed actions if the system has
filesystems locally available.  Uses ssh for remote access to platforms.
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


def _ssh_cmd(system, *args):
    """
    Helper function to build a remote shell command
    """
    if len(args) == 0:
        return None

    ssh = ['ssh']

    # TODO think about if the host selection needs to be smarter
    # also, is this guaranteed to be an iterable object?
    hostname = system['host'][0]
    username = system['ssh']['username']
    if 'key' in system['ssh']:
        ssh.extend(['-i', '%s' % system['ssh']['key']])
    if 'sshCmdOptions' in system['ssh']:
        ssh.extend(system['ssh']['sshCmdOptions'])
    ssh.extend(['%s@%s' % (username, hostname)])
    ssh.extend(args)
    return ssh


def _scp_cmd(system, localfile, remotefile):
    """
    Helper function to build a remote copy command
    """
    ssh = ['scp']

    # TODO think about if the host selection needs to be smarter
    # also, is this guaranteed to be an iterable object?
    hostname = system['host'][0]
    username = system['ssh']['username']
    if 'key' in system['ssh']:
        ssh.extend(['-i', '%s' % system['ssh']['key']])
    if 'scpCmdOptions' in system['ssh']:
        ssh.extend(system['ssh']['scpCmdOptions'])
    ssh.extend([localfile, '%s@%s:%s' % (username, hostname, remotefile)])
    return ssh


def _import_cp_cmd(system, localfile, remotefile):
    """
    Helper function to build a remote copy command
    Image will always be in user space on the system
    Worker could be local or remote, so just treat as remote
    """
    ssh = ['ssh']

    # TODO think about if the host selection needs to be smarter
    # also, is this guaranteed to be an iterable object?
    hostname = system['host'][0]
    username = system['ssh']['username']
    if 'key' in system['ssh']:
        ssh.extend(['-i', '%s' % system['ssh']['key']])
    if 'scpCmdOptions' in system['ssh']:
        ssh.extend(system['ssh']['scpCmdOptions'])
    ssh.extend(['%s@%s' % (username, hostname)])
    ssh.extend(['cp %s %s' % (localfile, remotefile)])
    return ssh


def _exec_and_log(cmd, logger):
    """
    Execute a command and log the results to logger
    """
    if logger is not None:
        logger.info("about to exec: %s" % ' '.join(cmd))
    proc = Popen(cmd, stdout=PIPE, stderr=PIPE)
    if proc is None:
        if logger is not None:
            logger.error("Could not execute '%s'" % ' '.join(cmd))
        return
    stdout, stderr = proc.communicate()
    if logger is not None:
        if stdout is not None and len(stdout) > 0:
            logger.debug("%s stdout: %s" % (cmd[0], stdout.strip()))
        if stderr is not None and len(stderr) > 0:
            logger.error("%s stderr: %s" % (cmd[0], stderr.strip()))
    return proc.returncode


def _get_stdout_and_log(cmd, logger=None):
    """
    Execute a command and return stdout and stderr. Log the results to logger
    """
    if logger is not None:
        logger.info("about to exec: %s" % ' '.join(cmd))
    rerror = ''
    proc = Popen(cmd, stdout=PIPE, stderr=PIPE)
    if proc is None:
        if logger is not None:
            logger.debug("Could not execute '%s'" % ' '.join(cmd))
            rerror = "Could not execute '%s'" % ' '.join(cmd)
        return rerror, ''
    stdout, stderr = proc.communicate()
    if logger is not None:
        if stdout is not None and len(stdout) > 0:
            logger.debug("%s stdout: %s" % (cmd[0], stdout.strip()))
        if stderr is not None and len(stderr) > 0:
            logger.debug("%s stderr: %s" % (cmd[0], stderr.strip()))
            # push this error back to calling function so
            # it can be reported sensibly
            rerror = "%s %s" % (cmd[0], stderr.strip())
    return rerror, stdout


def pre_create_tempfile(basepath, filename, sh_cmd, system, logger=None):
    """
    Generate a tempfile for filename on the system
    """

    # TODO: Add command to setup the file with the right striping
    partial_fn = '%s.XXXXXX.partial' % filename
    temp_fn = os.path.join(basepath, partial_fn)

    cmd = sh_cmd(system, 'mktemp', temp_fn)
    if logger is not None:
        logger.info('about to exec: %s' % ' '.join(cmd))
    proc = Popen(cmd, stdout=PIPE, stderr=PIPE)
    temp_fn = None
    if proc is not None:
        stdout, stderr = proc.communicate()
        if proc.returncode == 0:
            temp_fn = stdout.strip()
        else:
            memo = 'Failed to precreate transfer file, %s (%d)' \
                   % (stderr, proc.returncode)
            raise OSError(memo)
        if len(stderr) > 0 and logger is not None:
            logger.error("%s" % (stderr.strip()))
    return temp_fn


def copy_file(filename, system, logger=None):
    """
    Copy a file to the specified system
    """
    sh_cmd = None
    cp_cmd = None
    basepath = None
    if system['accesstype'] == 'local':
        sh_cmd = _sh_cmd
        cp_cmd = _cp_cmd
        basepath = system['local']['imageDir']
    elif system['accesstype'] == 'remote':
        sh_cmd = _ssh_cmd
        cp_cmd = _scp_cmd
        basepath = system['ssh']['imageDir']
    else:
        memo = '%s is not supported as a transfer type' % system['accesstype']
        raise NotImplementedError(memo)

    image_fn = os.path.split(filename)[1]
    target_fn = os.path.join(basepath, image_fn)

    # pre-create the file with a temporary name
    temp_fn = pre_create_tempfile(basepath, image_fn, sh_cmd, system, logger)

    if temp_fn is None:
        raise OSError('Got no valid response back from tempfile precreation')

    if not temp_fn.startswith(basepath):
        memo = 'Got unexpected response back from tempfile precreation: %s' \
               % temp_fn
        raise OSError(memo)

    copyret = None
    try:
        copy = cp_cmd(system, filename, temp_fn)
        copyret = _exec_and_log(copy, logger)
    except:
        rm_cmd = sh_cmd(system, 'rm', temp_fn)
        _exec_and_log(rm_cmd, logger)
        raise

    if copyret == 0:
        try:
            mv_cmd = sh_cmd(system, 'mv', temp_fn, target_fn)
            ret = _exec_and_log(mv_cmd, logger)
            return ret == 0
        except:
            # TODO we might also need to remove target_fn in this case
            rm_cmd = sh_cmd(system, 'rm', temp_fn)
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
    if system['accesstype'] == 'local':
        sh_cmd = _sh_cmd
        cp_cmd = _cp_cmd
        basepath = system['local']['imageDir']
    elif system['accesstype'] == 'remote':
        sh_cmd = _ssh_cmd
        cp_cmd = _import_cp_cmd
        basepath = system['ssh']['imageDir']
    else:
        memo = '%s is not supported as a transfer type' % system['accesstype']
        raise NotImplementedError(memo)

    image_fn = os.path.split(destfilename)[1]
    target_fn = os.path.join(basepath, image_fn)

    # pre-create the file with a temporary name
    temp_fn = pre_create_tempfile(basepath, image_fn, sh_cmd, system, logger)

    if temp_fn is None:
        raise OSError('Got no valid response back from tempfile precreation')

    if not temp_fn.startswith(basepath):
        memo = 'Got unexpected response back from tempfile precreation: %s' \
               % temp_fn
        raise OSError(memo)

    copyret = None
    try:
        copy = cp_cmd(system, filename, temp_fn)
        copyret = _exec_and_log(copy, logger)
    except:
        rm_cmd = sh_cmd(system, 'rm', temp_fn)
        _exec_and_log(rm_cmd, logger)
        raise

    if copyret == 0:
        try:
            mv_cmd = sh_cmd(system, 'mv', temp_fn, target_fn)
            ret = _exec_and_log(mv_cmd, logger)
            return ret == 0
        except:
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
    if system['accesstype'] == 'local':
        sh_cmd = _sh_cmd
        basepath = system['local']['imageDir']
    elif system['accesstype'] == 'remote':
        sh_cmd = _ssh_cmd
        basepath = system['ssh']['imageDir']
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
    if system['accesstype'] == 'local':
        sh_cmd = _sh_cmd
        basepath = system['local']['imageDir']
    elif system['accesstype'] == 'remote':
        sh_cmd = _ssh_cmd
        basepath = system['ssh']['imageDir']
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
    Calculate a hash of the image file,
    because this can be remote or local,
    need to use a separate helper executable fasthash
    assume it is in the path already
    """
    if system['accesstype'] == 'local':
        sh_cmd = _sh_cmd
    elif system['accesstype'] == 'remote':
        sh_cmd = _ssh_cmd
    hash_cmd = sh_cmd(system, 'fasthash', filename)
    ret = _get_stdout_and_log(hash_cmd, logger)
    if len(ret[0]) != 0:
        raise OSError("Error calculating hash: %s" % (ret[0]))
    if logger is not None:
        logger.info("fasthash returning: %s" % ret[1])
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
        logger.error("Transfer of %s failed" % image_path)
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
