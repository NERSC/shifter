#!/usr/bin/env python

import pexpect
import subprocess
import sys

image_name = sys.args[1]

pex = pexpect.spawnu("shifter --image=%s cat /proc/self/status | grep Cap" % image_name)
pex.expect(u'CapInh:\s+0+')
pex.expect(u'CapPrm:\s+0+')
pex.expect(u'CapEff:\s+0+')
pex.expect(u'CapBnd:\s+0+')
