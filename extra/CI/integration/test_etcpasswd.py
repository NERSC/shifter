#!/usr/bin/env python

import subprocess
import sys

image_name = sys.argv[1]

proc = subprocess.Popen(['whoami'], stdout=subprocess.PIPE)
stdout, _ = proc.communicate()
username = stdout.strip()

proc = subprocess.Popen(['getent','passwd',username], stdout=subprocess.PIPE)
passwd = proc.communicate()[0].decode("utf-8").rstrip()

cmd = ['shifter', '--image=%s' % (image_name), 'cat', '/etc/passwd']
proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)
out = proc.communicate()[0].decode("utf-8")

for line in out.split('\n'):
    if line.startswith(passwd):
        sys.exit(0)

sys.exit(1)

