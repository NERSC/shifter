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
import shutil
DEBUG = False


def update_status(ident, state, meta=None):
    if DEBUG:
        print('id=%s state=%s' % (ident, state))


class ImageWorkerTestCase(unittest.TestCase):

    def setUp(self):
        cwd = os.path.dirname(os.path.realpath(__file__))
        os.environ['PATH'] = cwd + ':' + os.environ['PATH']
        from shifter_imagegw import imageworker
        self.imageworker = imageworker
        self.updater = imageworker.Updater('bogusid', update_status)
        self.configfile = 'test.json'
        with open(self.configfile) as config_file:
            self.config = json.load(config_file)

        self.system = 'systema'
        self.itype = 'docker'
        self.tag = 'registry.services.nersc.gov/nersc-py:latest'
        self.tag = 'ubuntu:latest'
        self.tag = 'scanon/shanetest:latest'
        self.request = {
                        'system': self.system,
                        'itype': self.itype,
                        'tag': self.tag
        }
        if 'LOCALREGISTRY' in os.environ:
            self.config['DefaultImageLocation'] = 'local'
            self.tag = 'local' + '/' + self.tag
        if not os.path.exists(self.config['CacheDirectory']):
            os.mkdir(self.config['CacheDirectory'])
        self.expandedpath = \
            os.path.join(self.config['CacheDirectory'],
                         '%s_%s' % (self.itype, self.tag.replace('/', '_')))
        idir = self.config['Platforms']['systema']['ssh']['imageDir']
        if not os.path.exists(idir):
            os.makedirs(idir)
        self.imageDir = idir

    def cleanup_cache(self):
        paths = [self.imageDir, self.config['CacheDirectory']]
        for path in paths:
            for f in os.listdir(path):
                if f.find('.squashfs') > 0 or f.find('.meta') > 0:
                    fn = os.path.join(path, f)
                    os.remove(fn)

    def get_metafile(self, id):
        metafile = os.path.join(self.imageDir, '%s.meta' % (id))
        return metafile

    def read_metafile(self, metafile):
        kv = {}
        with open(metafile) as mf:
            for line in mf:
                (k, v) = line.rstrip().split(': ')
                kv[k] = v
        # Convert ACLs to list of ints
        if 'USERACL' in kv:
            list = [int(x) for x in kv['USERACL'].split(',')]
            kv['USERACL'] = list
        if 'GROUPACL' in kv:
            list = [int(x) for x in kv['GROUPACL'].split(',')]
            kv['GROUPACL'] = list

        return kv

    def test_pull_image_basic(self):
        self.cleanup_cache()
        request = {'system': self.system, 'itype': self.itype, 'tag': self.tag}
        status = self.imageworker.pull_image(request, self.updater)
        self.assertTrue(status)
        self.assertIn('meta', request)
        meta = request['meta']
        self.assertIn('id', meta)
        self.assertIn('workdir', meta)
        self.assertEqual(meta['entrypoint'][0], "/bin/sh")
        self.assertTrue(os.path.exists(request['expandedpath']))

        return

    # Pull the image but explicitly specify dockerhub
    def test_pull_image_dockerhub(self):
        request = {
            'system': self.system,
            'itype': self.itype,
            'tag': 'index.docker.io/ubuntu:latest'
        }
        status = self.imageworker.pull_image(request, self.updater)
        self.assertTrue(status)
        self.assertIn('meta', request)
        meta = request['meta']
        self.assertIn('id', meta)
        self.assertTrue(os.path.exists(request['expandedpath']))
        return

    # Use the URL format of the location, like an alias
    def test_pull_image_url(self):
        self.cleanup_cache()
        request = {
            'system': self.system,
            'itype': self.itype,
            'tag': 'urltest/ubuntu:latest'
        }
        status = self.imageworker.pull_image(request, self.updater)
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
        status = self.imageworker.pull_image(request, self.updater)
        self.assertTrue(status)
        self.assertIn('meta', request)
        meta = request['meta']
        self.assertIn('id', meta)
        self.assertTrue(os.path.exists(request['expandedpath']))
        return

    def test_convert_image(self):
        # Create a bogus tree
        self.cleanup_cache()
        base = self.imageDir + '/image'
        if os.path.exists(base):
            shutil.rmtree(base)
        os.makedirs('%s/%s' % (base, 'a/b/c'))
        request = self.request
        request['id'] = 'bogus'
        request['expandedpath'] = base
        status = self.imageworker.convert_image(request)
        self.assertTrue(status)
        # Cleanup
        if os.path.exists(base):
            shutil.rmtree(base)

    def test_transfer_image(self):
        hash = 'bogus'
        imagefile = os.path.join(self.config['ExpandDirectory'],
                                 '%s.%s' % (hash, 'squashfs'))
        request = self.request
        request['imagefile'] = imagefile
        with open(imagefile, 'w') as f:
            f.write('bogus')
        self.assertTrue(os.path.exists(imagefile))
        status = self.imageworker.transfer_image(request)
        self.assertTrue(status)

    def test_bad_pull_docker(self):
        self.cleanup_cache()
        request = self.request
        self.imageworker.CONFIG['Platforms']['systema']['use_external'] = True
        os.environ['UMOCI_FAIL'] = '1'
        with self.assertRaises(OSError):
            self.imageworker._pull_dockerv2(request, 'index.docker.io',
                                               'scanon/shanetest', 'latest',
                                               self.updater)
        self.imageworker.CONFIG['Platforms']['systema']['use_external'] = False
        os.environ.pop('UMOCI_FAIL')

    def test_pull_docker(self):
        self.cleanup_cache()
        request = self.request
        resp = self.imageworker._pull_dockerv2(request, 'index.docker.io',
                                               'scanon/shanetest', 'latest',
                                               self.updater)
        self.assertTrue(resp)

    def test_pull_docker_unicode(self):
        request = {
            'system': self.system,
            'itype': self.itype,
            'tag': 'index.docker.io/scanon/unicode:latest'
        }
        status = self.imageworker.pull_image(request, self.updater)
        self.assertTrue(status)
        self.assertIn('meta', request)
        meta = request['meta']
        self.assertIn('id', meta)
        self.assertTrue(os.path.exists(request['expandedpath']))
        tfile = os.path.join(request['expandedpath'], '\ua000')
        self.assertTrue(os.path.exists(tfile))
        return

    def test_pull_image(self):
        request = self.request
        resp = self.imageworker.pull_image(request, self.updater)
        self.assertTrue(resp)

    def test_puller_testmode(self):
        request = self.request
        result = self.imageworker.pull(request, self.updater, testmode=1)
        self.assertIn('workdir', result)
        self.assertIn('env', result)
        self.assertTrue(result)
        with self.assertRaises(OSError):
            self.imageworker.pull(request, self.updater, testmode=2)

    def test_puller_real(self):
        request = self.request
        result = self.imageworker.pull(request, self.updater)
        mf = self.get_metafile(result['id'])
        mfdata = self.read_metafile(mf)
        self.assertIn('WORKDIR', mfdata)
        request['userACL'] = [1001]
        result = self.imageworker.pull(request, self.updater)
        self.imageworker.remove_image(request, self.updater)

    def test_examine(self):
        self.imageworker.CONFIG['examiner'] = 'exam.sh'
        base = self.imageDir + '/image'
        request = {'id': 'bogus',
                   'expandedpath': base}
        result = self.imageworker.examine_image(request)
        self.assertTrue(result)
        request['id'] = 'bad'
        result = self.imageworker.examine_image(request)
        self.assertFalse(result)
        self.imageworker.CONFIG.pop('examiner')


if __name__ == '__main__':
    unittest.main()
