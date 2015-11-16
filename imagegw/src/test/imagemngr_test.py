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
        self.tag2='test2'
        self.auth='good:1:1'
        self.badauth='bad:1:1'
        self.logfile='/tmp/worker.log'
        self.pid=0
        self.query={'system':self.system,'itype':self.itype,'tag':self.tag}
        if os.path.exists(self.logfile):
            os.unlink(self.logfile)
        # Cleanup Mongo
        if self.images.find_one(self.query):
            self.images.remove(self.query)



    def tearDown(self):
        self.stop_worker()

    def test_session(self):
        s=self.m.new_session(self.auth,self.system)
        assert s is not None
        try:
            s=self.m.new_session(self.badauth,self.system)
        except:
            pass

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
        session=self.m.new_session(self.auth,self.system)
        l=self.m.lookup(session,i)
        assert 'status' in l
        assert '_id' in l
        assert self.m.get_state(l['_id'])=='READY'
        i=self.query.copy()
        i['tag']='bogus'
        l=self.m.lookup(session,i)
        assert l==None

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
                '-c','2',
                '-f',self.logfile])
        else:
            self.pid=pid

    def stop_worker(self):
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


    def test_0pull(self):

        # Use defaults for format, arch, os, ostcount, replication
        pr={
            'system':self.system,
            'itype':self.itype,
            'tag':'scanon/shanetest:latest',
			'remotetype':'dockerv2',
			'userAcl':[],
			'groupAcl':[]
        }
        # Do the pull
        self.start_worker()
        session=self.m.new_session(self.auth,self.system)
        id=self.m.pull(session,pr,TESTMODE=1)#,delay=False)
        assert id is not None
        # Confirm record
        q={'system':self.system,'itype':self.itype,'tag':'scanon/shanetest:latest'}
        mrec=self.images.find_one(q)
        assert '_id' in mrec
        # Track through transistions
        state=self.time_wait(id)
        # TIMEOUT=0
        # state='UNKNOWN'
        # while (state!='READY' and TIMEOUT<30):
        #     print "DEBUG: %s"%state
        #     state=self.m.get_state(id)
        #     TIMEOUT+=1
        #     time.sleep(0.5)
        #Debug
        assert state=='READY'
        imagerec=self.m.lookup(session,pr)
        print 'imagerec=%s'%(str(imagerec))
        assert 'ENTRY' in imagerec
        assert 'ENV' in imagerec
        self.stop_worker()


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
        session=self.m.new_session(self.auth,self.system)
        id=self.m.pull(session,pr,TESTMODE=1)#,delay=False)
        assert id is not None
        # Confirm record
        q=self.query.copy()
        mrec=self.images.find_one(q)
        assert '_id' in mrec
        # Track through transistions
        state=self.time_wait(id)
        #Debug
        assert state=='READY'
        imagerec=self.m.lookup(session,pr)
        #print 'imagerec=%s'%(str(imagerec))
        assert 'ENTRY' in imagerec
        assert 'ENV' in imagerec
        # Cause a failure
        self.images.drop()
        id=self.m.pull(session,pr,TESTMODE=2)
        time.sleep(1)
        state=self.m.get_state(id)
        print "DEBUG: %s"%state
        assert state=='FAILURE'
        #for r in self.images.find():
        #    print r
        self.stop_worker()

    def test_pull2(self):

        # Use defaults for format, arch, os, ostcount, replication
        pr1={
            'system':self.system,
            'itype':self.itype,
            'tag':self.tag,
			'remotetype':'dockerv2',
			'userAcl':[],
			'groupAcl':[]
        }
        pr2={
            'system':self.system,
            'itype':self.itype,
            'tag':self.tag2,
			'remotetype':'dockerv2',
			'userAcl':[],
			'groupAcl':[]
        }
        # Do the pull
        print "DEBUG: Starting worker"
        self.start_worker()
        session=self.m.new_session(self.auth,self.system)
        id1=self.m.pull(session,pr1,TESTMODE=1)#,delay=False)
        id2=self.m.pull(session,pr2,TESTMODE=1)#,delay=False)
        assert id1!=None
        # Confirm record
        q=self.query.copy()
        mrec=self.images.find_one(q)
        print mrec
        assert '_id' in mrec
        state=self.time_wait(id1)
        assert state=='READY'
        state=self.time_wait(id2)
        assert state=='READY'
        # Cause a failure
        self.images.drop()


    def test_expire(self):
        pass
        #    assert rv.status_code==200
        #    assert rv.data.rfind(self.service)

if __name__ == '__main__':
    unittest.main()
