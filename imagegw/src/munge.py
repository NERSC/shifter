#!/usr/bin/env python
import sys
import os
from subprocess import check_output, Popen, PIPE

# Helper routines for munge


def munge(text,socket=None):
    try:
        com=["munge", '-s', text]
        if socket is not None:
            com.extend(['-S',socket])
        message = check_output(com)
        return message.replace('\n','')
    except:
        return None


def unmunge(encoded,socket=None):
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
