import os
import imagegwapi
import unittest
import tempfile
import time
import urllib
import json
from pymongo import MongoClient

AUTH_HEADER='authentication'

class GWTestCase(unittest.TestCase):

    def setUp(self):
        self.configfile='test.json'
        with open(self.configfile) as config_file:
            self.config = json.load(config_file)
        os.environ['GWCONFIG']=self.configfile
        os.environ['CONFIG']=self.configfile
        mongouri=self.config['MongoDBURI']
        print "Debug: Connecting to %s"%mongouri
        client = MongoClient(mongouri)
        db=self.config['MongoDB']
        self.images=client[db].images
        self.images.drop()
        imagegwapi.imagegwapi.config['TESTING'] = True
        self.app = imagegwapi.imagegwapi.test_client()
        self.url="/api"
        self.system="test"
        self.type="docker"
        self.tag=urllib.quote("test:0.1")
        self.urlreq="%s/%s/%s"%(self.system,self.type,self.tag)
        # Need to switch to real munge tokens
        self.auth="good:1:1"
        self.auth_bad="bad:1:1"
        self.auth_header='authentication'


    #def tearDown(self):
    #  print "No teardown"

    def test_pull(self):
        uri='%s/pull/%s/'%(self.url,self.urlreq)
        rv = self.app.post(uri,headers={AUTH_HEADER:self.auth})
        assert rv.status_code==200
        #    assert rv.status_code==200
        #    assert rv.data.rfind(self.service)

    def test_lookup(self):
        # Do a pull so we can create an image record
        uri='%s/pull/%s/'%(self.url,self.urlreq)
        rv = self.app.post(uri,headers={AUTH_HEADER:self.auth})
        assert rv.status_code==200
        uri='%s/lookup/%s/'%(self.url,self.urlreq)
        rv = self.app.get(uri, headers={AUTH_HEADER:self.auth})
        assert rv.status_code==200
        uri='%s/lookup/%s/%s/'%(self.url,self.system,'bogus')
        rv = self.app.get(uri, headers={AUTH_HEADER:self.auth})
        assert rv.status_code==404
        #    assert rv.status_code==200
        #    assert rv.data.rfind(self.service)

    def test_expire(self):
        rv = self.app.get('%s/expire/%s/%s/%s/%s/'%(self.url,self.system,self.type,self.tag,self.id))
        assert rv.status_code==200
        #    assert rv.status_code==200
        #    assert rv.data.rfind(self.service)
    #def test_list(self):
    #    rv = self.app.get('/services/')
    #    assert rv.status_code==200
    #    assert rv.data.rfind(self.service)
#
#    def test_service(self):
#        rv = self.app.delete('/kill/'+self.service)
#        assert rv.status_code==200
#        rv = self.app.get('/services/'+self.service+'/')
#        assert rv.status_code==200
#
#    def test_delete_service(self):
#        rv = self.app.delete('/kill/'+self.service)
#        assert rv.status_code==200

if __name__ == '__main__':
    unittest.main()
