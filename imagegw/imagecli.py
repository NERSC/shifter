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
Basic CLI tool to talk to the image manager.  This is not well supported.
"""

import json
import os
import sys
import requests
from shifter_imagegw.munge import munge

SERVER = os.environ['IMAGEGW']
MUNGEENV = os.environ['MUNGE']

def usage(program):
    """ usage line """
    print "usage: %s <lookup|pull> system image" % (program)
    sys.exit(1)

def main():
    """ main """
    program = sys.argv.pop(0)
    if len(sys.argv) < 1:
        usage(program)
    com = sys.argv.pop(0)
    if MUNGEENV == "1":
        mungetok = munge("test")
    elif os.path.exists(MUNGEENV):
        with open(MUNGEENV) as mungec:
            mungetok = mungec.read()
    else:
        mungetok = MUNGEENV
    url = "http://%s/api"%(SERVER)

    if com == 'lookup' and len(sys.argv) >= 2:
        (system, image) = sys.argv[0:2]
        header = {'authentication': mungetok.strip()}
        uri = "%s/lookup/%s/docker/%s/" % (url, system, image)
        req = requests.get(uri, headers=header)
        print req.text
    elif com == 'list' and len(sys.argv) >= 1:
        system = sys.argv[0]
        header = {'authentication': mungetok.strip()}
        uri = "%s/list/%s/" % (url, system)
        req = requests.get(uri, headers=header)
        if req.status_code != 200:
            print "List failed"
            sys.exit(1)
        resp = json.loads(req.text)
        for res in resp['list']:
            tags = res['tag']
            if not isinstance(tags, list):
                tags = [tags]
            for img in tags:
                (image, tag) = img.split(':')
                print '%-25.25s  %-20.10s  %-12.12s   %-10.10s' % \
                     (image, tag, req['id'], req['itype'])
    elif com == 'pull' and len(sys.argv) >= 2:
        (system, image) = sys.argv[0:2]
        header = {'authentication':mungetok.strip()}
        uri = "%s/pull/%s/docker/%s/" % (url, system, image)
        req = requests.post(uri, headers=header)
        print req.text
    else:
        usage(program)

if __name__ == '__main__':
    main()
