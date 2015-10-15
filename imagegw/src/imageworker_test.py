import os
os.environ['CONFIG']='test.json'
import imageworker
import unittest
import tempfile
import time
import json
import sys
from pymongo import MongoClient
from subprocess import call

class ImageWorkerTestCase(unittest.TestCase):

    def setUp(self):
        self.configfile='test.json'
        with open(self.configfile) as config_file:
            self.config = json.load(config_file)
        self.system='systema'
        self.itype='docker'
        self.tag='registry.services.nersc.gov/nersc-py:latest'
        self.tag='ubuntu:latest'
        if not os.path.exists(self.config['CacheDirectory']):
            os.mkdir(self.config['CacheDirectory'])
        self.expandedpath=os.path.join(self.config['CacheDirectory'],
            '%s_%s'%(self.itype,self.tag.replace('/','_')))
        self.imagefile=os.path.join(self.config['CacheDirectory'],
            '%s_%s.%s'%(self.itype,self.tag.replace('/','_'),'squashfs'))



    #def tearDown(self):
    #  print "No teardown"


    def test0_pull_image(self):
        request={'system':self.system,'itype':self.itype,'tag':self.tag}
        status=imageworker.pull_image(request)
        print status
        assert status==True
        assert os.path.exists(request['expandedpath'])

        return

    def test1_convert_image(self):
        request={'system':self.system,'itype':self.itype,'tag':self.tag}
        status=imageworker.pull_image(request)
        assert status==True
        status=imageworker.convert_image(request)
        assert status==True
        return

    def test2_transfer_image(self):
        request={'system':self.system,'itype':self.itype,'tag':self.tag,'imagefile':self.imagefile}
        assert os.path.exists(self.imagefile)
        status=imageworker.transfer_image(request)
        assert status==True
        return


if __name__ == '__main__':
    unittest.main()
