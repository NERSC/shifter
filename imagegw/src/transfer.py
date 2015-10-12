#!/usr/bin/env python

import os
import subprocess



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
