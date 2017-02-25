# Shifter, Copyright (c) 2016, The Regents of the University of California,
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

import os
from shifter_imagegw import dockerv2
import unittest
import tempfile
import shutil


class Dockerv2TestCase(unittest.TestCase):

    def setUp(self):
        cwd = os.path.dirname(os.path.realpath(__file__))
        os.environ['PATH'] = cwd + ':' + os.environ['PATH']
        self.options = None
        if 'LOCALREGISTRY' in os.environ:
            self.options = {'baseUrl': os.environ['LOCALREGISTRY']}
        self.cleanpaths = []

    def tearDown(self):
        for path in self.cleanpaths:
            shutil.rmtree(path)

    def test_whiteout(self):
        cache = tempfile.mkdtemp()
        expand = tempfile.mkdtemp()
        self.cleanpaths.append(cache)
        self.cleanpaths.append(expand)

        resp = dockerv2.pull_image(self.options, 'dmjacobsen/whiteouttest',
                                   'latest', cachedir=cache, expanddir=expand)

        assert os.path.exists(resp['expandedpath'])
        assert os.path.exists(os.path.join(resp['expandedpath'], 'usr'))
        noexist = ['usr/local', 'usr/.wh.local']
        for loc in noexist:
            path = os.path.join(resp['expandedpath'], loc)
            assert not os.path.exists(path)

        return

    # This test case has files that in one layer are made non-writeable.
    # This requires fixing permissions on the parent layers before extraction.
    # This test uses a prepared image scanon/shanetest
    def test_permission_fixes(self):
        cache = tempfile.mkdtemp()
        expand = tempfile.mkdtemp()
        self.cleanpaths.append(cache)
        self.cleanpaths.append(expand)

        resp = dockerv2.pull_image(self.options, 'scanon/shanetest', 'latest',
                                   cachedir=cache, expanddir=expand)

        assert os.path.exists(resp['expandedpath'])
        bfile = os.path.join(resp['expandedpath'], 'tmp/b')
        assert os.path.exists(bfile)
        with open(bfile) as f:
            data = f.read()
            assert(data == 'blah\n')
        return


if __name__ == '__main__':
    unittest.main()
