#!/usr/bin/env python

import pexpect
import subprocess
import sys

image_name = sys.argv[1]

pex = pexpect.spawnu("shifter --image=%s cat /proc/self/status | grep Cap" % image_name)
pex.expect('CapInh:\s+0+')
pex.expect('CapPrm:\s+0+')
pex.expect('CapEff:\s+0+')
pex.expect('CapBnd:\s+0+')
