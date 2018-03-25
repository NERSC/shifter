#!/usr/bin/env python

import pexpect
import subprocess
import sys

image_name = sys.argv[1]

pex = pexpect.spawnu("shifter --image=%s ls -ld /etc/mtab" % image_name)
pex.expect(u'/proc/mounts')
pex = pexpect.spawnu("shifter --image=%s cat /etc/mtab" % image_name)
