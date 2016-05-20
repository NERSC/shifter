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

def copy_remote(filename,system):
    (basePath,imageFilename) = os.path.split(filename)
    ssh=system['ssh']
    remoteFilename = os.path.join(ssh['imageDir'], imageFilename)
    scpCmd = ['scp']
    if 'key' in ssh:
        scpCmd.extend(['-i','%s'%ssh['key']])
    if 'scpCmdOptions' in system:
        scpCmd.extend(system['scpCmdOptions'])
    hostname=system['host'][0]
    # TODO: Add command to pre-create the file with the right striping
    scpCmd.extend([filename,'%s@%s:%s' % (ssh['username'],hostname, remoteFilename)])
    fdnull=open('/dev/null','w')

    ret = subprocess.call(scpCmd)#, stdout=fdnull, stderr=fdnull)
    return ret == 0

def copy_local(filename,system):
    (basePath,imageFilename) = os.path.split(filename)
    local=system['local']
    targetFilename = os.path.join(local['imageDir'], imageFilename)
    cpCmd = ['cp']
    if 'cpCmdOptions' in system:
        cpCmd.extend(system['cpCmdOptions'])
    # TODO: Add command to pre-create the file with the right striping
    cpCmd.extend([filename, targetFilename])
    fdnull=open('/dev/null','w')

    ret = subprocess.call(cpCmd)#, stdout=fdnull, stderr=fdnull)
    return ret == 0

def remove_remote(filename,system):
    (basePath,imageFilename) = os.path.split(filename)
    ssh=system['ssh']
    remoteFilename = os.path.join(ssh['imageDir'], imageFilename)
    sshCmd = ['ssh']
    if 'key' in ssh:
        sshCmd.extend(['-i','%s'%ssh['key']])
    if 'sshCmdOptions' in system:
        sshCmd.extend(system['scpCmdOptions'])
    hostname=system['host'][0]
    sshCmd.extend([hostname,'rm', remoteFilename])
    ret = subprocess.call(sshCmd)
    return ret == 0

def remove_local(filename,system):
    (basePath,imageFilename) = os.path.split(filename)
    targetFilename = os.path.join(system['local']['imageDir'], imageFilename)
    os.unlink(targetFilename)
    return True

def transfer(system,imagePath,metadataPath=None):
    atype=system['accesstype']
    if atype=='remote':
        if metadataPath is not None:
            copy_remote(metadataPath, system)
        if copy_remote(imagePath,system):
            return True
        else:
            print "Copy failed"
            return False
    elif atype=='local':
        if metadataPath is not None:
            copy_local(metadataPath, system)
        if copy_local(imagePath,system):
            return True
        else:
            print "Copy failed"
            return False
    else:
        raise NotImplementedError('%s is not supported as a transfer type'%atype)
    return False

def remove(system,imagePath,metadataPath=None):
    atype=system['accesstype']
    if atype=='remote':
        if metadataPath is not None:
            remove_remote(metadataPath, system)
        if remove_remote(imagePath,system):
            return True
        else:
            print "Remove failed"
            return False
    elif atype=='local':
        if metadataPath is not None:
            remove_local(metadataPath, system)
        if remove_local(imagePath,system):
            return True
        else:
            print "Remove failed"
            return False
    else:
        raise NotImplementedError('%s is not supported as a remove transfer type'%atype)
    return False
