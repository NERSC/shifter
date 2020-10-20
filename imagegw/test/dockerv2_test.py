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
        return
        for path in self.cleanpaths:
            shutil.rmtree(path)

    def test_whiteout(self):
        cache = tempfile.mkdtemp()
        expand = tempfile.mkdtemp()
        self.cleanpaths.append(cache)
        self.cleanpaths.append(expand)
        return
        resp = dockerv2.pull_image(self.options, 'dmjacobsen/whiteouttest',
                                   'latest', cachedir=cache, expanddir=expand)

        assert os.path.exists(resp['expandedpath'])
        assert os.path.exists(os.path.join(resp['expandedpath'], 'usr'))
        noexist = ['usr/local', 'usr/.wh.local']
        for loc in noexist:
            path = os.path.join(resp['expandedpath'], loc)
            assert not os.path.exists(path)

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
        self.assertIn('workdir', resp)
        assert os.path.exists(resp['expandedpath'])
        assert os.path.exists(bfile)
        with open(bfile) as f:
            data = f.read()
            assert(data == 'blah\n')

    def test_unicode(self):
        cache = tempfile.mkdtemp()
        expand = tempfile.mkdtemp()
        self.cleanpaths.append(cache)
        self.cleanpaths.append(expand)

        resp = dockerv2.pull_image(self.options, 'scanon/unicode',
                                   'latest', cachedir=cache,
                                   expanddir=expand.encode('ascii'))

        assert os.path.exists(resp['expandedpath'])
        bfile = os.path.join(resp['expandedpath'], u'\ua000')
        assert os.path.exists(resp['expandedpath'])
        assert os.path.exists(bfile)

    def test_chgtype(self):
        cache = tempfile.mkdtemp()
        expand = tempfile.mkdtemp()
        self.cleanpaths.append(cache)
        self.cleanpaths.append(expand)

        resp = dockerv2.pull_image(self.options, 'scanon/chgtype', 'latest',
                                   cachedir=cache, expanddir=expand)
        assert os.path.exists(resp['expandedpath'])
        bfile = os.path.join(resp['expandedpath'], 'build/test')
        assert os.path.exists(bfile)
        bfile = os.path.join(resp['expandedpath'], 'build/test2')
        assert os.path.exists(bfile)

    def test_import(self):
        cache = tempfile.mkdtemp()
        expand = tempfile.mkdtemp()
        self.cleanpaths.append(cache)
        self.cleanpaths.append(expand)

        resp = dockerv2.pull_image(self.options, 'scanon/importtest', 'latest',
                                   cachedir=cache, expanddir=expand)
        assert os.path.exists(resp['expandedpath'])
        bfile = os.path.join(resp['expandedpath'], 'home')
        self.assertTrue(os.path.islink(bfile))

    def test_need_proxy(self):
        """
        Test if proxy is needed
        """
        os.environ['no_proxy'] = 'blah.com,blah2.com'
        self.assertTrue(dockerv2.need_proxy('proxy.blah3.com'))
        self.assertFalse(dockerv2.need_proxy('proxy.blah.com'))

    def test_setup_conn(self):
        """
        Test setup connection
        """
        url = 'https://registry-1.docker.io/v2/'
        conn = dockerv2._setup_http_conn(url)
        self.assertIsNotNone(conn)
        url = 'http://registry-1.docker.io/v2/'
        conn = dockerv2._setup_http_conn(url)
        self.assertIsNotNone(conn)
        url = 'ftp:/bogus.com/v2/'
        with self.assertRaises(ValueError):
            conn = dockerv2._setup_http_conn(url)
        os.environ['https_proxy'] = 'https://localhost:9999'
        url = 'https://registry-1.docker.io/v2/'
        # should fail with an IOError because it is a bogus proxy
        with self.assertRaises(IOError):
            conn = dockerv2._setup_http_conn(url)
        os.environ['http_proxy'] = 'http://localhost:9999'
        url = 'http://registry-1.docker.io/v2/'
        # should fail with an IOError because it is a bogus proxy
        with self.assertRaises(IOError):
            conn = dockerv2._setup_http_conn(url)
        os.environ.pop('https_proxy')
        os.environ.pop('http_proxy')


if __name__ == '__main__':
    unittest.main()
