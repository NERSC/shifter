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
from copy import deepcopy
from shifter_imagegw import imageworker
DEBUG = False


def update_status(ident, state, meta=None):
    if DEBUG:
        print('id=%s state=%s' % (ident, state))


class ImageWorkerTestCase(unittest.TestCase):

    def setUp(self):
        cwd = os.path.dirname(os.path.realpath(__file__))
        os.environ['PATH'] = cwd + ':' + os.environ['PATH']
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
        req = imageworker.ImageRequest(self.config, request, self.updater)
        status = req._pull_image()
        self.assertTrue(status)
        self.assertTrue(req.meta)
        self.assertIn('id', req.meta)
        self.assertIn('workdir', req.meta)
        self.assertEqual(req.meta['entrypoint'][0], "/bin/sh")
        self.assertTrue(os.path.exists(req.expandedpath))

        return

    # Pull the image but explicitly specify dockerhub
    def test_pull_image_dockerhub(self):
        request = {
            'system': self.system,
            'itype': self.itype,
            'tag': 'index.docker.io/ubuntu:latest'
        }
        req = imageworker.ImageRequest(self.config, request, self.updater)
        status = req._pull_image()
        self.assertTrue(status)
        self.assertTrue(req.meta)
        self.assertIn('id', req.meta)
        self.assertTrue(os.path.exists(req.expandedpath))
        return

    # Use the URL format of the location, like an alias
    def test_pull_image_url(self):
        self.cleanup_cache()
        request = {
            'system': self.system,
            'itype': self.itype,
            'tag': 'urltest/ubuntu:latest'
        }
        req = imageworker.ImageRequest(self.config, request, self.updater)
        status = req._pull_image()
        self.assertTrue(status)
        self.assertTrue(req.meta)
        self.assertIn('id', req.meta)
        self.assertTrue(os.path.exists(req.expandedpath))
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
        req = imageworker.ImageRequest(self.config, request, self.updater)
        status = req._pull_image()
        self.assertTrue(status)
        self.assertTrue(req.meta)
        self.assertIn('id', req.meta)
        self.assertTrue(os.path.exists(req.expandedpath))
        return

    def test_convert_image(self):
        # Create a bogus tree
        self.cleanup_cache()
        base = self.imageDir + '/image'
        if os.path.exists(base):
            shutil.rmtree(base)
        os.makedirs('%s/%s' % (base, 'a/b/c'))
        request = self.request
        req = imageworker.ImageRequest(self.config, request, self.updater)
        req.id = 'bogus'
        req.expandedpath = base
        status = req._convert_image()
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
        req = imageworker.ImageRequest(self.config, request, self.updater)
        status = req._transfer_image()
        self.assertTrue(status)

    def test_bad_pull_docker(self):
        self.cleanup_cache()
        request = self.request
        conf = deepcopy(self.config)
        conf['Platforms']['systema']['use_external'] = True
        os.environ['UMOCI_FAIL'] = '1'
        req = imageworker.ImageRequest(conf, request, self.updater)
        with self.assertRaises(OSError):
            req._pull_dockerv2('index.docker.io',
                               'scanon/shanetest', 'latest')
        os.environ.pop('UMOCI_FAIL')

    def test_pull_docker(self):
        self.cleanup_cache()
        request = self.request
        req = imageworker.ImageRequest(self.config, request, self.updater)
        resp = req._pull_dockerv2('index.docker.io',
                                  'scanon/shanetest', 'latest')
        self.assertTrue(resp)

    def test_pull_docker_unicode(self):
        request = {
            'system': self.system,
            'itype': self.itype,
            'tag': 'index.docker.io/scanon/unicode:latest'
        }
        req = imageworker.ImageRequest(self.config, request, self.updater)
        status = req._pull_image()
        self.assertTrue(status)
        self.assertTrue(req.meta)
        self.assertIn('id', req.meta)
        self.assertTrue(os.path.exists(req.expandedpath))
        tfile = os.path.join(req.expandedpath, '\ua000')
        self.assertTrue(os.path.exists(tfile))
        return

    def test_pull_image(self):
        request = self.request
        req = imageworker.ImageRequest(self.config, request, self.updater)
        resp = req._pull_image()
        self.assertTrue(resp)

    def test_puller_real(self):
        request = self.request
        req = imageworker.ImageRequest(self.config, request, self.updater)
        result = req.pull()
        mf = self.get_metafile(result['id'])
        mfdata = self.read_metafile(mf)
        self.assertIn('WORKDIR', mfdata)
        request['userACL'] = [1001]
        req = imageworker.ImageRequest(self.config, request, self.updater)
        result = req.pull()
        req.remove_image()

    def test_examine(self):
        conf = deepcopy(self.config)
        conf['examiner'] = 'exam.sh'
        base = self.imageDir + '/image'
        request = {
                   'system': self.system,
                   }
        req = imageworker.ImageRequest(conf, request, self.updater)
        req.id = 'bogus'
        req.expandedpath = base
        result = req._examine_image()
        self.assertTrue(result)
        req = imageworker.ImageRequest(conf, request, self.updater)
        req.id = 'bad'
        req.expandedpath = base
        result = req._examine_image()
        self.assertFalse(result)

    def test_labels(self):
        self.cleanup_cache()
        request = self.request
        conf = deepcopy(self.config)
        conf['Platforms']['systema']['use_external'] = True
        req = imageworker.ImageRequest(conf, request, self.updater)
        resp = req.pull()
        self.assertIn('labels', resp)
        self.assertIn('alabel', resp['labels'])


if __name__ == '__main__':
    unittest.main()
