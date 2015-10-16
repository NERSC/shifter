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
    #print "DEBUG: %s"%(scpCmd.join(' '))
    fdnull=open('/dev/null','w')

    ret = subprocess.call(scpCmd)#, stdout=fdnull, stderr=fdnull)
    return ret == 0


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
        # do something
        print "TODO"
    else:
        raise NotImplementedError('%s is not supported as a transfer type'%atype)
    return False
