import os
os.environ['CONFIG']='test.json'
import imagemngr
import unittest
import tempfile
import time
import json
import sys
from pymongo import MongoClient
from subprocess import call

class ImageMngrTestCase(unittest.TestCase):

    def setUp(self):
        self.configfile='test.json'
        with open(self.configfile) as config_file:
            self.config = json.load(config_file)
        mongodb=self.config['MongoDBURI']
        client = MongoClient(mongodb)
        db=self.config['MongoDB']
        self.images=client[db].images
        self.images.drop()
        self.m=imagemngr.imagemngr(self.configfile)
        self.system='systema'
        self.itype='docker'
        self.tag='test'
        self.id='fakeid'
        self.auth='good'
        self.badauth='bad'
        self.logfile='/tmp/worker.log'
        self.pid=0
        self.query={'system':self.system,'itype':self.itype,'tag':self.tag}
        if os.path.exists(self.logfile):
            os.unlink(self.logfile)
        # Cleanup Mongo
        if self.images.find_one(self.query):
            self.images.remove(self.query)



    #def tearDown(self):
    #  print "No teardown"

    def test_lookup(self):
        record={'system':self.system,
            'itype':self.itype,
            'id':self.id,
            'tag':self.tag,
            'status':'READY',
            'userACL':[],
            'groupACL':[],
            'ENV':[],
            'ENTRY':'',
            }
        # Create a fake record in mongo
        self.images.insert(record)
        i=self.query.copy()
        l=self.m.lookup(self.auth,i)
        assert 'status' in l
        assert '_id' in l
        assert self.m.get_state(l['_id'])=='READY'
        i=self.query.copy()
        i['tag']='bogus'
        l=self.m.lookup(self.auth,i)
        assert l==None
        # TODO: Test bad auth
        l=self.m.lookup(self.badauth,i)

    def start_worker(self,TESTMODE=1,system='systema'):
        # Start a celery worker.
        pid=os.fork()
        if pid==0:  # Child process
            os.environ['CONFIG']='test.json'
            os.environ['TESTMODE']='%d'%(TESTMODE)
            os.execvp('celery',['celery','-A','imageworker',
                'worker','--quiet',
                '-Q','%s'%(system),
                '--loglevel=WARNING',
                '-c','1',
                '-f',self.logfile])
        else:
            self.pid=pid

    def stop_worker(self):
        if self.pid>0:
            os.kill(self.pid,9)

    def test_pull(self):

        # Use defaults for format, arch, os, ostcount, replication
        pr={
            'system':self.system,
            'itype':self.itype,
            'tag':self.tag,
			'remotetype':'dockerv2',
			'userAcl':[],
			'groupAcl':[]
        }
        # Do the pull
        print "DEBUG: Starting worker"
        self.start_worker()
        id=self.m.pull(self.auth,pr,TESTMODE=1)#,delay=False)
        assert id!=None
        # Confirm record
        q=self.query.copy()
        mrec=self.images.find_one(q)
        assert '_id' in mrec
        # Track through transistions
        TIMEOUT=0
        state='UNKNOWN'
        while (state!='READY' and TIMEOUT<20):
            print "DEBUG: %s"%state
            state=self.m.get_state(id)
            TIMEOUT+=1
            time.sleep(0.5)
        #Debug
        assert state=='READY'
        # Cause a failure
        self.images.drop()
        id=self.m.pull(self.auth,pr,TESTMODE=2)
        assert id!=None
        time.sleep(1)
        state=self.m.get_state(id)
        print "DEBUG: %s"%state
        assert state=='FAILURE'
        #for r in self.images.find():
        #    print r
        self.stop_worker()

    def test_expire(self):
        pass
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
