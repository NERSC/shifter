#!/usr/bin/env python

import sys
import os
import subprocess
import logging
import glob
import select
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
Wrapper to call oscap scanner.  This is still a WIP.
"""


def check_io(child):
    """
    Process IO lines
    """
    ready_to_read = select.select([child.stdout, child.stderr],
                                  [], [], 1000)[0]
    for io in ready_to_read:
        line = io.readline()
        print line


print "Warning: This wrapper is not yet fully functional.  Do not use."
profiled = '/usr/share/xml/scap/ssg/content/ssg-rhel7-ds.xml'
image_loc = sys.argv[1]
image_id = sys.argv[2]

stdout_log_level = logging.DEBUG
stderr_log_level = logging.ERROR
etcp = os.path.join(image_loc, 'etc')
for file in os.listdir(etcp):
    fname = glob.glob('%s/*-release' % (etcp))
finp = open(fname[0], 'r')
fcont = finp.readline()
print fcont

if "Ubuntu" in fcont:
    print "Image is Ubuntu_based"

elif fcont.find("CentOS") == 0:
    print "Image is centos based"

elif fcont.find("Fedora") == 0:
    print "Image is Fedora based"

elif "Debian" in fcont:
    print "Image is Debian based"
elif "Red Hat" in fcont:
    print "Image is Redhat Linux based"
elif "Scientific" in fcont:
    print "Image is Scientific Linux based"
else:
    print "Some other unsupported flavour of Scanner"


command = ['oscap-chroot', image_loc, 'xccdf', 'eval',
           '--fetch-remote-resources', '--profile',
           'xccdf_org.ssgproject.content_profile_rht-ccp',
           '--report', '%s_report.html' % image_id, profiled]
child = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)
log_level = {child.stdout: stdout_log_level, child.stderr: stderr_log_level}


# keep checking stdout/stderr until the child exits
while child.poll() is None:
    check_io(child)

check_io(child)  # check again to catch anything after the process exits

sys.exit(child.wait())
