import os
os.environ['GWCONFIG']='test.json'
from shifter_imagegw import imageworker
import unittest
import tempfile
import time
import json
import sys
from pymongo import MongoClient
from subprocess import call

"""
Shifter, Copyright (c) 2015, The Regents of the University of California,
through Lawrence Berkeley National Laboratory (subject to receipt of any
required approvals from the U.S. Dept. of Energy).  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
 3. Neither the name of the University of California, Lawrence Berkeley
    National Laboratory, U.S. Dept. of Energy nor the names of its
    contributors may be used to endorse or promote products derived from this
    software without specific prior written permission.`

See LICENSE for full text.
"""

class ImageWorkerTestCase(unittest.TestCase):

    def setUp(self):
        cwd=os.path.dirname(os.path.realpath(__file__))
        os.environ['PATH']=cwd+':'+os.environ['PATH']
        self.configfile='test.json'
        with open(self.configfile) as config_file:
            self.config = json.load(config_file)
        self.system='systema'
        self.itype='docker'
        self.tag='registry.services.nersc.gov/nersc-py:latest'
        self.tag='ubuntu:latest'
        self.tag='scanon/shanetest:latest'
        self.hash='b3491cdefcdb79a31ab7ddf1bbcf7c8eeff9b4f00cb83a0be513fb800623f9cf'
        if not os.path.exists(self.config['CacheDirectory']):
            os.mkdir(self.config['CacheDirectory'])
        self.expandedpath=os.path.join(self.config['CacheDirectory'],
            '%s_%s'%(self.itype,self.tag.replace('/','_')))
        self.imagefile=os.path.join(self.config['ExpandDirectory'],
            '%s.%s'%(self.hash,'squashfs'))
        print "imagefile", self.imagefile
        idir=self.config['Platforms']['systema']['ssh']['imageDir']
        if not os.path.exists(idir):
            os.makedirs(idir)



    #def tearDown(self):
    #  print "No teardown"


    def test0_pull_image(self):
        request={'system':self.system,'itype':self.itype,'tag':self.tag}
        status=imageworker.pull_image(request)
        print status
        assert status is True
        assert 'meta' in request
        print request
        meta=request['meta']
        assert 'id' in meta
        assert meta['entrypoint'][0]=="/bin/sh"
        assert os.path.exists(request['expandedpath'])

        return

    def test1_convert_image(self):
        request={'system':self.system,'itype':self.itype,'tag':self.tag}
        status=imageworker.pull_image(request)
        assert status is True
        status=imageworker.convert_image(request)
        assert status==True
        return

    def test2_transfer_image(self):
        print self.imagefile
        request={'system':self.system,'itype':self.itype,'tag':self.tag,'imagefile':self.imagefile}
        assert os.path.exists(self.imagefile)
        status=imageworker.transfer_image(request)
        assert status==True
        return


if __name__ == '__main__':
    unittest.main()
