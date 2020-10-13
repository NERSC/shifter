#!/usr/bin/env python

import pexpect
import subprocess
import sys

image_name = sys.argv[1]

proc = subprocess.Popen(['whoami'], stdout=subprocess.PIPE)
stdout, _ = proc.communicate()
username = stdout.strip()

proc = subprocess.Popen(['getent','passwd',username], stdout=subprocess.PIPE)
passwd, _ = proc.communicate()

pex = pexpect.spawnu("shifter --image=%s cat /etc/passwd" % image_name)
pex.expect(str(passwd.replace('\n', '\r\n')).encode("utf-8"))
