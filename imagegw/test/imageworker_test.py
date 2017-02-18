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
import json


class ImageWorkerTestCase(unittest.TestCase):

    def setUp(self):
        cwd = os.path.dirname(os.path.realpath(__file__))
        os.environ['PATH'] = cwd + ':' + os.environ['PATH']
        from shifter_imagegw import imageworker
        self.imageworker = imageworker
        self.configfile = 'test.json'
        with open(self.configfile) as config_file:
            self.config = json.load(config_file)
        self.system = 'systema'
        self.itype = 'docker'
        self.tag = 'registry.services.nersc.gov/nersc-py:latest'
        self.tag = 'ubuntu:latest'
        self.tag = 'scanon/shanetest:latest'
        self.hash = \
            'b3491cdefcdb79a31ab7ddf1bbcf7c8eeff9b4f00cb83a0be513fb800623f9cf'
        self.hash2 = \
            'a90493edb7d6589542c475ebfd052fe3912a153015d6e0923cfd5f40d0bc2925'
        self.hash3 = \
            'ac6b4960ac85aeb6effc8538955078fcb1f3bb9e15451efe63b753a3a566884c'
        if not os.path.exists(self.config['CacheDirectory']):
            os.mkdir(self.config['CacheDirectory'])
        self.expandedpath = \
            os.path.join(self.config['CacheDirectory'],
                         '%s_%s' % (self.itype, self.tag.replace('/', '_')))
        self.imagefile = os.path.join(self.config['ExpandDirectory'],
                                      '%s.%s' % (self.hash, 'squashfs'))
        idir = self.config['Platforms']['systema']['ssh']['imageDir']
        if not os.path.exists(idir):
            os.makedirs(idir)
        self.imageDir = idir

    def cleanup_cache(self):
        for h in [self.hash, self.hash2, self.hash3]:
            fp = '%s/%s.meta' % (self.imageDir, h)
            if os.path.exists(fp):
                os.remove(fp)

    def test0_pull_image(self):
        self.cleanup_cache()
        request = {'system': self.system, 'itype': self.itype, 'tag': self.tag}
        status = self.imageworker.pull_image(request)
        self.assertTrue(status)
        self.assertIn('meta', request)
        meta = request['meta']
        self.assertIn('id', meta)
        self.assertEquals(meta['entrypoint'][0], "/bin/sh")
        self.assertTrue(os.path.exists(request['expandedpath']))

        #fp = '%s/%s.meta' % (self.imageDir, request['id'])
        #print fp
        #self.assertTrue(os.path.exists(fp))
        return

    # Pull the image but explicitly specify dockerhub
    def test_pull_image_dockerhub(self):
        request = {
            'system': self.system,
            'itype': self.itype,
            'tag': 'index.docker.io/ubuntu:latest'
        }
        status = self.imageworker.pull_image(request)
        self.assertTrue(status)
        self.assertIn('meta', request)
        meta = request['meta']
        self.assertIn('id', meta)
        self.assertTrue(os.path.exists(request['expandedpath']))
        return

    # Use the URL format of the location, like an alias
    def test_pull_image_url(self):
        request = {
            'system': self.system,
            'itype': self.itype,
            'tag': 'urltest/ubuntu:latest'
        }
        status = self.imageworker.pull_image(request)
        self.assertTrue(status)
        self.assertIn('meta', request)
        meta = request['meta']
        self.assertIn('id', meta)
        self.assertTrue(os.path.exists(request['expandedpath']))
        return

    # Use the URL format of the location and pull a nested image
    # (e.g. with an org)
    def test_pull_image_url_org(self):
        self.cleanup_cache()
        request = {
            'system': self.system,
            'itype': self.itype,
            'tag': 'urltest/%s' % (self.tag)
        }
        status = self.imageworker.pull_image(request)
        self.assertTrue(status)
        self.assertIn('meta', request)
        meta = request['meta']
        self.assertIn('id', meta)
        self.assertTrue(os.path.exists(request['expandedpath']))
        return

    def test1_convert_image(self):
        request = {
            'system': self.system,
            'itype': self.itype,
            'tag': self.tag
        }
        status = self.imageworker.pull_image(request)
        # TODO: a little odd that is True and == True used here
        self.assertTrue(status)
        status = self.imageworker.convert_image(request)
        self.assertTrue(status)

    def test2_transfer_image(self):
        request = {
            'system': self.system,
            'itype': self.itype,
            'tag': self.tag,
            'imagefile': self.imagefile
        }
        with open(self.imagefile, 'w') as f:
            f.write('bogus')
        assert os.path.exists(self.imagefile)
        status = self.imageworker.transfer_image(request)
        self.assertTrue(status)

if __name__ == '__main__':
    unittest.main()
