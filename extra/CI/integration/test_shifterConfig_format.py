#!/usr/bin/env python

import pexpect
import subprocess
import sys

image_name = sys.argv[1]

pex = pexpect.spawnu("whoami")
pex.expect(u'(\S+)')
username = pex.match.groups()[0]

pex = pexpect.spawnu("shifterimg lookup %s" % image_name)
pex.expect(u'([0-9a-f]+)')
image_id = pex.match.groups()[0]

expected_config = u'{"identifier":"%s","user":"%s","volMap":""}' \
        % (image_id, username)

pex = pexpect.spawnu("shifter --image=%s cat /var/shifterConfig.json" % image_name)
pex.expect(expected_config)
