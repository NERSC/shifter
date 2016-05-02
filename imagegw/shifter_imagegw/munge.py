#!/usr/bin/env python
import sys
import os
from subprocess import Popen, PIPE

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
# Helper routines for munge

def munge(text,socket=None):
    """
    munge text using the optional socket
    """
    try:
        com=["munge", '-s', text]
        if socket is not None:
            com.extend(['-S',socket])
        proc = Popen(com, stdout=PIPE, stderr=PIPE);
        (stdout,stderr) = p.communicate()
        return stdout.replace('\n','')
    except:
        return None


def unmunge(encoded,socket=None):
    """
    Unmunge an encoded string using an optional socket.
    returns a dictionary object.
    raises exceptions if it fails.
    """
    try:
        com=["unmunge"]
        if socket is not None:
            com.extend(['-S',socket])
        p = Popen(com,stdin=PIPE,stdout=PIPE,stderr=PIPE)
        output=p.communicate(input=encoded)[0]
        if p.returncode==15:
            raise OSError("Expired Credential")

        if p.returncode!=0:
            raise OSError("Unknown munge error %d %s"%(p.returncode,socket))

        resp=dict()
        inmessage=False
        message=''
        for line in output.splitlines():
            if line=='':
                inmessage=True
                continue
            if inmessage is True:
                message+=line
                continue
            index=line.find(':')
            if index>=0:
                key=line[:index]
                value=line[index+1:].lstrip()
                resp[key]=value
        if 'STATUS' not in resp:
            return None
        if resp['STATUS']!='Success (0)':
            return None
        resp['MESSAGE']=message
        return resp
    except:
        print sys.exc_value
        raise
        return None


def usage(me):
  print "%s <munge|unmunge>"%(me)


if __name__ == '__main__':
    me=sys.argv.pop(0)
    if len(sys.argv)<1:
        usage(me)
        sys.exit()

    command=sys.argv.pop(0)
    if command=='munge':
        print munge('test');
    elif command=="unmunge":
        message=sys.stdin.read().strip()
        r=unmunge(message,socket="/var/run/munge/munge.socket")
        print r
    else:
        usage(me)
