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

import os
import unittest
from shifter_imagegw import converters


class ConvertersTestCase(unittest.TestCase):

    def setUp(self):
        #os.environ['PATH']=os.environ['PATH']+":./test"
        test_dir = os.path.dirname(os.path.abspath(__file__)) + "/../test/"
        self.test_dir = test_dir
        if 'TMPDIR' in os.environ:
            self.outdir = os.environ['TMPDIR']
        else:
            self.outdir = '/tmp/'

    def make_fake(self):
        path = self.outdir + "/fakeimage"
        if os.path.exists(path) is False:
            os.makedirs(path)
        with open(path + '/a', "w") as f:
            f.write('blah')
        return path

    def tearDown(self):
        pass

# generate_ext4_image(expand_path, image_path):
# generate_cramfs_image(expand_path, image_path):
# generate_squashfs_image(expand_path, image_path):
    def test_convert(self):
        """
        Test convert function using a mock format
        """
        output = '%s/test.squashfs' % (self.outdir)
        resp = converters.convert('mock', '', output)
        assert resp is True
        assert os.path.exists(output)

        path = self.make_fake()

        resp = converters.convert('cramfs', path, '/tmp/blah.cramfs')
        self.assertTrue(resp)

        with self.assertRaises(NotImplementedError):
            resp = converters.convert('ext4', path, '/tmp/blah.ext4')

        resp = converters.convert('squashfs', path, output)
        self.assertTrue(resp)

    def test_writemeta(self):
        """
        Test Write meta function
        """

        meta = {'workdir': "/tmp/",
                'entrypoint': '/bin/sh',
                'env': ['a=b', 'c=d'],
                'private': True,
                'userACL': [1000, 1001],
                'groupACL': [1002, 1003],
                }
        output = '%s/test.meta' % (self.outdir)
        resp = converters.writemeta('squashfs', meta, output)
        assert resp is not None
        meta = {'ENV': []}
        with open(output) as f:
            for line in f:
                (k, v) = line.strip().split(": ", 2)
                if k == 'ENV':
                    meta['ENV'].append(v)
                else:
                    meta[k] = v
        keys = ('WORKDIR', 'FORMAT', 'ENTRY', 'USERACL', 'GROUPACL')
        for key in keys:
            self.assertIn(key, meta)
        self.assertEquals(meta['USERACL'].find("["), -1)
        self.assertEquals(meta['USERACL'].find("]"), -1)
        self.assertGreater(len(meta['ENV']), 0)

    def test_ext4(self):
        with self.assertRaises(NotImplementedError):
            converters.generate_ext4_image('/tmp', '/tmp')

    def test_cramfs(self):
        converters.generate_cramfs_image('/tmp/b', '/tmp/blah')
        self.assertTrue(os.path.exists('/tmp/blah'))
        os.remove('/tmp/blah')

    def test_squashfs(self):
        converters.generate_squashfs_image('/tmp/b', '/tmp/blah')
        self.assertTrue(os.path.exists('/tmp/blah'))
        os.remove('/tmp/blah')
