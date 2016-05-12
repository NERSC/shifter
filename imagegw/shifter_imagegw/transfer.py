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

def copy_file(filename,system):
    (basePath,imageFilename) = os.path.split(filename)
    remoteFilename = os.path.join(ssh['imageDir'], imageFilename)
    remoteTempFilename = os.path.join(ssh['imageDir'], '%s.XXXXXX.partial' % imageFilename)
    shCmd = None
    cpCmd = None
    if system['accesstype'] == 'local':
        shCmd = _shCmd
        cpCmd = _cpCmd
    elif system['accesstype'] == 'remote':
        shCmd = _sshCmd
        cpCmd = _scpCmd
    else:
        raise NotImplementedError('%s is not supported as a transfer type' % system['accesstype'])

    # pre-create the file with a temporary name
    # TODO: Add command to setup the file with the right striping
    preCreate = shCmd(system, 'mktemp', remoteTempFilename)
    proc = subprocess.Popen(preCreate, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    remoteTempFilename = None
    if proc is not None:
    	stdout,stderr = proc.communicate()
        if proc.returncode == 0:
            remoteTempFilename = stdout
        else:
            raise OSError('Failed to precreate transfer file, %s (%d)' % (stderr, proc.returncode))

    if remoteTempFilename is None or not remoteTempFilename.startswith(ssh['imageDir']):
        raise OSError('Got unexpected response back from tempfile precreation: %s' % stdout)
    
    copy = cpCmd(system, filename, remoteTempFilename)
    #print "DEBUG: %s"%(scpCmd.join(' '))
    fdnull=open('/dev/null','w')

    ret = subprocess.call(copy)#, stdout=fdnull, stderr=fdnull)
    if ret == 0:
        mvCmd = shCmd(system, 'mv', remoteTempFilename, remoteFilename)
        ret = subprocess.call(mvCmd)
    return ret == 0

def transfer(system,imagePath,metadataPath=None):
    if metadataPath is not None:
        copy_file(metadataPath, system)
    if copy_file(imagePath,system):
        return True
    print "Copy failed"
    return False
