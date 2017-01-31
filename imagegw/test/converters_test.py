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
        self.test_dir = os.path.dirname(os.path.abspath(__file__)) + "/../test/"

    def tearDown(self):
        pass

# generate_ext4_image(expand_path, image_path):
# generate_cramfs_image(expand_path, image_path):
# generate_squashfs_image(expand_path, image_path):
    def test_convert(self):
        """
        Test convert function using a mock format
        """
        output='%s/test.squashfs'%(os.environ['TMPDIR'])
        resp=converters.convert('mock','',output)
        assert resp is True
        assert os.path.exists(output)

    def test_writemeta(self):
        """
        Test Write meta function
        """

        meta = {'workdir': "/tmp/",
                'entrypoint': '/bin/sh',
                'env': ['a=b', 'c=d'],
                'userACL': ['usera', 'userb'],
                'groupACL': ['groupa', 'groupb'],
                }
        output = '%s/test.meta' % (os.environ['TMPDIR'])
        resp = converters.writemeta('squashfs', meta, output)
        assert resp is not None
        meta = {'ENV': []}
        with open(output) as f:
            for line in f:
                (k, v) = line.strip().split(": ", 2)
                print "%s = %s" % (k, v)
                if k == 'ENV':
                    meta['ENV'].append(v)
                else:
                    meta[k] = v
        print meta
        assert 'WORKDIR' in meta
        assert 'FORMAT' in meta
        assert 'ENTRY' in meta
        assert 'USERACL' in meta
        assert 'GROUPACL' in meta
        assert len(meta['ENV']) > 0
        return


if __name__ == '__main__':
    unittest.main()
