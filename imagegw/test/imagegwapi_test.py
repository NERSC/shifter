import os
import unittest
import tempfile
import time
import urllib
import json
from pymongo import MongoClient

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

AUTH_HEADER='authentication'

class GWTestCase(unittest.TestCase):

    def setUp(self):
        import imagegwapi
        self.configfile='test.json'
        with open(self.configfile) as config_file:
            self.config = json.load(config_file)
        os.environ['GWCONFIG']=self.configfile
        mongouri=self.config['MongoDBURI']
        print "Debug: Connecting to %s"%mongouri
        client = MongoClient(mongouri)
        db=self.config['MongoDB']
        if not os.path.exists(self.config['CacheDirectory']):
            os.mkdir(self.config['CacheDirectory'])
        p= self.config['Platforms']['systema']['ssh']['imageDir']
        if not os.path.exists(p):
            os.makedirs(p)
        self.images=client[db].images
        self.images.drop()
        imagegwapi.imagegwapi.config['TESTING'] = True
        imagegwapi.init(self.configfile)
        self.app = imagegwapi.imagegwapi.test_client()
        self.url="/api"
        self.system="systema"
        self.type="docker"
        self.tag=urllib.quote("ubuntu:14.04")
        self.urlreq="%s/%s/%s"%(self.system,self.type,self.tag)
        # Need to switch to real munge tokens
        self.auth="good:1:1"
        self.auth_bad="bad:1:1"
        self.auth_header='authentication'
        self.logfile='/tmp/worker.log'
        self.pid=0
        if os.path.exists(self.logfile):
            pass#os.unlink(self.logfile)
        self.start_worker()


    def tearDown(self):
        self.stop_worker()


    def start_worker(self,TESTMODE=1,system='systema'):
        # Start a celery worker.
        pid=os.fork()
        if pid==0:  # Child process
            os.environ['CONFIG']='test.json'
            os.environ['TESTMODE']='%d'%(TESTMODE)
            os.execvp('celery',['celery','-A','shifter_imagegw.imageworker',
                'worker','--quiet',
                '-Q','%s'%(system),
                '--loglevel=WARNING',
                '-c','2',
                '-f',self.logfile])
        else:
            self.pid=pid

    def stop_worker(self):
        print "Stopping worker"
        if self.pid>0:
            os.kill(self.pid,9)

    def time_wait(self,id,TIMEOUT=30):
        poll_interval=0.5
        count=TIMEOUT/poll_interval
        state='UNKNOWN'
        while (state!='READY' and count>0):
            print "DEBUG: id=%s state=%s"%(str(id),state)
            state=self.m.get_state(id)
            count-=1
            time.sleep(poll_interval)
        return state


    def test_pull(self):
        uri='%s/pull/%s/'%(self.url,self.urlreq)
        rv = self.app.post(uri,headers={AUTH_HEADER:self.auth})
        assert rv.status_code==200
        #    assert rv.status_code==200
        #    assert rv.data.rfind(self.service)

    def test_list(self):
        # Do a pull so we can create an image record
        uri='%s/pull/%s/'%(self.url,self.urlreq)
        rv = self.app.post(uri,headers={AUTH_HEADER:self.auth})
        assert rv.status_code==200
        uri='%s/list/%s/'%(self.url,self.system)
        rv = self.app.get(uri, headers={AUTH_HEADER:self.auth})
        assert rv.status_code==200
        #    assert rv.status_code==200
        #    assert rv.data.rfind(self.service)

    def test_lookup(self):
        # Do a pull so we can create an image record
        uri='%s/pull/%s/'%(self.url,self.urlreq)
        i=0
        while i<20:
            rv = self.app.post(uri,headers={AUTH_HEADER:self.auth})
            assert rv.status_code==200
            r=json.loads(rv.data)
            if r['status']=='READY':
                break
            if r['status']=='FAILURE':
                break
            print '  %s...'%(r['status'])
            time.sleep(1)
            i=i+1
        print ''
        uri='%s/lookup/%s/'%(self.url,self.urlreq)
        rv = self.app.get(uri, headers={AUTH_HEADER:self.auth})
        assert rv.status_code==200
        uri='%s/lookup/%s/%s/'%(self.url,self.system,'bogus')
        rv = self.app.get(uri, headers={AUTH_HEADER:self.auth})
        assert rv.status_code==404
        #    assert rv.status_code==200
        #    assert rv.data.rfind(self.service)

    def test_expire(self):
        uri='%s/expire/%s/%s/%s/%s/'%(self.url,self.system,self.type,self.tag,self.id)
        rv = self.app.get(uri, headers={AUTH_HEADER:self.auth})
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
