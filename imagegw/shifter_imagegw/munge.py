#!/usr/bin/env python
# Shifter, Copyright (c) 2015, The Regents of the University of California,
# through Lawrence Berkeley National Laboratory (subject to receipt of any
# required approvals from the U.S. Dept. of Energy).  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
# 3. Neither the name of the University of California, Lawrence Berkeley
#    National Laboratory, U.S. Dept. of Energy nor the names of its
#    contributors may be used to endorse or promote products derived from this
#    software without specific prior written permission.`
#
# See LICENSE for full text.

"""
Helper routines for munge
"""

import sys
from subprocess import Popen, PIPE
debug = False


def munge(text, socket=None):
    """
    munge text using the optional socket
    """
    try:
        com = ["munge", '-s', text]
        if socket is not None:
            com.extend(['-S', socket])
        proc = Popen(com, stdout=PIPE, stderr=PIPE)
        stdout = str(proc.communicate()[0])
        return stdout.replace('\n', '')
    except:
        return None


def unmunge(encoded, socket=None):
    """
    Unmunge an encoded string using an optional socket.
    returns a dictionary object.
    raises exceptions if it fails.
    """
    try:
        com = ["unmunge"]
        if socket is not None:
            com.extend(['-S', socket])
        proc = Popen(com, stdin=PIPE, stdout=PIPE, stderr=PIPE)
        output = proc.communicate(input=encoded.encode('utf-8'))[0]
        if proc.returncode == 15:
            raise OSError("Expired Credential")
        if proc.returncode == 17:
            raise OSError("Replayed Credential")
        elif proc.returncode != 0:
            memo = "Unknown munge error %d %s" % (proc.returncode, socket)
            raise OSError(memo)
        resp = dict()
        inmessage = False
        message = ''
        for line in output.decode("utf-8").splitlines():
            if line == '':
                inmessage = True
                continue
            if inmessage is True:
                message += line
                continue
            index = line.find(':')
            if index >= 0:
                key = line[:index]
                value = line[index + 1:].lstrip()
                resp[key] = value
        if 'STATUS' not in resp:
            return None
        if resp['STATUS'] != 'Success (0)':
            return None
        resp['MESSAGE'] = message
        return resp
    except:
        if debug:
            print(sys.exc_info()[1])
        raise


def usage(program):
    """
    Help for test mode of munge helpers
    """
    print("%s <munge|unmunge>" % (program))


def _main():
    """
    Brief test/validation code for munge helpers
    """
    program = sys.argv.pop(0)
    if len(sys.argv) < 1:
        usage(program)
        sys.exit()

    command = sys.argv.pop(0)
    if command == 'munge':
        print(munge('test'))
    elif command == "unmunge":
        message = sys.stdin.read().strip()
        resp = unmunge(message, socket="/var/run/munge/munge.socket")
        print("Response: " + resp)
    else:
        usage(program)


if __name__ == '__main__':
    _main()
