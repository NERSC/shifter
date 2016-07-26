import os
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
        from shifter_imagegw import imagemngr
        self.configfile='test.json'
        with open(self.configfile) as config_file:
            self.config = json.load(config_file)
        mongodb=self.config['MongoDBURI']
        client = MongoClient(mongodb)
        db=self.config['MongoDB']
        self.images=client[db].images
        self.images.drop()
        self.m=imagemngr.imagemngr(self.config)
        self.system='systema'
        self.itype='docker'
        self.tag='test'
        self.id='fakeid'
        self.tag2='test2'
        self.format='squashfs'
        self.auth='good:1:1'
        self.authadmin='good:0:0'
        self.badauth='bad:1:1'
        self.logfile='/tmp/worker.log'
        self.pid=0
        self.query={'system':self.system,'itype':self.itype,'tag':self.tag}
        if os.path.exists(self.logfile):
            pass#os.unlink(self.logfile)
        # Cleanup Mongo
        if self.images.find_one(self.query):
            self.images.remove(self.query)

    def tearDown(self):
        """
        tear down should stop the worker
        """
        self.stop_worker()

    def start_worker(self,TESTMODE=1,system='systema'):
        # Start a celery worker.
        pid=os.fork()
        if pid==0:  # Child process
            os.environ['GWCONFIG']='test.json'
            #os.environ['TESTMODE']='%d'%(TESTMODE)
            os.execvp('celery',['celery','-A','shifter_imagegw.imageworker',
                'worker','--quiet',
                '-Q','%s'%(system),
                '--loglevel=INFO',
                '-c','2',
                '-f',self.logfile])
        else:
            time.sleep(1)
            self.pid=pid

    def stop_worker(self):
        if self.pid>0:
            try:
                os.kill(self.pid,2)
                self.pid=0
            except:
                pass

    def time_wait(self,id,wstate='READY',TIMEOUT=30):
        poll_interval=0.5
        count=TIMEOUT/poll_interval
        state='UNKNOWN'
        while (state!=wstate and count>0):
            state=self.m.get_state(id)
            count-=1
            time.sleep(poll_interval)
        return state

    def create_fakeimage(self,system,id,format):
        if self.config['Platforms'][system]['accesstype']=='remote':
          idir=self.config['Platforms'][system]['ssh']['imageDir']
        else:
          idir=self.config['Platforms'][system]['local']['imageDir']

        if os.path.exists(idir) is False:
          os.makedirs(idir)
        file = os.path.join(idir, '%s.%s'%(id,format))
        with open(file,'w') as f:
            f.write('')
        metafile = os.path.join(idir, '%s.meta'%(id))
        with open(metafile,'w') as f:
            f.write('')
        return file,metafile

    def good_pullrecord(self):
        return {'system':self.system,
                    'itype':self.itype,
                    'id':self.id,
                    'pulltag':self.tag,
                    'status':'READY',
                    'userACL':[],
                    'groupACL':[],
                    'ENV':[],
                    'ENTRY':'',
                    'last_pull':time.time()
                    }


    def good_record(self):
        return {'system':self.system,
            'itype':self.itype,
            'id':self.id,
            'tag':[self.tag],
            'format':self.format,
            'status':'READY',
            'userACL':[],
            'groupACL':[],
            'last_pull':time.time(),
            'ENV':[],
            'ENTRY':'',
            }

#
#  Tests
#

    def test_session(self):
        s=self.m.new_session(self.auth,self.system)
        assert s is not None
        try:
            s=self.m.new_session(self.badauth,self.system)
        except:
            pass

    def test_noadmin(self):
        s=self.m.new_session(self.auth,self.system)
        assert s is not None
        resp=self.m.isadmin(s,self.system)
        assert resp is False

    def test_admin(self):
        s=self.m.new_session(self.authadmin,self.system)
        assert s is not None
        resp=self.m.isadmin(s,self.system)
        assert resp is True


# Create an image and tag with a new tag.
# Make sure both tags show up.
# Remove the tag and make sure the original tags
# doesn't also get removed.
    def test_0add_remove_tag(self):
        record=self.good_record()
        # Create a fake record in mongo
        id=self.images.insert(record)

        assert id is not None
        before=self.images.find_one({'_id':id})
        assert before is not None
        # Add a tag a make sure it worked
        status=self.m.add_tag(id,self.system,'testtag')
        assert status is True
        after=self.images.find_one({'_id':id})
        assert after is not None
        assert after['tag'].count('testtag')==1
        assert after['tag'].count(self.tag)==1
        # Remove a tag and make sure it worked
        status=self.m.remove_tag(self.system,'testtag')
        assert status is True
        after=self.images.find_one({'_id':id})
        assert after is not None
        assert after['tag'].count('testtag')==0

    # Similar to above but just test the adding part
    def test_0add_remove_tagitem(self):
        record=self.good_record()
        record['tag']=self.tag
        # Create a fake record in mongo
        id=self.images.insert(record)

        status=self.m.add_tag(id,self.system,'testtag')
        assert status is True
        rec=self.images.find_one({'_id':id})
        assert rec is not None
        assert rec['tag'].count(self.tag)==1
        assert rec['tag'].count('testtag')==1

    # Same as above but use the lookup instead of a directory
    # direct mongo lookup
    def test_0add_remove_withtag(self):
        record=self.good_record()
        # Create a fake record in mongo
        id=self.images.insert(record)

        session=self.m.new_session(self.auth,self.system)
        i=self.query.copy()
        status=self.m.add_tag(id,self.system,'testtag')
        assert status is True
        rec=self.m.lookup(session,i)
        assert rec is not None
        assert rec['tag'].count(self.tag)==1
        assert rec['tag'].count('testtag')==1

    # Test if tag isn't a list
    def test_0add_remove_two(self):
        record=self.good_record()
        # Create a fake record in mongo
        id1=self.images.insert(record.copy())
        record['id']='fakeid2'
        record['tag']=[]
        id2=self.images.insert(record.copy())


        session=self.m.new_session(self.auth,self.system)
        i=self.query.copy()
        status=self.m.add_tag(id2,self.system,self.tag)
        assert status is True
        rec1=self.images.find_one({'_id':id1})
        rec2=self.images.find_one({'_id':id2})
        assert rec1['tag'].count(self.tag)==0
        assert rec2['tag'].count(self.tag)==1

    # Similar to above but just test the adding part
    def test_0add_same_image_two_system(self):
        record=self.good_record()
        record['tag']=self.tag
        # Create a fake record in mongo
        id1=self.images.insert(record.copy())
        # add testtag for systema
        status=self.m.add_tag(id1,self.system,'testtag')
        assert status is True
        record['system']='systemb'
        id2=self.images.insert(record.copy())
        status=self.m.add_tag(id2,'systemb','testtag')
        assert status is True
        # Now make sure testtag for first system is still
        # present
        rec=self.images.find_one({'_id':id1})
        assert rec is not None
        assert rec['tag'].count('testtag')==1

    def test_0isasystem(self):
        assert self.m.isasystem(self.system) is True
        assert self.m.isasystem('bogus') is False

    def test_0resetexp(self):
        record={'system':self.system,
            'itype':self.itype,
            'id':self.id,
            'pulltag':self.tag,
            'status':'READY',
            'userACL':[],
            'groupACL':[],
            'ENV':[],
            'ENTRY':'',
            'last_pull':0
            }
        id=self.images.insert(record.copy())
        assert id is not None
        expire=self.m.resetexpire(id)
        assert expire>time.time()
        rec=self.images.find_one({'_id':id})
        assert rec['expiration']==expire

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
        rec={'last_pull':0,'last_heartbeat':time.time()-7200,'status':'PULLING'}
        assert self.m.pullable(rec) is True
        # recent pull
        rec={'last_pull':time.time(),'status':'PULLING'}
        assert self.m.pullable(rec) is False

        # A hung pull
        rec={'last_pull':0,'last_heartbeat':time.time(),'status':'PULLING'}
        assert self.m.pullable(rec) is False

    def test_0complete_pull(self):
        # Test complete_pull
        record={'system':self.system,
            'itype':self.itype,
            'id':self.id,
            'pulltag':self.tag,
            'status':'READY',
            'userACL':[],
            'groupACL':[],
            'ENV':[],
            'ENTRY':'',
            'last_pull':0
            }
        # Create a fake record in mongo
        # First test when there is no existing image
        id=self.images.insert(record.copy())
        assert id is not None
        resp={'id':id,'tag':self.tag}
        self.m.complete_pull(id,resp)
        rec=self.images.find_one({'_id':id})
        assert rec is not None
        assert rec['tag']==[self.tag]
        assert rec['last_pull']>0
        # Create an identical request and
        # run complete again
        id2=self.images.insert(record.copy())
        assert id2 is not None
        self.m.complete_pull(id2,resp)
        # confirm that the record was removed
        rec2=self.images.find_one({'_id':id2})
        assert rec2 is None

    def test_0update_states(self):
        # Test a repull
        record=self.good_record()
        record['last_pull']=0
        record['status']='FAILURE'
        # Create a fake record in mongo
        id=self.images.insert(record)
        assert id is not None
        self.m.update_states()
        rec=self.images.find_one({'_id':id})
        assert rec is None


    def test_lookup(self):
        record=self.good_record()
        # Create a fake record in mongo
        self.images.insert(record)
        i=self.query.copy()
        session=self.m.new_session(self.auth,self.system)
        l=self.m.lookup(session,i)
        assert 'status' in l
        assert '_id' in l
        assert self.m.get_state(l['_id'])=='READY'
        i=self.query.copy()
        r=self.images.find_one({'_id':l['_id']})
        assert 'expiration' in r
        assert r['expiration']>time.time()
        i['tag']='bogus'
        l=self.m.lookup(session,i)
        assert l==None

    def test_list(self):
        record=self.good_record()
        # Create a fake record in mongo
        id1=self.images.insert(record.copy())
        # rec2 is a failed pull, it shouldn't be listed
        rec2=record.copy()
        rec2['status']='FAILURE'
        id2=self.images.insert(rec2)
        session=self.m.new_session(self.auth,self.system)
        li=self.m.list(session,self.system)
        assert len(li)==1
        l=li[0]
        assert '_id' in l
        assert self.m.get_state(l['_id'])=='READY'
        assert l['_id']==id1

    def test_repull(self):
        # Test a repull
        record=self.good_record()

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
        assert state=='READY'
        imagerec=self.m.lookup(session,pr)
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
        assert 'ENTRY' in imagerec
        assert 'ENV' in imagerec
        # Cause a failure
        self.images.drop()
        rec=self.m.pull(session,pr,TESTMODE=2)
        time.sleep(10)
        assert rec is not None
        id=rec['_id']
        state=self.m.get_state(id)
        assert state=='FAILURE'
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
        assert '_id' in mrec
        state=self.time_wait(id1)
        assert state=='READY'
        state=self.time_wait(id2)
        assert state=='READY'
        # Cause a failure
        mrec=self.images.find_one(q)
        self.images.drop()
        self.stop_worker()


    def test_expire_remote(self):
        system=self.system
        record=self.good_record()
        # Create a fake record in mongo
        self.start_worker()
        id=self.images.insert(record)
        assert id is not None
        # Create a bogus image file
        file,metafile=self.create_fakeimage(system,record['id'],self.format)
        session=self.m.new_session(self.authadmin,system)
        er={'system':system,'tag':self.tag,'itype':self.itype}
        rec=self.m.expire(session,er,TESTMODE=1)#,delay=False)
        assert rec is not None
        time.sleep(2)
        state=self.m.get_state(id)
        assert state=='EXPIRED'
        assert os.path.exists(file) is False
        assert os.path.exists(metafile) is False

    def test_expire_local(self):
        record=self.good_record()
        system='systemb'
        record['system']=system
        # Create a fake record in mongo
        self.start_worker(system=system)
        id=self.images.insert(record)
        assert id is not None
        # Create a bogus image file
        file,metafile=self.create_fakeimage(system,record['id'],self.format)
        session=self.m.new_session(self.authadmin,system)
        er={'system':system,'tag':self.tag,'itype':self.itype}
        rec=self.m.expire(session,er)#,delay=False)
        assert rec is not None
        time.sleep(2)
        state=self.m.get_state(id)
        assert state=='EXPIRED'
        assert os.path.exists(file) is False
        assert os.path.exists(metafile) is False


    def test_expire_noadmin(self):
        record=self.good_record()
        # Create a fake record in mongo
        self.start_worker()
        id=self.images.insert(record)
        assert id is not None
        # Create a bogus image file
        file,metafile=self.create_fakeimage(self.system,record['id'],self.format)
        session=self.m.new_session(self.auth,self.system)
        er={'system':self.system,'tag':self.tag,'itype':self.itype}
        rec=self.m.expire(session,er,TESTMODE=1)#,delay=False)
        assert rec is not None
        time.sleep(2)
        state=self.m.get_state(id)
        assert state=='READY'
        assert os.path.exists(file) is True
        assert os.path.exists(metafile) is True

    def test_autoexpire_stuckpull(self):
        record=self.good_pullrecord()
        record['status']='ENQUEUED'
        record['last_pull']=time.time()-3000
        id=self.images.insert(record)
        assert id is not None
        session=self.m.new_session(self.auth,self.system)
        self.m.autoexpire(session,self.system,TESTMODE=1)
        state=self.m.get_state(id)
        assert state is None

    def test_autoexpire_recentpull(self):
        record=self.good_pullrecord()
        record['status']='ENQUEUED'
        id=self.images.insert(record)
        assert id is not None
        session=self.m.new_session(self.auth,self.system)
        self.m.autoexpire(session,self.system,TESTMODE=1)
        state=self.m.get_state(id)
        assert state=='ENQUEUED'

    def test_autoexpire(self):
        record=self.good_record()

        # Make it a candidate for expiration (10 secs too old)
        record['expiration']=time.time()-10
        self.start_worker()
        id=self.images.insert(record)
        assert id is not None
        # Create a bogus image file
        file,metafile=self.create_fakeimage(self.system,record['id'],self.format)
        session=self.m.new_session(self.auth,self.system)
        self.m.autoexpire(session,self.system,TESTMODE=1)#,delay=False)
        time.sleep(2)
        state=self.m.get_state(id)
        assert state=='EXPIRED'
        assert os.path.exists(file) is False
        assert os.path.exists(metafile) is False

    def test_autoexpire_dontexpire(self):
        # A new image shouldn't expire
        record=self.good_record()
        record['expiration']=time.time()+1000
        self.start_worker()
        id=self.images.insert(record)
        assert id is not None
        # Create a bogus image file
        file,metafile=self.create_fakeimage(self.system,record['id'],self.format)
        session=self.m.new_session(self.auth,self.system)
        self.m.autoexpire(session,self.system,TESTMODE=1)#,delay=False)
        time.sleep(2)
        state=self.m.get_state(id)
        assert state=='READY'
        assert os.path.exists(file) is True
        assert os.path.exists(metafile) is True

    def test_autoexpire_othersystem(self):
        # A new image shouldn't expire
        record=self.good_record()
        record['expiration']=time.time()-10
        record['system']='other'
        self.start_worker()
        id=self.images.insert(record)
        assert id is not None
        # Create a bogus image file
        file,metafile=self.create_fakeimage(self.system,record['id'],self.format)
        session=self.m.new_session(self.auth,self.system)
        self.m.autoexpire(session,self.system,TESTMODE=1)#,delay=False)
        time.sleep(2)
        state=self.m.get_state(id)
        assert state=='READY'
        assert os.path.exists(file) is True
        assert os.path.exists(metafile) is True


if __name__ == '__main__':
    unittest.main()
