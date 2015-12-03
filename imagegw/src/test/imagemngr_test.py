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

    def test_00add_remove_tag(self):
        record={'system':self.system,
            'itype':self.itype,
            'id':self.id,
            'tag':[self.tag],
            'status':'READY',
            'userACL':[],
            'groupACL':[],
            'ENV':[],
            'ENTRY':'',
            }
        # Create a fake record in mongo
        id=self.images.insert(record)

        session=self.m.new_session(self.auth,self.system)
        assert id is not None
        before=self.m.lookup(session,self.query.copy())
        assert before is not None
        status=self.m.add_tag(id,'testtag')
        assert status is True
        print self.query
        after=self.m.lookup(session,self.query.copy())
        assert after is not None
        print 'rec %s'%(str(after))
        assert after['tag'].index('testtag')>=0
        assert after['tag'].index(self.tag)>=0
        status=self.m.remove_tag('testtag')
        assert status is True
        i=self.query.copy()
        l=self.m.lookup(session,i)
        try:
            index=l['tag'].index('testtag')
            assert index<0
        except:
            pass

    # Test if tag isn't a list
    def test_0add_remove_tagitem(self):
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
        id=self.images.insert(record)

        session=self.m.new_session(self.auth,self.system)
        i=self.query.copy()
        status=self.m.add_tag(id,'testtag')
        assert status is True
        rec=self.m.lookup(session,i)
        assert rec is not None
        assert rec['tag'].index(self.tag)>=0
        assert rec['tag'].index('testtag')>=0

    # Test if tag isn't a list
    def test_000add_remove_withtag(self):
        record={'system':self.system,
            'itype':self.itype,
            'id':self.id,
            'tag':[self.tag],
            'status':'READY',
            'userACL':[],
            'groupACL':[],
            'ENV':[],
            'ENTRY':'',
            }
        # Create a fake record in mongo
        id=self.images.insert(record)

        session=self.m.new_session(self.auth,self.system)
        i=self.query.copy()
        status=self.m.add_tag(id,'testtag')
        assert status is True
        rec=self.m.lookup(session,i)
        assert rec is not None
        assert rec['tag'].index(self.tag)>=0
        assert rec['tag'].index('testtag')>=0


    def test_0pullable(self):
        # An old READY image
        rec={'last_pull':0,'status':'READY'}
        assert self.m.pullable(rec) is True
        rec={'last_pull':time.time(),'status':'READY'}
        # A recent READY image
        assert self.m.pullable(rec) is False
        # A failed image
        rec={'last_pull':0,'status':'FAILURE'}
        assert self.m.pullable(rec) is True
        # recent pull
        rec={'last_pull':time.time(),'status':'FAILURE'}
        assert self.m.pullable(rec) is False

        # A hung pull
        rec={'last_pull':0,'status':'PULLING'}
        assert self.m.pullable(rec) is True
        # recent pull
        rec={'last_pull':time.time(),'status':'PULLING'}
        assert self.m.pullable(rec) is False



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

    def test_list(self):
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
        session=self.m.new_session(self.auth,self.system)
        li=self.m.list(session,self.system)
        assert len(li)>0
        l=li[0]
        assert '_id' in l
        assert self.m.get_state(l['_id'])=='READY'

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

    def test_repull(self):
        # Test a repull
        record={'system':self.system,
            'itype':self.itype,
            'id':self.id,
            'tag':[self.tag],
            'status':'READY',
            'userACL':[],
            'groupACL':[],
            'ENV':[],
            'ENTRY':'',
            'last_pull':time.time()
            }
        # Create a fake record in mongo
        id=self.images.insert(record)
        assert id is not None

        pr={
            'system':self.system,
            'itype':self.itype,
            'tag':self.tag,
			'remotetype':'dockerv2',
			'userAcl':[],
			'groupAcl':[]
        }

        session=self.m.new_session(self.auth,self.system)
        pull=self.m.pull(session,pr)
        assert pull is not None
        assert pull['status']=='READY'



    def test_pull(self):

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
        rec=self.m.pull(session,pr,TESTMODE=1)#,delay=False)
        assert rec is not None
        assert '_id' in rec
        id=rec['_id']
        # Re-pull the same thing.  Should give the same record
        rec=self.m.pull(session,pr,TESTMODE=1)#,delay=False)
        assert rec is not None
        assert '_id' in rec
        id2=rec['_id']
        assert id==id2
        q={'system':self.system,'itype':self.itype,'pulltag':{'$in':['scanon/shanetest:latest']}}
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
        #print 'imagerec=%s'%(str(imagerec))
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
        #print "DEBUG: Starting worker"
        self.start_worker()
        session=self.m.new_session(self.auth,self.system)
        rec=self.m.pull(session,pr,TESTMODE=1)#,delay=False)
        assert rec is not None
        id=rec['_id']
        # Confirm record
        q={'system':self.system,'itype':self.itype,'pulltag':self.tag}
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
        rec=self.m.pull(session,pr,TESTMODE=2)
        time.sleep(1)
        assert rec is not None
        id=rec['_id']
        state=self.m.get_state(id)
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
        rec1=self.m.pull(session,pr1,TESTMODE=1)#,delay=False)
        rec2=self.m.pull(session,pr2,TESTMODE=1)#,delay=False)
        assert rec1 is not None
        id1=rec1['_id']
        assert rec2 is not None
        id2=rec2['_id']
        # Confirm record
        q={'system':self.system,'itype':self.itype,'pulltag':self.tag}
        mrec=self.images.find_one(q)
        print mrec
        assert '_id' in mrec
        state=self.time_wait(id1)
        assert state=='READY'
        state=self.time_wait(id2)
        assert state=='READY'
        # Cause a failure
        mrec=self.images.find_one(q)
        self.images.drop()


    def test_expire(self):
        pass
        #    assert rv.status_code==200
        #    assert rv.data.rfind(self.service)

if __name__ == '__main__':
    unittest.main()
