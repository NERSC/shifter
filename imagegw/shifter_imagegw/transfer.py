#!/usr/bin/env python

import os
import subprocess

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
def _shCmd(system, *args):
    if len(args) == 0:
        return None
    return args

def _cpCmd(system, localfile, targetfile):
    return ['cp', localfile, targetfile]

def _sshCmd(system, *args):
    if len(args) == 0:
        return None

    ssh = ['ssh']

    ### TODO think about if the host selection needs to be smarter
    ### also, is this guaranteed to be an iterable object?
    hostname = system['host'][0]
    username = system['ssh']['username']
    if 'key' in system['ssh']:
        ssh.extend(['-i','%s' % system['ssh']['key']])
    if 'sshCmdOptions' in system['ssh']:
        ssh.extend(system['ssh']['sshCmdOptions'])
    ssh.extend(['%s@%s' % (username, hostname)])
    ssh.extend(args)
    return ssh

def _scpCmd(system, localfile, remotefile):
    ssh = ['scp']

    ### TODO think about if the host selection needs to be smarter
    ### also, is this guaranteed to be an iterable object?
    hostname = system['host'][0]
    username = system['ssh']['username']
    if 'key' in system['ssh']:
        ssh.extend(['-i','%s' % system['ssh']['key']])
    if 'scpCmdOptions' in system['ssh']:
        ssh.extend(system['ssh']['scpCmdOptions'])
    ssh.extend([localfile, '%s@%s:%s' % (username, hostname, remotefile)])
    return ssh

def _execAndLog(cmd, logger):
    if logger is not None:
        logger.info("about to exec: %s" % ' '.join(cmd))
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if proc is None:
        if logger is not None:
            logger.error("Could not execute '%s'" % ' '.join(cmd))
        return
    stdout,stderr = proc.communicate()
    if logger is not None:
        if stdout is not None and len(stdout) > 0:
            logger.debug("%s stdout: %s" % (cmd[0], stdout.strip()))
        if stderr is not None and len(stderr) > 0:
            logger.error("%s stderr: %s" % (cmd[0], stderr.strip()))
    return proc.returncode

def copy_file(filename, system, logger=None):
    shCmd = None
    cpCmd = None
    baseRemotePath = None
    if system['accesstype'] == 'local':
        shCmd = _shCmd
        cpCmd = _cpCmd
        baseRemotePath = system['local']['imageDir']
    elif system['accesstype'] == 'remote':
        shCmd = _sshCmd
        cpCmd = _scpCmd
        baseRemotePath = system['ssh']['imageDir']
    else:
        raise NotImplementedError('%s is not supported as a transfer type' % system['accesstype'])

    (basePath,imageFilename) = os.path.split(filename)
    remoteFilename = os.path.join(baseRemotePath, imageFilename)
    remoteTempFilename = os.path.join(baseRemotePath, '%s.XXXXXX.partial' % imageFilename)

    # pre-create the file with a temporary name
    # TODO: Add command to setup the file with the right striping
    preCreate = shCmd(system, 'mktemp', remoteTempFilename)
    if logger is not None:
        logger.info('about to exec: %s' % ' '.join(preCreate))
    proc = subprocess.Popen(preCreate, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    remoteTempFilename = None
    if proc is not None:
    	stdout,stderr = proc.communicate()
        if proc.returncode == 0:
            remoteTempFilename = stdout.strip()
        else:
            raise OSError('Failed to precreate transfer file, %s (%d)' % (stderr, proc.returncode))
        if len(stderr) > 0 and logger is not None:
            logger.error("%s stderr: %s" % (preCreate[0], stderr.strip()))

    if remoteTempFilename is None or not remoteTempFilename.startswith(baseRemotePath):
        raise OSError('Got unexpected response back from tempfile precreation: %s' % stdout)
    
    copyret = None
    try:
        copy = cpCmd(system, filename, remoteTempFilename)
        copyret = _execAndLog(copy, logger)
    except:
        rmCmd = shCmd(system, 'rm', remoteTempFilename)
        _execAndLog(rmCmd, logger)
        raise

    if copyret == 0:
        try:
            mvCmd = shCmd(system, 'mv', remoteTempFilename, remoteFilename)
            ret = _execAndLog(mvCmd, logger)
            return ret == 0
        except:
            ### we might also need to remove remoteFilename in this case
            rmCmd = shCmd(system, 'rm', remoteTempFilename)
            _execAndLog(rmCmd, logger)
            raise
    return False

#def remove_local(filename,system):
#    (basePath,imageFilename) = os.path.split(filename)
#    targetFilename = os.path.join(system['local']['imageDir'], imageFilename)
#    os.unlink(targetFilename)
#    return True

def remove_file(filename, system, logger=None):
    shCmd = None
    baseRemotePath = None
    if system['accesstype'] == 'local':
        shCmd = _shCmd
        baseRemotePath = system['local']['imageDir']
    elif system['accesstype'] == 'remote':
        shCmd = _sshCmd
        baseRemotePath = system['ssh']['imageDir']
    (basePath,imageFilename) = os.path.split(filename)
    remoteFilename = os.path.join(baseRemotePath, imageFilename)
    rmCmd = shCmd(system, 'rm','-f', remoteFilename)
    _execAndLog(rmCmd, logger)
    return True


def transfer(system,imagePath,metadataPath=None,logger=None):
    if metadataPath is not None:
        copy_file(metadataPath, system, logger)
    if copy_file(imagePath,system, logger):
        return True
    if logger is not None:
        logger.error("Transfer of %s failed" % imagePath)
    return False

def remove(system,imagePath,metadataPath=None,logger=None):
    if metadataPath is not None:
        remove_file(metadataPath, system)
    if remove_file(imagePath,system):
        return True
    if logger is not None:
        logger.error("Remove of %s failed" % imagePath)
    return False
