#!/usr/bin/env python

import requests, json, os, sys

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

server=os.environ['IMAGEGW']
mungeenv=os.environ['MUNGE']

def usage(me):
   print "usage: %s <lookup|pull> system image"%(me)
   sys.exit(1)

if __name__ == '__main__':
    me=sys.argv.pop(0)
    if len(sys.argv)<1:
        usage(me)
    com=sys.argv.pop(0)
    if os.path.exists(mungeenv):
      with open(mungeenv) as m:
          munge=m.read()
    else:
      munge=mungeenv
    url = "http://%s/api"%(server)

    if com=='lookup' and len(sys.argv)>=2:
         (system,image)=sys.argv
         header={'authentication':munge.strip()}
         uri="%s/lookup/%s/docker/%s/"%(url,system,image)
         print uri
         r = requests.get(uri,headers=header)
         print r.text
    elif com=='pull' and len(sys.argv)>=2:
         (system,image)=sys.argv
         header={'authentication':munge.strip()}
         uri="%s/pull/%s/docker/%s/"%(url,system,image)
         r = requests.post(uri,headers=header)
         print r.text
    else:
        usage(me)
