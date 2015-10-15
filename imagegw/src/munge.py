#!/usr/bin/env python
import sys
import os
import subprocess

# Helper routines for munge


def munge(text,socket=None):
    try:
        com=["munge", '-s', text]
        if socket is not None:
            com.append(['-S',socket])
        message = subprocess.check_output(com)
        return message.replace('\n','')
    except:
        return None


def unmunge(encoded,socket=None):
    try:
        com=["unmunge"]
        if socket is not None:
            com.append(['-S',socket])
        output = subprocess.check_output(com)
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
        r=unmunge('')
        print r
    else:
        usage(me)
